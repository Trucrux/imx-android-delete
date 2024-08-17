/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "IsiFilter"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>

#include <utils/Log.h>
#include "IsiFilter.h"
#include <IMXC2Interface.h>
#include "graphics_ext.h"
#include "Memory.h"
#include <C2PlatformSupport.h>
#include <C2Buffer.h>
#include <C2Work.h>
#include <cutils/properties.h>
#include "IMXUtils.h"
#include "Codec2BufferUtils.h"


#define ISI_FILTER_DEBUG_FLAG "vendor.media.nxp.filter.isi.debug"
#define ISI_FILTER_DEBUG_DUMP_INPUT 0x1
#define ISI_FILTER_DEBUG_DUMP_OUTPUT 0x2

#define ISI_FILTER_INPUT_FILE "/data/isi_filter_in.yuv"
#define ISI_FILTER_OUTPUT_FILE "/data/isi_filter_out.yuv"


namespace android {

static const uint32_t source_format_table[]={
    HAL_PIXEL_FORMAT_RGB_565,
    HAL_PIXEL_FORMAT_RGB_888,
    HAL_PIXEL_FORMAT_RGBA_8888,
    HAL_PIXEL_FORMAT_RGBX_8888,
    HAL_PIXEL_FORMAT_BGRA_8888,
};
static const uint32_t destination_format_table[]={
    HAL_PIXEL_FORMAT_YCbCr_420_SP,
};
#define Align(ptr,align)    (((uint32_t)(ptr)+(align)-1)/(align)*(align))


IsiFilter::IsiFilter(c2_node_id_t id, C2String name, const std::shared_ptr<C2ReflectorHelper>& helper, const std::shared_ptr<IsiFilterInterface> &intfImpl)
    : IMXC2ComponentBase(std::make_shared<IMXInterface<IsiFilterInterface>>(name, id, intfImpl)),
    mIntfImpl(intfImpl)
{
    (void)helper;
}
IsiFilter::~IsiFilter(){
    ALOGV("%s", __FUNCTION__);
    onRelease();
}

c2_status_t IsiFilter::onInit() {
    ALOGV("%s", __FUNCTION__);

    mDebugFlag = property_get_int32(ISI_FILTER_DEBUG_FLAG, 0);

    mInMemType = V4L2_MEMORY_DMABUF;
    mOutMemType = V4L2_MEMORY_DMABUF;

    return C2_OK;
}



void IsiFilter::onStart()
{
    ALOGV("onStart");
    mSourceFormat = mIntfImpl->getPixelFormat();
    mTargetFormat = mSourceFormat;

    mWidth = 0;
    mHeight = 0;
    mSrcBufferSize = 0;
    mTarBufferSize = 0;

    mIsiOutNum = 0;
    mInNum = 0;
    mOutNum = 0;
    mPassthrough = true;
    mStart = false;
    mEOS = false;

    mBitrate = mIntfImpl->getBitrate_l();
    mRequestSync = mIntfImpl->getRequestSync_l();
    mSendConfig = true;
    mConfigID = 0;
    mSyncFrame = false;
    mBitrateChange = false;
    ALOGV("onStart mSourceFormat=0x%x, bitrate=%d",mSourceFormat, mBitrate->value);

}
c2_status_t IsiFilter::onStop()
{
    ALOGV("onStop");
    if(mPassthrough)
        return C2_OK;

    onFlush_sm();

    destroyInputBuffers();
    destroyOutputBuffers();

    return C2_OK;
}
c2_status_t IsiFilter::onFlush_sm(){
    ALOGV("onFlush_sm");
    if(mPassthrough)
        return C2_OK;

    if(mPoller)
        mPoller->stopPolling();

    if(mFetcher)
        mFetcher->Stop();

    stopInputStream();
    stopOutputStream();

    mInNum = 0;
    mOutNum = 0;
    mIsiOutNum = 0;

    return C2_OK;
}
c2_status_t IsiFilter::drainInternal(uint32_t drainMode){
    ALOGV("drainInternal");
    (void)drainMode;

    return C2_OK;
}
void IsiFilter::onReset(){
    ALOGV("onReset");
}
void IsiFilter::onRelease() {

    if(mPassthrough)
        return;

    if(mOutNum > 0)
        onStop();

    if(mFetcher){
        mFetcher.clear();
    }

    if(mPoller){
        mPoller.clear();
    }

    if(mFd > 0){
        mDev->Close();
        mFd = 0;
    }

    if(mDev != nullptr)
        delete mDev;
    mDev = nullptr;

    ALOGV("%s END", __FUNCTION__);
    return;
}
void IsiFilter::processWork(const std::unique_ptr<C2Work> &work){

    std::shared_ptr<C2Buffer> buffer = nullptr;
    status_t ret = OK;
    int32_t inputId = static_cast<int32_t>(work->input.ordinal.frameIndex.peeku() & 0x3FFFFFFF);
    work->result = C2_OK;
    work->workletsProcessed = 0u;

    ALOGV("processWork inputId=%d ts=%lld, flag=%x",inputId, (long long)work->input.ordinal.timestamp.peeku(), work->input.flags);

    if (work->input.buffers.empty())
        return handleEmptyBuffer(work);

    do{
        if(!mStart){
            ret = updateParam(work);
            if(ret)
                break;

            if(mPassthrough)
                mStart = true;
        }

        if(mPassthrough){
            return handlePassThroughBuffer(work);
        }

        if(!mStart){
            ret = startInternal();
            if(ret)
                break;

            mStart = true;
        }

        ret = queueInput(work);

        if(ret){
            ALOGE("queueInput failed ret=%d",ret);
            break;
        }

        for(int i = 0; i < kOutputBufferNum; ++i){
            ret = queueOutput();
            if(ret){
                ALOGE("queueOutput failed ret=%d",ret);
                break;
            }
        }

    }while(0);

    if(ret){
        work->result = C2_CORRUPTED;
        work->workletsProcessed = 1u;
        ALOGE("processWork failed, ret=%d",ret);
        return;
    }

    work->workletsProcessed = 0;
    work->result = C2_OK;
    return;
}
c2_status_t IsiFilter::canProcessWork(){

    if(mPassthrough)
        return C2_OK;

    if(allInputBuffersInIsi())
        return C2_BAD_VALUE;

    if(mFreeOutputIndex.empty())
        return C2_BAD_VALUE;

    return C2_OK;
}
void IsiFilter::handleEmptyBuffer(const std::unique_ptr<C2Work> &work){

    //set eos flag, and send eos frame after last capture buffer dequeued.
    if(!mPassthrough && mStart && (work->input.flags & C2FrameData::FLAG_END_OF_STREAM)){
        mEOS = true;
        ALOGV("set eos");
        return;
    }else{
        // copy work
        work->worklets.front()->output.flags = work->input.flags;
        work->worklets.front()->output.ordinal = work->input.ordinal;
        work->worklets.front()->output.infoBuffers = work->input.infoBuffers;

        work->workletsProcessed = 1;
        work->result = C2_OK;
        ALOGV("processWork copy empty buffer");
        return;
    }
}
void IsiFilter::handlePassThroughBuffer(const std::unique_ptr<C2Work> &work){

    if(!mPassthrough)
        return;

    std::shared_ptr<C2Buffer> buffer = work->input.buffers.front();

    if(!buffer)
        return;

    int32_t inputId = static_cast<int32_t>(work->input.ordinal.frameIndex.peeku() & 0x3FFFFFFF);
    getDynamicConfigParam(inputId);

    if(mSendConfig || inputId == mConfigID){
        C2StreamRequestSyncFrameTuning::output request_sync(0u, C2_FALSE);
        C2StreamBitrateInfo::output bitrate(0u, 64000);

        if(mSyncFrame){
            request_sync = *mRequestSync;
            mSyncFrame = false;
        }
        bitrate = *mBitrate;

        work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(request_sync));
        work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(bitrate));

