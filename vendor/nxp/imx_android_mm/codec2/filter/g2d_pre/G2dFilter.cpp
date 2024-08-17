/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "G2dPreFilter"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>
#include <utils/Log.h>
#include "G2dFilter.h"
#include <IMXC2Interface.h>
#include "graphics_ext.h"
#include "Memory.h"
#include <C2PlatformSupport.h>
#include <C2Buffer.h>
#include <C2Work.h>
#include <cutils/properties.h>
#include "IMXUtils.h"
#include "Codec2BufferUtils.h"


#define G2D_PRE_FILTER_DEBUG_FLAG "vendor.media.nxp.filter.g2d_pre.debug"

#define G2D_PRE_FILTER_DUMP_INPUT 0x1
#define G2D_PRE_FILTER_DUMP_OUTPUT 0x2
#define G2D_PRE_FILTER_DEBUG_TS 0x4

#define G2D_PRE_FILTER_INPUT_FILE "/data/g2d_pre_filter_in.yuv"
#define G2D_PRE_FILTER_OUTPUT_FILE "/data/g2d_pre_filter_out.yuv"

namespace android {

static const uint32_t source_format_table[]={
    HAL_PIXEL_FORMAT_RGB_565,
    HAL_PIXEL_FORMAT_RGB_888,
    HAL_PIXEL_FORMAT_RGBA_8888,
    HAL_PIXEL_FORMAT_RGBX_8888,
    HAL_PIXEL_FORMAT_BGRA_8888,
};

static const uint32_t destination_format_table[]={
    HAL_PIXEL_FORMAT_YCbCr_422_I,
};

G2dFilter::G2dFilter(c2_node_id_t id, C2String name, const std::shared_ptr<C2ReflectorHelper>& helper, const std::shared_ptr<G2dFilterInterface> &intfImpl)
    : IMXC2ComponentBase(std::make_shared<IMXInterface<G2dFilterInterface>>(name, id, intfImpl)),
    mIntfImpl(intfImpl),
    mFetcher(nullptr),
    mBlockPool(nullptr),
    pDev(nullptr),
    mSourceFormat(0),
    mTargetFormat(0),
    mWidth(0),
    mHeight(0),
    mWidthWithAlign(0),
    mHeightWithAlign(0),
    mSrcBufferSize(0),
    mTarBufferSize(0),
    mDebugFlag(0),
    mOutCnt(0),
    mPassthrough(true),
    mStart(false),
    mEOS(false),
    mSendConfig(false),
    mSyncFrame(false),
    mBitrateChange(false)
{
    (void)helper;
}
G2dFilter::~G2dFilter(){
    ALOGV("%s", __FUNCTION__);
    onRelease();
}

c2_status_t G2dFilter::onInit() {
    ALOGV("%s", __FUNCTION__);

    mDebugFlag = property_get_int32(G2D_PRE_FILTER_DEBUG_FLAG, 0);

    if(pDev.get())
        pDev.release();

    pDev = std::make_unique<G2DDevice>(mDebugFlag & G2D_PRE_FILTER_DEBUG_TS);

    if(!pDev)
        return C2_CORRUPTED;

    return C2_OK;
}
void G2dFilter::onStart()
{
    ALOGV("onStart");
    mSourceFormat = mIntfImpl->getPixelFormat();
    mTargetFormat = mSourceFormat;

    mWidth = 0;
    mHeight = 0;
    mSrcBufferSize = 0;
    mTarBufferSize = 0;

    mPassthrough = true;
    mStart = false;
    mEOS = false;

    mBitrate = mIntfImpl->getBitrate_l();
    mRequestSync = mIntfImpl->getRequestSync_l();
    mSendConfig = true;
    mSyncFrame = false;
    mBitrateChange = false;
    ALOGV("onStart mSourceFormat=0x%x, bitrate=%d",mSourceFormat, mBitrate->value);

}
c2_status_t G2dFilter::onStop()
{
    ALOGV("onStop");
    if(mPassthrough)
        return C2_OK;

    pDev->getRunningTimeAndFlush();
    if(mFetcher)
        mFetcher->Stop();

    return C2_OK;
}
c2_status_t G2dFilter::onFlush_sm(){
    ALOGV("onFlush_sm");

    if(mPassthrough)
        return C2_OK;

    if(mFetcher && mFetcher->Running())
        mFetcher->Stop();

    return C2_OK;
}
c2_status_t G2dFilter::drainInternal(uint32_t drainMode){
    ALOGV("drainInternal");
    (void)drainMode;
    return C2_OK;
}
void G2dFilter::onReset(){
    ALOGV("onReset");
}
status_t G2dFilter::verifyFormat(bool source, uint32_t format)
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

status_t G2dFilter::createFrameFetcher(){

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

    ret = mFetcher->SetParam(mWidthWithAlign, mHeightWithAlign, mTargetFormat, 
        (C2AndroidMemoryUsage::HW_TEXTURE_READ | C2AndroidMemoryUsage::HW_CODEC_READ));
    if(ret)
        return ret;

    ret = mFetcher->Start();

    return ret;

}

void G2dFilter::processWork(const std::unique_ptr<C2Work> &work){
    ATRACE_CALL();

    std::shared_ptr<C2Buffer> buffer = nullptr;
    status_t ret = OK;
    int32_t inputId = static_cast<int32_t>(work->input.ordinal.frameIndex.peeku() & 0x3FFFFFFF);

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
            ALOGV("processWork startInternal success");
        }

        if(mFetcher == nullptr){
            ret = UNKNOWN_ERROR;
            break;
        }

        if(!mFetcher->Running())
            mFetcher->Start();

        std::shared_ptr<C2GraphicBlock> dstBlock;

        while(true){
            ret = mFetcher->HasFrames();
            if(ret == OK)
                break;
            else if(ret == TIMED_OUT)
                usleep(3000);
            else
                break;
        }
        if(ret)
            break;

        ret = mFetcher->GetFrameBlock(&dstBlock);
        if(ret){
            ALOGE("GetFrameBlock failed");
            break;
        }

        buffer = work->input.buffers.front();
        const C2ConstGraphicBlock block = buffer->data().graphicBlocks().front();

        int src_fd = block.handle()->data[0];
        uint64_t src_addr = 0;

        ret = IMXGetBufferAddr(src_fd, mSrcBufferSize, src_addr, false);
        if(ret)
            break;

        ALOGV("processWork src_fd=%d, mSrcBufferSize=%d src_addr=%llx", src_fd, mSrcBufferSize, (long long)src_addr);

        getGpuPhysicalAddress(src_fd, mSrcBufferSize, &src_addr);

        int tar_fd = dstBlock->handle()->data[0];

        uint64_t tar_addr;
        ret = IMXGetBufferAddr(tar_fd, mTarBufferSize, tar_addr, false);
        if(ret)
            break;

        ALOGV("processWork tar_fd=%d, mTarBufferSize=%d tar_addr=%llx", tar_fd, mTarBufferSize, (long long)tar_addr);

        getGpuPhysicalAddress(tar_fd, mTarBufferSize, &tar_addr);

        ALOGV("processWork blit src_addr=%llx, tar_addr=%llx", (long long)src_addr, (long long)tar_addr);
        ret = pDev->blit(src_addr, tar_addr);
        if(ret)
            break;

        mOutCnt ++;
        dumpInputBuffer(src_fd, mSrcBufferSize);
        dumpOutputBuffer(tar_fd, mTarBufferSize);

        C2ConstGraphicBlock constBlock = dstBlock->share(C2Rect(mWidthWithAlign, mHeightWithAlign), C2Fence());
        std::shared_ptr<C2Buffer> outC2Buffer = C2Buffer::CreateGraphicBuffer(std::move(constBlock));

        work->worklets.front()->output.buffers.push_back(outC2Buffer);