        ALOGD("add updateConfig 2 request_sync=%d,bitrate=%d, mConfigID=%d", (int)request_sync.value, bitrate.value, mConfigID);
        mConfigID = -1;
        mSendConfig = false;
    }

    work->worklets.front()->output.buffers.push_back(std::move(buffer));
    //copy ts & flag
    work->worklets.front()->output.flags = work->input.flags;
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->worklets.front()->output.infoBuffers = work->input.infoBuffers;

    work->workletsProcessed = 1;
    work->result = C2_OK;
    return;
}
status_t IsiFilter::updateParam(const std::unique_ptr<C2Work> &work){

    std::shared_ptr<C2Buffer> buffer = work->input.buffers.front();

    if(!buffer)
        return BAD_TYPE;

    std::shared_ptr<const C2GraphicView> view = 
        std::make_shared<const C2GraphicView>(buffer->data().graphicBlocks().front().map().get());
    if (view->error() != C2_OK) {
        ALOGE("graphic view map err = %d", view->error());
        return BAD_TYPE;
    }

    const C2GraphicView *const rawBuf = view.get();
    if(IsNV12(*rawBuf))
        mSourceFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    else if(IsI420(*rawBuf))
        mSourceFormat = HAL_PIXEL_FORMAT_YCbCr_420_P;

    const C2ConstGraphicBlock block = buffer->data().graphicBlocks().front();
    fsl::Memory *prvHandle = (fsl::Memory*)block.handle();

    if (mSourceFormat == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED){
        mSourceFormat = static_cast<uint32_t>(prvHandle->format);
        ALOGV("prvHandle->format=0x%x, fsl_format=%x, w=%d,h=%d, stride=%d, fd=%d",
            prvHandle->format, prvHandle->fslFormat, prvHandle->width, prvHandle->height, prvHandle->stride, prvHandle->fd);
    }

    mWidth = block.width();
    mHeight = block.height();


    if(OK == verifyFormat(true, mSourceFormat)){
        mPassthrough = false;
    }

    ALOGI("[%p]updateParam mSourceFormat=0x%x, mPassthrough=%d, mWidth=%d, mHeight=%d",
        this, mSourceFormat, mPassthrough, mWidth, mHeight);

    return OK;
}
status_t IsiFilter::verifyFormat(bool source, uint32_t format)
{
    if(source){
        for(auto fmt : source_format_table){
            if(fmt == format){
                return OK;
            }
        }
    }else{
        for(auto fmt : destination_format_table){
            if(fmt == format){
                return OK;
            }
        }
    }

    return NAME_NOT_FOUND;
}

status_t IsiFilter::startInternal(){

    status_t ret = UNKNOWN_ERROR;

    if(mDev == nullptr){
        mDev = new V4l2Dev();
    }
    if(mDev == nullptr)
        return ret;

    mFd = mDev->Open(V4L2_DEV_ISI);

    ret = prepareInputParams();
    if(ret != OK){
        ALOGE("prepareInputParams failed");
        return ret;
    }

    ret = setInputFormats();
    if(ret != OK){
        ALOGE("SetInputFormats failed");
        return ret;
    }

    ret = prepareOutputParams();
    if(ret != OK){
        ALOGE("SetInputFormats failed");
        return ret;
    }

    ret = setOutputFormats();
    if(ret != OK){
        ALOGE("SetOutputFormats failed");
        return ret;
    }

    ret = prepareInputBuffers();
    if(ret != OK){
        ALOGE("prepareInputBuffers failed");
        return ret;
    }

    ret = prepareOutputBuffers();
    if(ret != OK){
        ALOGE("prepareInputBuffers failed");
        return ret;
    }

    ret = createFrameFetcher();
    if(ret != OK){
        ALOGE("createFrameFetcher failed");
        return ret;
    }

    ret = createPoller();
    if(ret != OK){
        ALOGE("createPoller failed");
        return ret;
    }

    ALOGV("startInternal ret=%d",ret);

    return ret;
}

typedef struct{
    uint32_t v4l2_format;
    int pixel_format;
}V4L2_FORMAT_TABLE;

static V4L2_FORMAT_TABLE color_format_table[]={
{V4L2_PIX_FMT_RGBA32, HAL_PIXEL_FORMAT_RGBA_8888},
{V4L2_PIX_FMT_RGBA32, HAL_PIXEL_FORMAT_RGBX_8888},
{V4L2_PIX_FMT_RGB565, HAL_PIXEL_FORMAT_RGB_565},
{V4L2_PIX_FMT_NV12, HAL_PIXEL_FORMAT_YCbCr_420_SP},
};

uint32_t IsiFilter::getV4l2Format(uint32_t color_format)
{
    uint32_t i=0;

    for(i = 0; i < sizeof(color_format_table)/sizeof(V4L2_FORMAT_TABLE);i++){
        if(color_format == color_format_table[i].pixel_format){
            return color_format_table[i].v4l2_format;
        }
    }

    return 0;
}

status_t IsiFilter::prepareInputParams()
{
    mWidthAlign = 1;
    mHeightAlign = 1;

    struct v4l2_frmsizeenum info;
    memset(&info, 0, sizeof(v4l2_frmsizeenum));
    if(OK == mDev->GetFormatFrameInfo(mOutFormat, &info)){
        mWidthAlign = info.stepwise.step_width;
        mHeightAlign = info.stepwise.step_height;
    }
    //align with v4l2 encoder
    mWidthAlign = 16;

    mWidthWithAlign = Align(mWidth, mWidthAlign);
    mHeightWithAlign = Align(mHeight, mHeightAlign);

    mInFormat = getV4l2Format(mSourceFormat);

    ALOGV("prepareInputParams width=%d,height=%d,format=%x",mWidthWithAlign,mHeightWithAlign, mInFormat);

    mSrcBufferSize = mWidthWithAlign * mHeightWithAlign * pxlfmt2bpp(mSourceFormat) / 8;

    return OK;
}
status_t IsiFilter::prepareOutputParams()
{
    status_t ret = OK;

    mTargetFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;

    ret = verifyFormat(false, mTargetFormat);
    if(ret)
        return ret;

    mOutFormat = getV4l2Format(mTargetFormat);
    mTarBufferSize = mWidthWithAlign * mHeightWithAlign * pxlfmt2bpp(mTargetFormat) / 8;

    if(mOutFormat == V4L2_PIX_FMT_NV12){
        mOutputPlaneSize[0] = mTarBufferSize;
    }else
        return UNKNOWN_ERROR;

    ALOGI("mTargetFormat=0x%x mPassthrough=%d, mOutputPlaneSize[0]=%d, ",
        mTargetFormat, mPassthrough, mOutputPlaneSize[0]);

    return OK;
}