        if(mSendConfig || mSyncFrame || mBitrateChange){
            C2StreamRequestSyncFrameTuning::output request_sync(0u, C2_FALSE);
            C2StreamBitrateInfo::output bitrate(0u, 64000);
            if(mSyncFrame){
                request_sync = *mRequestSync;
                mSyncFrame = false;
            }
            bitrate = *mBitrate;

            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(request_sync));
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(bitrate));

            mBitrateChange = false;
            mSendConfig = false;
        }
    }while(0);

    if(ret){
        work->result = C2_CORRUPTED;
        work->workletsProcessed = 1u;
        ALOGE("processWork failed, ret=%d",ret);
        return;
    }

    //copy ts & flag
    work->worklets.front()->output.flags = work->input.flags;
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->worklets.front()->output.infoBuffers = work->input.infoBuffers;

    work->workletsProcessed = 1;
    work->result = C2_OK;
    return;

}
void G2dFilter::handleEmptyBuffer(const std::unique_ptr<C2Work> &work){

    // copy work
    work->worklets.front()->output.flags = work->input.flags;
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->worklets.front()->output.infoBuffers = work->input.infoBuffers;

    work->workletsProcessed = 1;
    work->result = C2_OK;
    ALOGV("processWork copy empty buffer");
    return;
}
void G2dFilter::handlePassThroughBuffer(const std::unique_ptr<C2Work> &work){

    if(!mPassthrough)
        return;

    std::shared_ptr<C2Buffer> buffer = work->input.buffers.front();

    if(!buffer)
        return;

    checkDynamicConfigParam();

    if(mSendConfig || mSyncFrame || mBitrateChange){
        C2StreamRequestSyncFrameTuning::output request_sync(0u, C2_FALSE);
        C2StreamBitrateInfo::output bitrate(0u, 64000);

        if(mSyncFrame){
            request_sync = *mRequestSync;
            mSyncFrame = false;
        }
        bitrate = *mBitrate;

        work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(request_sync));
        work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(bitrate));

        ALOGD("add updateConfig 2 request_sync=%d,bitrate=%d,", (int)request_sync.value, bitrate.value);
        mSendConfig = false;
        mBitrateChange = false;
    }

    work->worklets.front()->output.buffers.push_back(std::move(buffer));
    //copy ts & flag
    work->worklets.front()->output.flags = work->input.flags;
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->worklets.front()->output.infoBuffers = work->input.infoBuffers;

    work->workletsProcessed = 1;
    work->result = C2_OK;
    ALOGV("handlePassThroughBuffer success");
    return;
}
status_t G2dFilter::checkDynamicConfigParam(){

    std::shared_ptr<C2StreamIntraRefreshTuning::output> intraRefresh = mIntfImpl->getIntraRefresh_l();
    std::shared_ptr<C2StreamBitrateInfo::output> bitrate = mIntfImpl->getBitrate_l();
    std::shared_ptr<C2StreamRequestSyncFrameTuning::output> requestSync = mIntfImpl->getRequestSync_l();

    if(bitrate != mBitrate) {
        mBitrate = bitrate;
        mBitrateChange = true;
        ALOGV("mBitrateChange true");
    }

    if(requestSync->value == 1){
        mRequestSync = requestSync;
        mSyncFrame = true;
        ALOGV("mSyncFrame true");
        C2StreamRequestSyncFrameTuning::output clearSync(0u, C2_FALSE);
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        mIntfImpl->config({ &clearSync }, C2_MAY_BLOCK, &failures);
    }

    return OK;
}
status_t G2dFilter::updateParam(const std::unique_ptr<C2Work> &work){

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

    mWidthWithAlign = mWidth;
    mHeightWithAlign = mHeight;

    mSrcBufferSize = mWidthWithAlign * mHeight * pxlfmt2bpp(mSourceFormat) / 8;

    if(OK == verifyFormat(true, mSourceFormat)){
        mPassthrough = false;
        setTargetFormat();
    }

    ALOGI("[%p]updateParam mSourceFormat=0x%x, mPassthrough=%d, mWidth=%d, mHeight=%d, mWidthWithAlign=%d",
        this, mSourceFormat, mPassthrough, mWidth, mHeight, mWidthWithAlign);

    return OK;
}
status_t G2dFilter::setTargetFormat()
{

    mTargetFormat = destination_format_table[0];

    mTarBufferSize = mWidthWithAlign * mHeightWithAlign * pxlfmt2bpp(mTargetFormat) / 8;

    ALOGI("mTargetFormat=0x%x ,mTarBufferSize=%d", mTargetFormat,mTarBufferSize);

    return OK;
}

status_t G2dFilter::startInternal(){

    status_t ret = UNKNOWN_ERROR;

    if(pDev.get())
        pDev.release();

    pDev = std::make_unique<G2DDevice>(mDebugFlag & G2D_PRE_FILTER_DEBUG_TS);

    if(!pDev)
        return ret;

    ret = setParameters();
    if(ret)
        return ret;

    ret = createFrameFetcher();
    if(ret != OK){
        ALOGE("createFrameFetcher failed");
        return ret;
    }

    return OK;
}
status_t G2dFilter::setParameters() {

    status_t ret = OK;
    G2D_PARAM src_param;
    G2D_PARAM tar_param;

    if(mPassthrough)
        return ret;

    //TODO: check alignment
    src_param.format = mSourceFormat;
    src_param.width = mWidth;
    src_param.height = mHeight;
    src_param.stride = mWidthWithAlign;

    src_param.cropWidth = mWidth;
    src_param.cropHeight = mHeight;
    src_param.interlace = false;

    ret = pDev->setSrcParam(&src_param);
    if(ret)
        return ret;

    tar_param.format = mTargetFormat;
    tar_param.width = mWidth;
    tar_param.height = mHeight;
    tar_param.stride = mWidthWithAlign;
    tar_param.cropWidth = mWidth;
    tar_param.cropHeight = mHeight;
    tar_param.interlace = false;

    ret = pDev->setDstParam(&tar_param);
    if(ret)
        return ret;

    return OK;
}

void G2dFilter::onRelease() {

    if(mPassthrough)
        return;

    if(mFetcher){
        mFetcher.clear();
    }

    if(pDev.get()){
        pDev.reset(nullptr);
    }

    ALOGV("%s END", __FUNCTION__);
    return;
}

void G2dFilter::dumpInputBuffer(int fd, uint32_t size)
{

    if(!(mDebugFlag & G2D_PRE_FILTER_DUMP_INPUT))
        return;

    //only dump one frame
    if(mOutCnt != 30)
        return;

    dumpBuffer(fd, size, G2D_PRE_FILTER_INPUT_FILE);

    return;
}
void G2dFilter::dumpOutputBuffer(int fd, uint32_t size)
{

    if(!(mDebugFlag & G2D_PRE_FILTER_DUMP_OUTPUT))
        return;

    //only dump one frame
    if(mOutCnt != 30)
        return;

    dumpBuffer(fd, size, G2D_PRE_FILTER_OUTPUT_FILE);

    return;
}


}// namespace android