status_t IsiFilter::setInputFormats()
{
    int result = 0;
    Mutex::Autolock autoLock(mIsiLock);

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));

    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.num_planes = kInputBufferPlaneNum;
    format.fmt.pix_mp.pixelformat = mInFormat;
    format.fmt.pix_mp.width = mWidthWithAlign;
    format.fmt.pix_mp.height = mHeightWithAlign;
    format.fmt.pix_mp.plane_fmt[0].bytesperline = mWidthWithAlign;
    format.fmt.pix_mp.field = V4L2_FIELD_NONE;

    result = ioctl (mFd, VIDIOC_S_FMT, &format);
    if(result != 0){
        ALOGE("IsiFilter VIDIOC_S_FMT OUTPUT_MPLANE failed");
        return UNKNOWN_ERROR;
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result < 0)
        return UNKNOWN_ERROR;


    if(format.fmt.pix_mp.pixelformat != mInFormat){
        ALOGE("SetInputFormats mOutFormat mismatch");
        return UNKNOWN_ERROR;
    }

    if(format.fmt.pix_mp.width != mWidthWithAlign ||
        format.fmt.pix_mp.height != mHeightWithAlign){
        ALOGE("SetInputFormats resolution mismatch");
        return UNKNOWN_ERROR;
    }

    if(format.fmt.pix_mp.plane_fmt[0].sizeimage != mSrcBufferSize){
        ALOGE("SetInputFormats bufferSize mismatch sizeimage=%d,buffersize=%d", format.fmt.pix_mp.plane_fmt[0].sizeimage, mSrcBufferSize);
        return UNKNOWN_ERROR;
    }

    ALOGV("SetInputFormats success mInFormat=%x,width=%d,height=%d",mInFormat, mWidthWithAlign, mHeightWithAlign);
    return OK;
}
status_t IsiFilter::setOutputFormats()
{
    int result = 0;
    Mutex::Autolock autoLock(mIsiLock);

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    format.fmt.pix_mp.num_planes = kOutputBufferPlaneNum;
    format.fmt.pix_mp.pixelformat = mOutFormat;
    format.fmt.pix_mp.plane_fmt[0].bytesperline = mWidthWithAlign;

    format.fmt.pix_mp.width = mWidthWithAlign;
    format.fmt.pix_mp.height = mHeightWithAlign;
    format.fmt.pix_mp.field = V4L2_FIELD_NONE;
    ALOGV("SetOutputFormats mOutFormat=%x,w=%d,h=%d,stride=%d,size0=%d,",
        mOutFormat,mWidthWithAlign,mHeightWithAlign,mWidthWithAlign,mOutputPlaneSize[0]);
    result = ioctl (mFd, VIDIOC_S_FMT, &format);
    if(result != 0){
        ALOGE("IsiFilter VIDIOC_S_FMT CAPTURE_MPLANE failed err=%d",result);
        return UNKNOWN_ERROR;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result != 0)
        return UNKNOWN_ERROR;

    if(format.fmt.pix_mp.pixelformat != mOutFormat){
        ALOGE("SetOutputFormats mInFormat mismatch");
        return UNKNOWN_ERROR;
    }

    if( format.fmt.pix_mp.width != mWidthWithAlign ||
        format.fmt.pix_mp.height != mHeightWithAlign){
        ALOGE("SetOutputFormats resolution mismatch");
        return UNKNOWN_ERROR;
    }

    if(format.fmt.pix_mp.plane_fmt[0].bytesperline != mWidthWithAlign){
        ALOGE("SetOutputFormats stride mismatch");
        return UNKNOWN_ERROR;
    }

    if(format.fmt.pix_mp.plane_fmt[0].sizeimage > mOutputPlaneSize[0]){
        ALOGE("SetOutputFormats bufferSize mismatch sizeimage=%d", format.fmt.pix_mp.plane_fmt[0].sizeimage);
        return UNKNOWN_ERROR;
    }

    return OK;
}
IsiFilter::InputRecord::InputRecord()
    : at_device(false), input_id(-1), ts(-1) {
    memset(&plane, 0, sizeof(VideoFramePlane));
}

IsiFilter::InputRecord::~InputRecord() {}

status_t IsiFilter::prepareInputBuffers()
{
    int result = 0;
    Mutex::Autolock autoLock(mIsiLock);

    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = kInputBufferNum;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = mInMemType;
    ALOGV("prepareInputBuffers VIDIOC_REQBUFS bufferNum=%d",reqbufs.count);
    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGE("VIDIOC_REQBUFS failed result=%d",result);
        return UNKNOWN_ERROR;
    }
    ALOGV("prepareInputBuffers reqbufs.count=%d",reqbufs.count);

    mInputBufferMap.resize(reqbufs.count);

    ALOGV("prepareInputBuffers total input=%d size=%d",reqbufs.count, (int)mInputBufferMap.size());

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        mInputBufferMap[i].at_device = false;
        mInputBufferMap[i].input_id = -1;
        mInputBufferMap[i].plane.fd = -1;
        mFreeInputIndex.push(i);
    }

    return OK;
}
status_t IsiFilter::prepareOutputBuffers()
{
    int result = 0;
    Mutex::Autolock autoLock(mIsiLock);

    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = kOutputBufferNum;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = mOutMemType;
    ALOGV("prepareOutputBuffers VIDIOC_REQBUFS bufferNum=%d",reqbufs.count);
    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGE("VIDIOC_REQBUFS failed result=%d",result);
        return UNKNOWN_ERROR;
    }

    ALOGV("prepareOutputBuffers reqbufs.count=%d",reqbufs.count);

    mOutputBufferMap.resize(reqbufs.count);

    for (size_t i = 0; i < mOutputBufferMap.size(); i++) {
        mOutputBufferMap[i] = nullptr;
        mFreeOutputIndex.push(i);
    }

    return OK;
}
status_t IsiFilter::createFrameFetcher()
{
    status_t ret = OK;
    if (!mBlockPool) {
        c2_status_t err = C2_OK;
        C2BlockPool::local_id_t poolId = C2BlockPool::BASIC_GRAPHIC;
        err = GetCodec2BlockPool(poolId, shared_from_this(), &mBlockPool);
        if (err == C2_OK && mBlockPool->getLocalId() == C2BlockPool::BASIC_GRAPHIC) {
            // can't C2BasicGraphicBlockPool because it doesn't have a pool manager
            err = CreateCodec2BlockPool(C2PlatformAllocatorStore::GRALLOC, shared_from_this(), &mBlockPool);
            if (err) {
                ALOGE("CreateCodec2BlockPool GRALLOC failed, err %d", err);
                return UNKNOWN_ERROR;
            }
        }
        ALOGD("Using filter block pool with poolID %llu => got %llu",
                (unsigned long long)poolId,
                (unsigned long long)(mBlockPool ? mBlockPool->getLocalId() : 111000111));

    }

    if(mFetcher != nullptr){
        if(mFetcher->Running()){
            ret = mFetcher->Stop();
        }
    }else{
        mFetcher = new FrameFetcher();
        ret = mFetcher->Init(mBlockPool);

        if(mFetcher == nullptr){
            ret = UNKNOWN_ERROR;
            return ret;
        }
    }

    ret = mFetcher->SetParam(mWidthWithAlign, mHeightWithAlign, mTargetFormat, C2AndroidMemoryUsage::HW_TEXTURE_READ);
    if(ret)
        return ret;

    ret = mFetcher->Start();

    return ret;
}
status_t IsiFilter::createPoller()
{
    status_t ret = OK;

    if (mPoller == NULL) {
        mPoller = new V4l2Poller(mDev, this);
        mPoller->initPoller(PollThreadWrapper);
    }

    return ret;
}
status_t IsiFilter::PollThreadWrapper(void *me, status_t poll_ret) {
    return static_cast<IsiFilter *>(me)->HandlePollThread(poll_ret);
}
status_t IsiFilter::HandlePollThread(status_t poll_ret){
    status_t ret = OK;

    ALOGV("pollThreadHandler poll_ret=%x",poll_ret);

    if (poll_ret & V4L2_DEV_POLL_OUTPUT) {
        dequeueInputBuffer();
    }
    if (poll_ret & V4L2_DEV_POLL_CAPTURE) {
        dequeueOutputBuffer();
    }

    if(mEOS && mInNum == mOutNum){
        ALOGV("mEOS finishWithException");
        (void)finishWithException(true/*eos*/, true);
        mEOS = false;
    }

    return ret;
}
status_t IsiFilter::getDynamicConfigParam(int32_t index){

    std::shared_ptr<C2StreamIntraRefreshTuning::output> intraRefresh = mIntfImpl->getIntraRefresh_l();
    std::shared_ptr<C2StreamBitrateInfo::output> bitrate = mIntfImpl->getBitrate_l();
    std::shared_ptr<C2StreamRequestSyncFrameTuning::output> requestSync = mIntfImpl->getRequestSync_l();

    if(bitrate != mBitrate) {
        mBitrate = bitrate;
        mConfigID = index;
        mBitrateChange = true;
        ALOGV("mBitrateChange true");
    }

    if(requestSync->value == 1){
        mRequestSync = requestSync;
        mConfigID = index;
        mSyncFrame = true;
        ALOGV("mSyncFrame true");
        C2StreamRequestSyncFrameTuning::output clearSync(0u, C2_FALSE);
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        mIntfImpl->config({ &clearSync }, C2_MAY_BLOCK, &failures);
    }

    (void)index;
    return OK;
}
status_t IsiFilter::queueInput(const std::unique_ptr<C2Work> &work)
{

    int result = 0;
    int32_t index = -1;
    uint32_t flags = work->input.flags;

    const C2ConstGraphicBlock block = work->input.buffers[0]->data().graphicBlocks().front();
    int inFd = block.handle()->data[0];

    int32_t inputId = static_cast<int32_t>(work->input.ordinal.frameIndex.peeku() & 0x3FFFFFFF);
    uint64_t timestamp = work->input.ordinal.timestamp.peeku();
    uint32_t v4l2_flags = (V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TIMESTAMP_COPY);

    getDynamicConfigParam(inputId);

    dumpInputBuffer(inFd, mSrcBufferSize);

    if(mFreeInputIndex.empty()){
        ALOGE("queueInput mFreeInputIndex empty");
        return INVALID_OPERATION;
    }

    index = mFreeInputIndex.front();
    if(index >= kInputBufferNum){
        ALOGE("[%p]queueInput, invalid index",this);
        return INVALID_OPERATION;
    }

    //TODO: handle eos, sync frame flag, timestamps
    if(flags & C2FrameData::FLAG_END_OF_STREAM && inFd < 0){
        return BAD_VALUE;
    }

    if(flags & C2FrameData::FLAG_END_OF_STREAM)
        v4l2_flags |= V4L2_BUF_FLAG_LAST;

    //(mSyncFrame/*flag & FLAG_SYNC_FRAME*/){
     // v4l2_flags |= V4L2_BUF_FLAG_KEYFRAME;
    //  mSyncFrame = false;
    //

    mIsiLock.lock();
    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane plane[kInputBufferPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(&plane[0], 0, kInputBufferPlaneNum * sizeof(struct v4l2_plane));

    plane[0].bytesused = mSrcBufferSize;
    plane[0].length = mSrcBufferSize;
    plane[0].data_offset = 0;

    if(mInMemType == V4L2_MEMORY_DMABUF){
        plane[0].m.fd = inFd;
    }

    stV4lBuf.index = index;
    stV4lBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    stV4lBuf.memory = mInMemType;
    stV4lBuf.m.planes = &plane[0];
    stV4lBuf.length = kInputBufferPlaneNum;
    stV4lBuf.flags = v4l2_flags;
    stV4lBuf.timestamp.tv_sec = inputId;

    ALOGV("OUTPUT_MPLANE VIDIOC_QBUF index=%d,len=%d,fd=%d, ts=%lld, v4l2_flags=%x\n",
        stV4lBuf.index, plane[0].bytesused, plane[0].m.fd, (long long)0, v4l2_flags);

    result = ioctl(mFd, VIDIOC_QBUF, &stV4lBuf);
    if(result < 0){
        ALOGE("OUTPUT_MPLANE VIDIOC_QBUF failed, index=%d, result=%x",index,result);
        mIsiLock.unlock();
        return UNKNOWN_ERROR;
    }

    mInputBufferMap[index].input_id = inputId;
    mInputBufferMap[index].ts = timestamp;
    mInputBufferMap[index].at_device = true;
    mInNum ++;

    mInputQueue.push(inputId);
    mFreeInputIndex.pop();

    mIsiLock.unlock();

    if(!mInputStreamOn)
        startInputStream();

    if(!mPoller->isPolling()) {
        mPoller->startPolling();
    }
    ALOGV("queueInput success mInNum=%d",mInNum);

    return OK;
}
status_t IsiFilter::queueOutput()
{
    status_t ret = OK;
    ALOGV("queueOutput BEGIN");

    if(mFreeOutputIndex.empty())
        return OK;

    if(!mFetcher->Running())
        mFetcher->Start();

    while(true){
        ret = mFetcher->HasFrames();
        if(ret == OK)
            break;
        else if(ret == TIMED_OUT)
            usleep(3000);
        else
            break;
    }

    std::shared_ptr<C2GraphicBlock> block;
    ret = mFetcher->GetFrameBlock(&block);
    if(ret){
        ALOGE("GetFrameBlock failed");
        return UNKNOWN_ERROR;
    }

    int fd = block->handle()->data[0];

    int32_t index = mFreeOutputIndex.front();

    if(index >= kOutputBufferNum)
        return UNKNOWN_ERROR;

    mIsiLock.lock();

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[kOutputBufferPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(&planes[0], 0, kOutputBufferPlaneNum * sizeof(struct v4l2_plane));

    planes[0].bytesused = mOutputPlaneSize[0];
    planes[0].length = mOutputPlaneSize[0];
    planes[0].data_offset = 0;

    if(mOutMemType == V4L2_MEMORY_DMABUF){
        planes[0].m.fd = fd;
    }

    ALOGV("queueOutput index=%d, fd=%d, size[0]=%d",index, fd, mOutputPlaneSize[0]);

    stV4lBuf.index = index;
    stV4lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    stV4lBuf.memory = mOutMemType;
    stV4lBuf.m.planes = &planes[0];
    stV4lBuf.length = kOutputBufferPlaneNum;
    stV4lBuf.flags = 0;
    //do not send timestamp

    int result = ioctl(mFd, VIDIOC_QBUF, &stV4lBuf);
    if(result < 0){
        ALOGE("CAPTURE_MPLANE VIDIOC_QBUF failed, index=%d, result=%x",index,result);
        mIsiLock.unlock();
        return UNKNOWN_ERROR;
    }

    mIsiOutNum++;
    mFreeOutputIndex.pop();
    mOutputQueue.push(index);
    mOutputBufferMap[index] = std::move(block);
    mIsiLock.unlock();

    if(!mOutputStreamOn && mIsiOutNum == kOutputBufferNum)
        startOutputStream();

    ALOGV("queueOutput success, mIsiOutNum=%d",mIsiOutNum);
    return ret;
}

status_t IsiFilter::dequeueInputBuffer()
{
    int result = 0;
    int input_id = -1;
    uint32_t index = 0;

    {
        Mutex::Autolock autoLock(mIsiLock);
        if(!mInputStreamOn)
            return OK;

        struct v4l2_buffer stV4lBuf;
        struct v4l2_plane plane[kInputBufferPlaneNum];
        memset(&stV4lBuf, 0, sizeof(stV4lBuf));
        memset(plane, 0, sizeof(v4l2_plane));
        stV4lBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        stV4lBuf.memory = mInMemType;
        stV4lBuf.m.planes = plane;
        stV4lBuf.length = kInputBufferPlaneNum;
        result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
        if(result < 0)
            return UNKNOWN_ERROR;

        ALOGV("OUTPUT_MPLANE VIDIOC_DQBUF index=%d\n", stV4lBuf.index);

        if(stV4lBuf.index >= kInputBufferNum)
            return BAD_INDEX;


        index = stV4lBuf.index;

        if(mInputBufferMap[index].at_device){
            mInputBufferMap[index].at_device = false;
            mFreeInputIndex.push(index);
            input_id = mInputBufferMap[index].input_id;
            ALOGV("VIDIOC_DQBUF input_id=%d",input_id);
        }else
            ALOGE("mInputBufferMap[index].at_device FALSE");
    }

    if(input_id >= 0)
        (void)postClearInputMsg(input_id);

    return OK;
}
status_t IsiFilter::dequeueOutputBuffer()
{
    int result = 0;
    status_t ret = OK;
    //int keyFrame = 0;
    uint32_t index = 0;
    uint64_t timestamp = static_cast<uint64_t>(-1);
    bool updateConfig = false;
    {
        Mutex::Autolock autoLock(mIsiLock);
        if(!mOutputStreamOn)
            return OK;

        struct v4l2_buffer stV4lBuf;
        struct v4l2_plane planes[kOutputBufferPlaneNum];
        memset(&stV4lBuf, 0, sizeof(stV4lBuf));
        memset(planes, 0, sizeof(planes));
        stV4lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        stV4lBuf.memory = mOutMemType;
        stV4lBuf.m.planes = planes;
        stV4lBuf.length = kOutputBufferPlaneNum;
        result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
        if(result < 0)
            return UNKNOWN_ERROR;

        if(stV4lBuf.index >= kOutputBufferNum)
            return BAD_INDEX;

        index = stV4lBuf.index;
        //keyFrame = (stV4lBuf.flags & V4L2_BUF_FLAG_KEYFRAME)?1:0;

        mOutNum ++;
        mOutputQueue.pop();
        mFreeOutputIndex.push(index);

        if(mInputQueue.empty())
            return OK;

        int32_t input_id = mInputQueue.front();
        mInputQueue.pop();

        for(auto record : mInputBufferMap){
            if(record.input_id == input_id){
                timestamp = static_cast<uint64_t>(record.ts);
                if(mConfigID == record.input_id || mSendConfig){
                    updateConfig = true;
                    mConfigID = -1;
                    mSendConfig = false;
                }
                break;
            }
        }

        ALOGV("CAPTURE_MPLANE VIDIOC_DQBUF index=%d, len=%d,ts=%lld,flag=%x,input_id=%d, mOutNum=%d",
            stV4lBuf.index, stV4lBuf.m.planes[0].bytesused, (long long)timestamp, stV4lBuf.flags, input_id, mOutNum);
    }

    ret = handleOutputFrame(index, timestamp, updateConfig);
    return ret;
}
status_t IsiFilter::handleOutputFrame(uint32_t index, uint64_t timestamp, bool updateConfig){

    std::shared_ptr<C2GraphicBlock> block = std::move(mOutputBufferMap[index]);
    C2ConstGraphicBlock constBlock = block->share(C2Rect(mWidth, mHeight), C2Fence());
    int tar_fd = constBlock.handle()->data[0];
    std::shared_ptr<C2Buffer> buffer = C2Buffer::CreateGraphicBuffer(std::move(constBlock));
    C2StreamRequestSyncFrameTuning::output request_sync(0u, C2_FALSE);
    C2StreamBitrateInfo::output bitrate(0u, 64000);

    dumpOutputBuffer(tar_fd, mTarBufferSize);

    if (updateConfig) {
        if(mSyncFrame){
            request_sync = *mRequestSync;
            mSyncFrame = false;
        }
        bitrate = *mBitrate;
    }

    auto fillWork = [buffer, timestamp, updateConfig, request_sync, bitrate](const std::unique_ptr<C2Work> &work) {
        uint32_t flags = 0;
        if ((work->input.flags & C2FrameData::FLAG_END_OF_STREAM) &&
                (c2_cntr64_t(timestamp) == work->input.ordinal.timestamp)) {
            flags |= C2FrameData::FLAG_END_OF_STREAM;
            ALOGV("signalling eos");
        }

        if (updateConfig) {
            ALOGD("updateConfig request_sync=%d,bitrate=%d", (int)request_sync.value, bitrate.value);
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(request_sync));
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(bitrate));
        }

        work->worklets.front()->output.flags = (C2FrameData::flags_t)flags;
        work->worklets.front()->output.buffers.clear();
        work->worklets.front()->output.buffers.push_back(buffer);
        work->worklets.front()->output.ordinal = work->input.ordinal;
        work->workletsProcessed = 1u;
    };
    ALOGV("call finish ts=%lld",(long long)timestamp);
    c2_status_t err = finish(timestamp, fillWork);
    if(err != C2_OK)
        ALOGD("could not find c2work");

    return OK;
}

status_t IsiFilter::startInputStream()
{
    Mutex::Autolock autoLock(mIsiLock);
    if(!mInputStreamOn){
        enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if(0 == ioctl(mFd, VIDIOC_STREAMON, &buf_type)){
            mInputStreamOn = true;
            ALOGV("VIDIOC_STREAMON OUTPUT_MPLANE success");
        }
    }
    return OK;
}
status_t IsiFilter::stopInputStream()
{
    Mutex::Autolock autoLock(mIsiLock);
    if(mInputStreamOn){
        enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if(0 == ioctl(mFd, VIDIOC_STREAMOFF, &buf_type)){
            mInputStreamOn = false;
            ALOGV("VIDIOC_STREAMOFF OUTPUT_MPLANE success");
        }
    }

    while(mInputQueue.size() > 0)
        mInputQueue.pop();

    while(mFreeInputIndex.size() > 0)
        mFreeInputIndex.pop();

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        mInputBufferMap[i].at_device = false;
        mInputBufferMap[i].input_id = -1;
        mInputBufferMap[i].plane.fd = -1;
        mFreeInputIndex.push(i);
    }

    return OK;
}
status_t IsiFilter::startOutputStream()
{
    Mutex::Autolock autoLock(mIsiLock);
    if(!mOutputStreamOn){
        enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if(0 == ioctl(mFd, VIDIOC_STREAMON, &buf_type)){
            mOutputStreamOn = true;
            ALOGV("VIDIOC_STREAMON CAPTURE_MPLANE success");
        }
    }
    return OK;
}
status_t IsiFilter::stopOutputStream()
{
    Mutex::Autolock autoLock(mIsiLock);

    //call VIDIOC_STREAMOFF and ignore the result
    enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(mFd, VIDIOC_STREAMOFF, &buf_type);
    mOutputStreamOn = false;
    ALOGV("VIDIOC_STREAMOFF CAPTURE_MPLANE success");

    while(mOutputQueue.size() > 0)
        mOutputQueue.pop();

    while(mFreeOutputIndex.size() > 0)
        mFreeOutputIndex.pop();

    for (size_t i = 0; i < mOutputBufferMap.size(); i++) {
        mOutputBufferMap[i] = nullptr;
        mFreeOutputIndex.push(i);
    }

    return OK;
}
status_t IsiFilter::destroyInputBuffers()
{
    Mutex::Autolock autoLock(mIsiLock);

    int result = 0;
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;//use mmap to free buffer

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0)
        return UNKNOWN_ERROR;

    ALOGV("destroyInputBuffers success");
    return OK;
}
status_t IsiFilter::destroyOutputBuffers()
{
    Mutex::Autolock autoLock(mIsiLock);

    int result = 0;
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;//use mmap to free buffer

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0)
        return UNKNOWN_ERROR;

    ALOGV("destroyOutputBuffers success");
    return OK;
}
bool IsiFilter::allInputBuffersInIsi(){

    if(mInputStreamOn && mFreeInputIndex.empty())
        return true;

    return false;
}

void IsiFilter::dumpInputBuffer(int fd, uint32_t size)
{
    if(!(mDebugFlag & ISI_FILTER_DEBUG_DUMP_INPUT))
        return;

    //only dump one frame
    if(mInNum != 30)
        return;

    dumpBuffer(fd, size, ISI_FILTER_INPUT_FILE);

    return;
}
void IsiFilter::dumpOutputBuffer(int fd, uint32_t size)
{
    if(!(mDebugFlag & ISI_FILTER_DEBUG_DUMP_OUTPUT))
        return;

    //only dump one frame
    if(mOutNum != 30)
        return;

    dumpBuffer(fd, size, ISI_FILTER_OUTPUT_FILE);

    return;
}

}// namespace android
