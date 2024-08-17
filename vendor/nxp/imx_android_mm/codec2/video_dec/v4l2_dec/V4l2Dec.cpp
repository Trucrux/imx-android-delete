/**
 *  Copyright 2018-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "V4l2Dec"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>

#include "V4l2Dec.h"
#include <media/stagefright/MediaErrors.h>
#include <C2PlatformSupport.h>
#include "graphics_ext.h"
#include "Imx_ext.h"
#include "Memory.h"
#include "IMXUtils.h"
#include "Tsm_wrapper.h"
#include <sys/mman.h>
#include <media/stagefright/foundation/avc_utils.h>
#include <linux/imx_vpu.h>

#if 0
#define DEC_LOG_TS ALOGV
#else
#define DEC_LOG_TS(...)
#endif

namespace android {

#define VPU_DECODER_LOG_LEVELFILE "/data/vpu_dec_level"
#define DUMP_DEC_INPUT_FILE "/data/temp_dec_in.bit"
#define DUMP_DEC_OUTPUT_FILE "/data/temp_dec_out.yuv"

#define DUMP_DEC_FLAG_INPUT     0x1
#define DUMP_DEC_FLAG_OUTPUT    0x2


#define IMX_V4L2_BUF_FLAG_HEADERS_ONLY      0x00200000
#define IMX_V4L2_BUF_FLAG_TIMESTAMP_INVALID    0x00400000

#define Align(ptr,align)    (((uint32_t)(ptr)+(align)-1)/(align)*(align))
#define AMPHION_FRAME_ALIGN_WIDTH     (256)
#define AMPHION_FRAME_ALIGN_HEIGHT    (128)
#define IMX_JPEG_FRAME_ALIGN (16)

// Surface maxDequeueBuffers depends on outputDelay
// some clips request many buffers, V4l2Dec::allocateOutputBuffers() fails
// if Surface maxDequeueBuffers don't have so many buffers.

//stride and slice height are both 16 for g1 decoder
//stride is 16 and slice height is 8 for g2 decoder
#define IS_G2_DECODER   (!strcmp(mMime,MEDIA_MIMETYPE_VIDEO_HEVC) || !strcmp(mMime, MEDIA_MIMETYPE_VIDEO_VP9))
#define HANTRO_FRAME_ALIGN (8)
#define HANTRO_FRAME_ALIGN_WIDTH (HANTRO_FRAME_ALIGN*2)
#define HANTRO_FRAME_ALIGN_HEIGHT (IS_G2_DECODER ? HANTRO_FRAME_ALIGN : HANTRO_FRAME_ALIGN_WIDTH)

#define FRAME_SURPLUS	(4)

static void DecPostSem(sem_t* sem) {
    int ret = sem_post(sem);
    if (ret)
        ALOGW("sem_post failed, ret=%d", ret);
}

static void DecWaitSem(sem_t* sem, int timeout) {
    struct timespec ts;
    int ret = 0;
    if (-1 != clock_gettime(CLOCK_REALTIME, &ts)) {
        ts.tv_sec += timeout; // timeout 3s
        ret = sem_timedwait(sem, &ts);
        if (ret) {
            ALOGW("sem_timedwait failed ret=%d, timeout: %d seconds", ret, timeout);
        }
    }
}

V4l2Dec::TimestampHandler::TimestampHandler():
    bResyncTsm(true),
    mDroppedCount(0) {
    mTsmHandle = tsmCreate();
    mInputIdLenMap.clear();
    if (!mTsmHandle)
        ALOGE("create tsm handler failed");
}

V4l2Dec::TimestampHandler::~TimestampHandler() {
    if (mTsmHandle) {
        tsmDestroy(mTsmHandle);
    }
}
void V4l2Dec::TimestampHandler::pushTimestamp(uint64_t timestamp, uint32_t size, uint32_t id) {
    if (!mTsmHandle)
        return;

    Mutex::Autolock autoLock(mTsLock);
    if (bResyncTsm) {
        tsmReSync(mTsmHandle, timestamp, MODE_AI);
        bResyncTsm = false;
    }

    mInputIdLenMap.emplace(id, size);
    tsmSetBlkTs(mTsmHandle, size, timestamp);
    DEC_LOG_TS("TimestampHandler pushTimestamp ts=%lld, id=%d, size=%d",(long long)timestamp, (int)id, (int)size);
    return;
}

void V4l2Dec::TimestampHandler::validTimestamp(uint32_t id) {
    if (!mTsmHandle)
        return;

    Mutex::Autolock autoLock(mTsLock);
    auto iter = mInputIdLenMap.find(id);
    if (iter == mInputIdLenMap.end()) {
        DEC_LOG_TS("can't find input id %d in TimestampHandler map, maybe it was consumed", id);
        return;
    }

    for (auto iter = mInputIdLenMap.begin(); iter!= mInputIdLenMap.end(); ) {
        if (iter->first <= id) {
            DEC_LOG_TS("TimestampHandler validTimestamp id=%d, size=%d ",(int)id, (int)iter->second);
            tsmSetFrmBoundary(mTsmHandle, 0, iter->second, NULL);
            iter = mInputIdLenMap.erase(iter);
        } else
            ++iter;
    }
}

uint64_t V4l2Dec::TimestampHandler::popTimestamp() {
    if (!mTsmHandle)
        return 0;
    uint64_t ts = 0;
    Mutex::Autolock autoLock(mTsLock);
    for (;mDroppedCount > 0; --mDroppedCount) {
        ts = tsmGetFrmTs(mTsmHandle, NULL);
        DEC_LOG_TS("TimestampHandler popTimestamp drop ts=%lld",(long long)ts);
    }
    ts = tsmGetFrmTs(mTsmHandle, NULL);
    DEC_LOG_TS("TimestampHandler popTimestamp ts=%lld",(long long)ts);
    return ts;
}

void V4l2Dec::TimestampHandler::flushTimestamp() {
    if (!mTsmHandle)
        return;

    Mutex::Autolock autoLock(mTsLock);
    tsmFlush(mTsmHandle);
    bResyncTsm = true;
    mDroppedCount = 0;
    mInputIdLenMap.clear();
}

void V4l2Dec::TimestampHandler::dropTimestampInc(uint32_t count) {
    Mutex::Autolock autoLock(mTsLock);
    mDroppedCount += count;
}

V4l2Dec::V4l2Dec(const char* mime):
    mMime(mime),
    mFetchThread(0),
    pDev(NULL),
    mFd(-1),
    mOutBufType(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE),
    mCapBufType(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE),
    mPoller(NULL){

    bFetchStarted = false;
    bFetchStopped = false;

    bInputStreamOn = false;
    bOutputStreamOn = false;

    bH264 = false;

    bForcePixelFormat = false;

    bSawInputEos = false;
    bNewSegment = true;
    bPendingFlush = false;

    mState = UNINITIALIZED;

    mVpuOwnedOutputBufferNum = 0;
    mRegisteredOutBufNum = 0;
    mUVOffset = 0;

// TODO: remove macro AMPHION_V4L2, use pDev->GetFormatFrameInfo to get align size
#ifdef AMPHION_V4L2
    if (strcmp(mMime, MEDIA_MIMETYPE_VIDEO_MJPEG) == 0) {
        mFrameAlignW = mFrameAlignH = IMX_JPEG_FRAME_ALIGN;
    } else {
        mFrameAlignW = AMPHION_FRAME_ALIGN_WIDTH;
        mFrameAlignH = AMPHION_FRAME_ALIGN_HEIGHT;
        bAdaptiveMode = true;
    }
#else
    mFrameAlignW = HANTRO_FRAME_ALIGN_WIDTH;
    mFrameAlignH = HANTRO_FRAME_ALIGN_HEIGHT;
#endif

    mInputFormat.bufferNum = kInputBufferCount;
    mInputFormat.bufferSize = kInputBufferSizeFor4k;
    mInputFormat.width = DEFAULT_FRM_WIDTH;
    mInputFormat.height = DEFAULT_FRM_HEIGHT;
    mInputFormat.interlaced = false;

    mOutputFormat.width = DEFAULT_FRM_WIDTH;
    mOutputFormat.height = DEFAULT_FRM_HEIGHT;

    mOutputFormat.pixelFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;

    //use bufferNum to check if need fetch buffer, so default is 0
    mOutputFormat.minBufferNum = 0;
    mOutputFormat.bufferNum = 0;
    mOutputFormat.rect.left = 0;
    mOutputFormat.rect.top = 0;
    mOutputFormat.rect.right = mOutputFormat.width;
    mOutputFormat.rect.bottom = mOutputFormat.height;
    mOutputFormat.interlaced = false;

    mVc1Format = V4L2_PIX_FMT_VC1_ANNEX_G;
    mPollTimeoutCnt = 0;

    mInMemType = V4L2_MEMORY_MMAP;
    mOutMemType = V4L2_MEMORY_DMABUF;//V4L2_MEMORY_USERPTR
    bLowLatency = false;
    mInCnt = 0;
    mOutCnt = 0;
    mLastOutputSeq = 0;

    nDebugFlag = 0;
    mInFormat = V4L2_PIX_FMT_H264;
    mOutFormat = V4L2_PIX_FMT_NV12;

    bHasColorAspect = false;
    bHasHdr10StaticInfo = false;
    bHasResChange = false;
    memset(&sHdr10StaticInfo,0,sizeof(DecStaticHDRInfo));
    memset(&mIsoColorAspect,0,sizeof(VideoColorAspect));
    memset(&mSemaphore, 0, sizeof(sem_t));
    mTsHandler = nullptr;
}
V4l2Dec::~V4l2Dec()
{
    if(pDev != NULL && (bFetchStarted || (mPoller && mPoller->isPolling())))
        onStop();

    onDestroy();
    ALOGV("V4l2Dec::~V4l2Dec");
}
status_t V4l2Dec::onInit(){
    ATRACE_CALL();
    status_t ret = UNKNOWN_ERROR;

    if(pDev == NULL){
        pDev = new V4l2Dev();
    }
    if(pDev == NULL)
        return ret;

#ifdef AMPHION_V4L2
    if (!strcmp(mMime, MEDIA_MIMETYPE_VIDEO_MJPEG))
        mFd = pDev->Open(V4L2_DEV_IMX_JPEG_DEC);
    else
        mFd = pDev->Open(V4L2_DEV_DECODER);
#else
    mFd = pDev->Open(V4L2_DEV_DECODER);
#endif
    ALOGV("pV4l2Dev->Open fd=%d",mFd);

    if(mFd < 0)
        return ret;

    ret = pDev->GetVideoBufferType(&mOutBufType, &mCapBufType);
    if (ret != OK)
        return ret;

    if (mPoller == NULL) {
        mPoller = new V4l2Poller(pDev, this);
        mPoller->initPoller(PollThreadWrapper);
    }

    mTsHandler = new TimestampHandler();

    if (0 != sem_init(&mSemaphore, 0, 0)) {
        ALOGE("%s: sem_init failed", __func__);
        return BAD_VALUE;
    }

    mState = UNINITIALIZED;

    ParseVpuLogLevel();

    getV4l2MinBufferCount(mOutBufType);
    getV4l2MinBufferCount(mCapBufType);

    mCodecDataHandler = std::make_unique<CodecDataHandler>();
    return OK;
}

status_t V4l2Dec::onStart()
{
    ATRACE_CALL();
    status_t ret = UNKNOWN_ERROR;
    ALOGV("onStart BEGIN");

    ret = prepareInputParams();
    if(ret != OK){
        ALOGE("prepareInputParams failed");
        return ret;
    }

    ret = SetInputFormats();
    if(ret != OK){
        ALOGE("SetInputFormats failed");
        return ret;
    }

    if(bH264 && bLowLatency){
        ALOGI("enable low latency decoder");
        ret = pDev->EnableLowLatencyDecoder(bLowLatency);
        if(ret != OK)
            ALOGE("EnableLowLatencyDecoder failed");
    }

    if(mInputBufferMap.empty() || (mInputFormat.bufferSize != mInputBufferMap[0].plane.size)){

        ret = prepareInputBuffers();
        if(ret != OK)
            return ret;

        if(mInMemType == V4L2_MEMORY_MMAP)
            ret = createInputBuffers();

        if(ret != OK){
            ALOGE("onStart createInputBuffers failed");
            return ret;
        }
    }

    ret = mCodecDataHandler->allocBuffer(mInputFormat.bufferSize);
    if(ret != OK){
        ALOGE("allocBuffer failed");
        return ret;
    }

    ret = prepareOutputParams();
    if(ret != OK){
        ALOGE("prepareOutputParams failed");
        return ret;
    }

    ret = SetOutputFormats();
    if(ret != OK){
        ALOGE("SetOutputFormats failed");
        return ret;
    }

    mState = RUNNING;

    if(mOutputFormat.bufferNum > 0) {
        destroyFetchThread();
        if (mOutputBufferMap.empty()) {
            importOutputBuffers();
        }
    }

    mInCnt = 0;
    mOutCnt = 0;

    bCodecDataQueued = false;
    mPollTimeoutCnt = 0;
    ALOGV("onStart ret=%d",ret);
    return ret;
}

status_t V4l2Dec::getV4l2MinBufferCount(enum v4l2_buf_type type) {
    struct v4l2_control ctl;
    memset(&ctl, 0, sizeof(struct v4l2_control));

    if (V4L2_TYPE_IS_OUTPUT(type))
        ctl.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
    else
        ctl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

    if (ioctl(mFd, VIDIOC_G_CTRL, &ctl) < 0) {
        ctl.value = 4; // default value;
    }

    if (V4L2_TYPE_IS_OUTPUT(type)) {
        if (ctl.value > mInputFormat.bufferNum && ctl.value < kMaxInputBufferCount) {
            mInputFormat.minBufferNum = ctl.value;
            mInputFormat.bufferNum = mInputFormat.minBufferNum;
        }
    } else {
        mOutputFormat.minBufferNum = ctl.value;
        mOutputFormat.bufferNum = mOutputFormat.minBufferNum;
    }

    return OK;
}

status_t V4l2Dec::prepareInputParams()
{
    status_t ret = UNKNOWN_ERROR;
    Mutex::Autolock autoLock(mLock);

    //TODO: get mime from base class
    ret = pDev->GetStreamTypeByMime(mMime, &mInFormat);
    if(ret != OK){
        return ret;
    }

    // special for vc1 because vc1 has subtype
    if (strcmp(mMime, MEDIA_MIMETYPE_VIDEO_VC1) == 0) {
        mInFormat = mVc1Format;
    }

    if(!pDev->IsOutputFormatSupported(mInFormat)){
        ALOGE("input format not suppoted");
        return ret;
    }

    if(strcmp(mMime, MEDIA_MIMETYPE_VIDEO_AVC) == 0) {
        bH264 = true;
    }

    //input buffer size is set from IMXC2VideoDecoder
    #if 0
    if (mInputFormat.width >= 3840 && mInputFormat.height >= 2160)
        mInputFormat.bufferSize = kInputBufferSizeFor4k;
    else {
        // set bufferSize large enough to avoid this case: 176x144 -> 1920x1080
        mInputFormat.bufferSize = Align(mInputFormat.width * mInputFormat.height * 2, 4096);
        if (mInputFormat.bufferSize < kInputBufferSizeFor1080p)
            mInputFormat.bufferSize = kInputBufferSizeFor1080p;
    }
    #endif
    ALOGV("prepareInputParams bufferSize=%d",mInputFormat.bufferSize);
    #if 0
    struct v4l2_frmsizeenum info;
    memset(&info, 0, sizeof(v4l2_frmsizeenum));
    if(OK != pDev->GetFormatFrameInfo(mInFormat, &info)){
        ALOGE("GetFormatFrameInfo failed");
        return ret;
    }

    if(mInputFormat.width > info->max_width || mInputFormat.height > info->max_height)
        return UNKNOWN_ERROR;
    #endif
    return OK;
}
status_t V4l2Dec::SetInputFormats()
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = mOutBufType;

    if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType)) {
        format.fmt.pix_mp.num_planes = kInputBufferPlaneNum;
        format.fmt.pix_mp.pixelformat = mInFormat;
        format.fmt.pix_mp.width = mInputFormat.width;
        format.fmt.pix_mp.height = mInputFormat.height;
        format.fmt.pix_mp.field = V4L2_FIELD_NONE;
        format.fmt.pix_mp.plane_fmt[0].sizeimage = mInputFormat.bufferSize;
    } else {
        format.fmt.pix.pixelformat = mInFormat;
        format.fmt.pix.width = mInputFormat.width;
        format.fmt.pix.height = mInputFormat.height;
        format.fmt.pix.sizeimage = mInputFormat.bufferSize;
    }
    ALOGV("SetInputFormats width %d, height %d, mOutBufType %d", 
        mInputFormat.width, mInputFormat.height, mOutBufType);

    result = ioctl (mFd, VIDIOC_S_FMT, &format);
    if(result != 0) {
        ALOGE("ioctl VIDIOC_S_FMT failed, result=%d", result);
        return UNKNOWN_ERROR;
    }

    memset(&format, 0, sizeof(format));
    format.type = mOutBufType;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result != 0) {
        ALOGE("ioctl VIDIOC_G_FMT failed, result=%d", result);
        return UNKNOWN_ERROR;
    }

    uint32_t retFormat, retWidth, retHeight, retSizeimage;

    if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType)) {
        retFormat = format.fmt.pix_mp.pixelformat;
        retHeight = format.fmt.pix_mp.height;
        retWidth = format.fmt.pix_mp.width;
        retSizeimage = format.fmt.pix_mp.plane_fmt[0].sizeimage;
    } else {
        retFormat = format.fmt.pix.pixelformat;
        retHeight = format.fmt.pix.height;
        retWidth = format.fmt.pix.width;
        retSizeimage = format.fmt.pix.sizeimage;
    }

    if(retFormat != mInFormat){
        ALOGE("SetInputFormats mInFormat mismatch, set 0x%x, get 0x%x");
        return UNKNOWN_ERROR;
    }

    if(retWidth != mInputFormat.width || retHeight != mInputFormat.height){
        ALOGE("SetInputFormats resolution mismatch");
        return UNKNOWN_ERROR;
    }

    if(retSizeimage != mInputFormat.bufferSize){
        ALOGW("SetInputFormats bufferSize mismatch retSizeimage %d input bufferSize %d",
                retSizeimage, mInputFormat.bufferSize);
        mInputFormat.bufferSize = retSizeimage;
    }

    return OK;
}
status_t V4l2Dec::prepareOutputParams()
{
    status_t ret = UNKNOWN_ERROR;
    Mutex::Autolock autoLock(mLock);

    ret = pDev->GetV4l2FormatByColor(mOutputFormat.pixelFormat, &mOutFormat);
    if(ret != OK) {
        ALOGE("GetV4l2FormatByColor failed, mOutputFormat.pixelFormat=0x%x", mOutputFormat.pixelFormat);
        return ret;
    }

    ALOGD("prepareOutputParams begin pixelFormat=0x%x,mOutFormat=0x%x",mOutputFormat.pixelFormat, mOutFormat);

    if (!pDev->IsCaptureFormatSupported(mOutFormat)) {
        for (uint32_t i = 0;;i++) {
            if (pDev->GetCaptureFormat(&mOutFormat, i) != OK)
                return BAD_VALUE;
            else if (pDev->GetColorFormatByV4l2(mOutFormat, (uint32_t*)&mOutputFormat.pixelFormat) != OK
                             || V4L2_FORMAT_IS_NON_CONTIGUOUS_PLANES(mOutFormat))
                continue;
            else
                break;
        }
    }

    //update output frame width & height
    mOutputFormat.width = Align(mOutputFormat.rect.right, mFrameAlignW);
    mOutputFormat.height = Align(mOutputFormat.rect.bottom, mFrameAlignH);

    ALOGV("prepareOutputParams rect %d/%d, align %d/%d, aligned %d/%d",
            mOutputFormat.rect.right, mOutputFormat.rect.bottom, mFrameAlignW,mFrameAlignH,
            mOutputFormat.width , mOutputFormat.height);

    mOutputPlaneSize[0] = mOutputFormat.width * mOutputFormat.height * pxlfmt2bpp(mOutputFormat.pixelFormat) / 8;
    ALOGV("prepareOutputParams pixel format =0x%x,success",mOutputFormat.pixelFormat);

    if(bAdaptiveMode){
        //driver's buffer size was get from get_tiled_8l128_plane_size() of vpu_helpers.c
        //allocator buffer size is calculated by different method in MemoryDesc::checkFormat() FORMAT_NV12_TILED
        //we follow allocator method to set buffer buffer size
        mMaxWidth = Align(mMaxWidth, mFrameAlignW);
        mMaxHeight = Align(mMaxHeight, mFrameAlignH);

        uint32_t buffer_height = Align(mMaxHeight, mFrameAlignW);
        mMaxOutputBufferSize = mMaxWidth * buffer_height * 3 / 2;
        ALOGV("prepareOutputParams mMaxOutputBufferSize=%d",mMaxOutputBufferSize);
    }

    return OK;
}
status_t V4l2Dec::SetOutputFormats()
{
    int result = 0;
    uint32_t alignedWidth, retFormat;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = mCapBufType;

    if(pDev->Is10BitV4l2Format(mOutFormat)){
        alignedWidth = Align(mOutputFormat.width*5/4, mFrameAlignW);
    } else {
        alignedWidth = Align(mOutputFormat.width, mFrameAlignW);
        mOutputFormat.width = Align(mOutputFormat.width, mFrameAlignW);
        mOutputFormat.height = Align(mOutputFormat.height, mFrameAlignH);
    }

    if (V4L2_TYPE_IS_MULTIPLANAR(mCapBufType)) {
        format.fmt.pix_mp.num_planes = kOutputBufferPlaneNum;
        format.fmt.pix_mp.pixelformat = mOutFormat;
        format.fmt.pix_mp.width = mOutputFormat.width;
        format.fmt.pix_mp.height = mOutputFormat.height;
        format.fmt.pix_mp.plane_fmt[0].bytesperline = alignedWidth;
        format.fmt.pix_mp.field = V4L2_FIELD_NONE;
    } else {
        format.fmt.pix.pixelformat = mOutFormat;
		format.fmt.pix.width = mOutputFormat.width;
		format.fmt.pix.height = mOutputFormat.height;
		format.fmt.pix.bytesperline = alignedWidth;
    }

    ALOGV("SetOutputFormats w=%d,h=%d,alignedWidth=%d,fmt=0x%x, OutputPlaneSize=%d",
        mOutputFormat.width, mOutputFormat.height, alignedWidth,mOutFormat, mOutputPlaneSize[0]);

    result = ioctl (mFd, VIDIOC_S_FMT, &format);
    if(result != 0){
        ALOGE("SetOutputFormats VIDIOC_S_FMT failed");
        return UNKNOWN_ERROR;
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = mCapBufType;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result < 0)
        return UNKNOWN_ERROR;

    if (V4L2_TYPE_IS_MULTIPLANAR(mCapBufType)) {
        retFormat = format.fmt.pix_mp.pixelformat;
        if(format.fmt.pix_mp.plane_fmt[0].sizeimage !=  mOutputPlaneSize[0]){
            ALOGW("SetOutputFormats bufferSize mismatch, set %d, get %d",
                mOutputPlaneSize[0], format.fmt.pix_mp.plane_fmt[0].sizeimage);
            mOutputPlaneSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
        }
    } else {
        retFormat = format.fmt.pix.pixelformat;
        if(format.fmt.pix.sizeimage !=  mOutputPlaneSize[0]) {
            ALOGW("SetOutputFormats bufferSize mismatch, %d -> %d",
                mOutputPlaneSize[0], format.fmt.pix.sizeimage);
            mOutputPlaneSize[0] = format.fmt.pix.sizeimage;
            mOutputFormat.bufferSize = mOutputPlaneSize[0];
        }
    }

    if(retFormat != mOutFormat){
        ALOGE("SetOutputFormats mOutFormat mismatch");
        return UNKNOWN_ERROR;
    }

    ALOGV("SetOutputFormats success");
    return OK;
}

V4l2Dec::InputRecord::InputRecord()
    : at_device(false), input_id(-1), ts(-1) {
    memset(&plane, 0, sizeof(VideoFramePlane));
}

V4l2Dec::InputRecord::~InputRecord() {}

V4l2Dec::OutputRecord::OutputRecord()
    : at_device(false), picture_id(0), flag(0) {
    memset(&planes, 0, sizeof(VideoFramePlane)*kOutputBufferPlaneNum);
}

V4l2Dec::OutputRecord::~OutputRecord() {
}

status_t V4l2Dec::prepareInputBuffers()
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = mInputFormat.bufferNum;
    reqbufs.type = mOutBufType;
    reqbufs.memory = mInMemType;

    ALOGV("prepareInputBuffers count=%d",reqbufs.count);
    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGE("VIDIOC_REQBUFS failed result=%d",result);
        return UNKNOWN_ERROR;
    }

    while(!mFreeInputIndex.empty())
        mFreeInputIndex.pop();

    mInputBufferMap.resize(reqbufs.count);

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        mInputBufferMap[i].at_device = false;
        mInputBufferMap[i].plane.fd = -1;
        mInputBufferMap[i].plane.vaddr = 0;
        mInputBufferMap[i].plane.paddr = 0;
        mInputBufferMap[i].plane.size = mInputFormat.bufferSize;
        mInputBufferMap[i].plane.length = 0;
        mInputBufferMap[i].plane.offset = 0;
        mInputBufferMap[i].input_id = -1;
        mFreeInputIndex.push(i);
    }

    ALOGV("prepareInputBuffers total input=%d size=%d",mInputFormat.bufferNum, mInputBufferMap.size());

    return OK;
}
status_t V4l2Dec::createInputBuffers()
{

    int result = 0;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes;
    void * ptr = NULL;
    uint64_t tmp = 0;

    if(mInMemType != V4L2_MEMORY_MMAP)
        return UNKNOWN_ERROR;

    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(&planes, 0, sizeof(planes));

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        stV4lBuf.type = mOutBufType;
        stV4lBuf.memory = V4L2_MEMORY_MMAP;
        stV4lBuf.index = i;

        if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType)) {
            stV4lBuf.length = kInputBufferPlaneNum;
            stV4lBuf.m.planes = &planes;
        }
        result = ioctl(mFd, VIDIOC_QUERYBUF, &stV4lBuf);
        if(result < 0)
            return UNKNOWN_ERROR;

        planes.length = mInputFormat.bufferSize;

        if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType)) {
            ptr = mmap(NULL, planes.length,
                    PROT_READ | PROT_WRITE, /* recommended */
                    MAP_SHARED,             /* recommended */
                    mFd, planes.m.mem_offset);
        } else {
		    ptr = mmap(NULL, stV4lBuf.length,
    				PROT_READ | PROT_WRITE,
    				MAP_SHARED,
    				mFd, stV4lBuf.m.offset);
        }

        if(ptr != MAP_FAILED){
            tmp = (uint64_t)ptr;
            mInputBufferMap[i].plane.vaddr = tmp;
        }else
            return NO_MEMORY;
    }

    ALOGV("createInputBuffers success");
    return OK;
}
status_t V4l2Dec::destroyInputBuffers()
{
    Mutex::Autolock autoLock(mLock);
    if (mInputBufferMap.empty())
        return OK;

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        if(mInMemType == V4L2_MEMORY_MMAP && mInputBufferMap[i].plane.vaddr != 0)
            munmap((void*)(uintptr_t)mInputBufferMap[i].plane.vaddr, mInputBufferMap[i].plane.size);
    }

    int result = 0;
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 0;
    reqbufs.type = mOutBufType;
    reqbufs.memory = V4L2_MEMORY_MMAP;//use mmap to free buffer

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGV("ignore the result");
    }

    mInputBufferMap.clear();
    while(!mFreeInputIndex.empty())
        mFreeInputIndex.pop();

    ALOGV("destroyInputBuffers success");
    return OK;
}
status_t V4l2Dec::importOutputBuffers()
{
    {
        Mutex::Autolock autoLock(mLock);
        if(mState == STOPPING)
            return OK;

        int result = 0;
        struct v4l2_requestbuffers reqbufs;
        memset(&reqbufs, 0, sizeof(reqbufs));
        reqbufs.count = kMaxOutputBufferCount;
        reqbufs.type = mCapBufType;
        reqbufs.memory = mOutMemType;

        result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

        if(result != 0){
            return UNKNOWN_ERROR;
        }

        mOutputBufferMap.resize(kMaxOutputBufferCount);
    }

    createFetchThread();
    return OK;
}
status_t V4l2Dec::destroyOutputBuffers()
{
    if (bOutputStreamOn)
        stopOutputStream();

    Mutex::Autolock autoLock(mLock);
    if (mOutputBufferMap.empty())
        return OK;

    int result = 0;
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 0;
    reqbufs.type = mCapBufType;
    reqbufs.memory = V4L2_MEMORY_MMAP;//use mmap to free buffer

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGV("ignore VIDIOC_REQBUFS result");
    }

    mRegisteredOutBufNum = 0;
    mOutputBufferMap.clear();
    ALOGV("destroyOutputBuffers success");
    return OK;
}
status_t V4l2Dec::PollThreadWrapper(void *me, status_t poll_ret) {
    return static_cast<V4l2Dec *>(me)->HandlePollThread(poll_ret);
}
status_t V4l2Dec::HandlePollThread(status_t poll_ret){

    status_t ret = OK;

    ALOGV("Poller: ret=%x, in=%d,out=%d, vpu_in=%d, vpu_out=%d", poll_ret,mInCnt, mOutCnt,
        mInputFormat.bufferNum - mFreeInputIndex.size(), mVpuOwnedOutputBufferNum );

    if (poll_ret & V4L2_DEV_POLL_EVENT) {
        onDequeueEvent();
    }
    if (poll_ret & V4L2_DEV_POLL_OUTPUT) {
        dequeueInputBuffer();
    }
    if (poll_ret & V4L2_DEV_POLL_CAPTURE) {
        dequeueOutputBuffer();
    }

    if (0 == poll_ret)
        mPollTimeoutCnt ++;
    else
        mPollTimeoutCnt = 0;

    //detect vpu hang
    if (isDeviceHang(mPollTimeoutCnt)) {
        ALOGE("Poller: NOT_ENOUGH_DATA");
        NotifyError(TIMED_OUT);
        mPollTimeoutCnt = 0;
        ret = TIMED_OUT;
    }

    //in kernel_imx/drivers/media/test-drivers/vicodec/vicodec-core.c vicodec_encoder_cmd()
    //driver will not send eos frame until both output and capture port stream on.
    //for one frame that only has codec data, if driver could not decode it, detect eos here.
    if(poll_ret == 0 && isDeviceEos()){
        handleDecEos();
    }

    if((poll_ret & V4L2_DEV_POLL_ERROR) && mPoller && mPoller->isPolling()){
        ALOGE("Poller: NotifyError");
        NotifyError(UNKNOWN_ERROR);
        ret = UNKNOWN_ERROR;
    }

    return ret;
}
status_t V4l2Dec::HandleFetchThread()
{
    int64_t waitUs = ALooper::GetNowUs();
    status_t ret;

    while(bFetchStarted){
        // only start fetching when vpu output buffer isn't enough
        if (mVpuOwnedOutputBufferNum >= mOutputFormat.bufferNum ||
                (RUNNING != mState && RES_CHANGED != mState) || mOutputBufferMap.size() == 0) {
            waitUs = ALooper::GetNowUs();
            usleep(5000);
            continue;
        }
        ALOGV("getFreeGraphicBlock begin mVpuOwnedOutputBufferNum %d mOutputFormat.bufferNum %d", mVpuOwnedOutputBufferNum, mOutputFormat.bufferNum);

        GraphicBlockInfo *gbInfo = getFreeGraphicBlock();
        if(!gbInfo) {
            ret = fetchOutputBuffer();
            if (OK == ret) {
                gbInfo = getFreeGraphicBlock();
            } else if (WOULD_BLOCK == ret) {
                if (bNewSegment && mVpuOwnedOutputBufferNum < mOutputFormat.minBufferNum &&
                        ALooper::GetNowUs() - waitUs > 1000000) {
                    // can't fetch more buffer after flush, migrate current buffer and fetch new
                    migrateOutputBuffers();
                }
                usleep(1000);
                continue;
            } else {
                bReceiveError = true;
                NotifyError(BAD_VALUE);
                break;
            }
        }

        queueOutput(gbInfo);
        waitUs = ALooper::GetNowUs();
    }
    ALOGV("HandleFetchThread stopped");
    bFetchStopped = true;
    return OK;
}
// static
void *V4l2Dec::FetchThreadWrapper(void *me) {
    return (void *)(uintptr_t)static_cast<V4l2Dec *>(me)->HandleFetchThread();
}

status_t V4l2Dec::createFetchThread()
{
    ALOGV("%s", __FUNCTION__);
    Mutex::Autolock autoLock(mLock);
    if(!bFetchStarted){
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        bFetchStarted = true;
        bFetchStopped = false;
        pthread_create(&mFetchThread, &attr, FetchThreadWrapper, this);
        pthread_attr_destroy(&attr);
    }
    return OK;
}
status_t V4l2Dec::destroyFetchThread()
{
    ALOGV("%s", __FUNCTION__);

    if(bFetchStarted){
        int32_t cnt = 0;
        bFetchStarted = false;
        while(!bFetchStopped && cnt < 20){
            usleep(1000);
            cnt++;
        }
        ALOGV("%s bFetchStopped=%d,cnt=%d", __FUNCTION__,bFetchStopped,cnt);
    }

    if(mFetchThread){
        pthread_join(mFetchThread, NULL);
        mFetchThread = 0;
    }

    ALOGV("%s END", __FUNCTION__);
    return OK;
}
bool V4l2Dec::canProcessWork(){

    if(allInputBuffersInVPU())
        return false;
    else
        return true;
}

status_t V4l2Dec::decodeInternal(std::unique_ptr<IMXInputBuffer> input)
{
    ATRACE_CALL();
    int result = 0;
    int32_t index = -1;
    uint32_t v4l2_flags = 0;

    if(input == nullptr)
        return BAD_VALUE;

    if(STOPPED == mState || UNINITIALIZED == mState) {
        if (OK != onStart())
            return BAD_VALUE;
    }

    #if 0
    if (!bCodecDataQueued && pCodecDataBuf && nCodecDataLen > 0) {
        ALOGV("queue codecdata, len: %d", nCodecDataLen);

        bCodecDataQueued = true;

        status_t ret = decodeInternal(std::make_unique<IMXInputBuffer>(
                                                pCodecDataBuf, -1, -1, nCodecDataLen, -1, false, true));
        if (ret != OK) {
            ALOGE("queue codecdata failed with ret %d", ret);
            return ret;
        }
    }
    #endif
    if(input->csd && input->pInBuffer && !mCodecDataHandler->sent() && !bSecureMode){
        ALOGV("add codecdata, len: %d", input->size);
        mCodecDataHandler->addData((uint8_t*)input->pInBuffer, input->size);
        return OK;
    }

    if (!mCodecDataHandler->sent() && !bSecureMode) {
        uint8_t* pInbuf = nullptr;
        uint32_t codecDataLen = 0;
        int codec_data_fd = -1;
        status_t ret = OK;

        if(mInMemType == V4L2_MEMORY_DMABUF)
            ret = mCodecDataHandler->getData(&codec_data_fd, &codecDataLen);
        else
            ret = mCodecDataHandler->getData(&pInbuf, &codecDataLen);

        ALOGV("get codecdata, len: %d ret=%d", codecDataLen,ret);

        if(OK == ret){
            ret = decodeInternal(std::make_unique<IMXInputBuffer>(
                                                    pInbuf, codec_data_fd, -1, codecDataLen, -1, false, true));
            if (ret != OK) {
                ALOGE("queue codecdata failed with ret %d", ret);
                return ret;
            }
        }
    }


    int32_t fd = input->fd;
    int64_t ts = (int64_t)input->timestamp;
    uint32_t buf_length = 0;
    bool eos = input->eos;

    if (eos) {
        ALOGD("get input eos, call stopDecoder");
        Mutex::Autolock autoLock(mLock);
        bSawInputEos = true;
        pDev->StopDecoder();
        return OK;
    }

    if (input->csd){
        int ret = pDev->EnableSeparateMode();
        if(ret != OK)
            ALOGE("enable separate mode fail!");
        v4l2_flags |= (IMX_V4L2_BUF_FLAG_HEADERS_ONLY | IMX_V4L2_BUF_FLAG_TIMESTAMP_INVALID);
        ts = -1;
    }

    if(input->size > mInputFormat.bufferSize){
        ALOGE("invalid buffer size=%d,cap=%d",input->size, mInputFormat.bufferSize);
        return UNKNOWN_ERROR;
    }

    #if 0
    //check nal type for h264 frame, do not queue single sps or pps frame as one normal frame.
    if(bH264 && fd > 0 && input->id && input->size > 0 && !bSecureMode && !(v4l2_flags & IMX_V4L2_BUF_FLAG_HEADERS_ONLY)){

        const uint8_t * data = (uint8_t *)input->pInBuffer;
        size_t data_size = input->size;
        const uint8_t * nal_start = NULL;
        size_t nal_size = 0;
        bool codec_data_nal = false;
        bool has_other_nal = false;

        while (getNextNALUnit(&data, &data_size, &nal_start, &nal_size, true) == OK) {
            if (nal_size == 0) continue;

            unsigned nalType = nal_start[0] & 0x1f;
            if(nalType <= 5 || nalType == 20){
                has_other_nal = true;
                break;
            }else if(nalType >= 6 && nalType <= 8){
                codec_data_nal = true;
            }
        }

        if(!has_other_nal && codec_data_nal){
            ALOGV("SKIP SPS or PPS");
            //return bad value then client IMXC2VideoDecoder will handle it
            return BAD_VALUE;
        }
    }
    #endif

QueueOneBuffer:
    mLock.lock();

    if(mFreeInputIndex.empty()){
        mLock.unlock();
        ALOGE("decodeInternal ERROR, empty");
        //First frame after codec data needs 2 index, one for codec data, the other for first frame.
        usleep(5000);
        goto QueueOneBuffer;
    }

    index = mFreeInputIndex.front();
    if(index >= mInputFormat.bufferNum){
        mLock.unlock();
        ALOGE("decodeInternal ERROR, invalid index");
        return BAD_VALUE;
    }

    mInputBufferMap[index].input_id = input->id;
    ALOGV("decodeInternal input->BUF=%p, index=%d, len=%zu, ts=%lld, fd=%d",input->pInBuffer, index, input->size, ts, fd);

    if(mInMemType == V4L2_MEMORY_MMAP){
        uint32_t offset = 0;
        if (bSecureMode) {
#ifdef HANTRO_V4L2
            // vsi vpu reserve 16 bytes to save physical address
            uint64_t paddr;
            if (IMXGetBufferAddr(fd, input->size, (uint64_t&)paddr, false) == 0) {
                memcpy((void*)(uintptr_t)mInputBufferMap[index].plane.vaddr, &paddr, sizeof(uint64_t));
                offset += 16;
            } else {
                mLock.unlock();
                ALOGE("can't get physical address in secure mode");
                return BAD_VALUE;
            }
#endif
        }

        memcpy((void*)(uintptr_t)(mInputBufferMap[index].plane.vaddr + offset), input->pInBuffer, input->size);
        buf_length += input->size;
        dumpInputBuffer((void*)(uintptr_t)(mInputBufferMap[index].plane.vaddr + offset), buf_length);
    }else if(mInMemType == V4L2_MEMORY_DMABUF){
        buf_length += input->size;
        mInputBufferMap[index].plane.fd = fd;
        dumpInputBuffer(fd, buf_length);
     }

    if(mInputBufferMap[index].at_device){
        ALOGE("onQueueInputBuffer index=%d, at_device",index);
    }

    struct v4l2_buffer stV4lBuf;
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    struct v4l2_plane plane;//kInputBufferPlaneNum
    memset(&plane, 0, sizeof(plane));

    stV4lBuf.index = index;
    stV4lBuf.type = mOutBufType;

    if (mTsHandler && !input->csd) {
        stV4lBuf.timestamp.tv_sec = input->id;
        stV4lBuf.timestamp.tv_usec = 0;
        v4l2_flags |= (V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TIMESTAMP_COPY);
    } else if(ts >= 0) {
        stV4lBuf.timestamp.tv_sec = ts / 1000000;
        stV4lBuf.timestamp.tv_usec = ts % 1000000;
        v4l2_flags |= (V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TIMESTAMP_COPY);
    } else {
        stV4lBuf.timestamp.tv_sec = -1;
        stV4lBuf.timestamp.tv_usec = 0;
        v4l2_flags |= IMX_V4L2_BUF_FLAG_TIMESTAMP_INVALID;
    }
    stV4lBuf.memory = mInMemType;
    stV4lBuf.flags = v4l2_flags;

    if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType)) {

        plane.bytesused = buf_length;
        plane.length = mInputFormat.bufferSize;
        plane.data_offset = 0;

        if(mInMemType == V4L2_MEMORY_MMAP)
            plane.m.mem_offset = 0;
        else if(mInMemType == V4L2_MEMORY_USERPTR)
            plane.m.userptr = (unsigned long)mInputBufferMap[index].plane.vaddr;
        else if(mInMemType == V4L2_MEMORY_DMABUF)
            plane.m.fd = mInputBufferMap[index].plane.fd;

        stV4lBuf.m.planes = &plane;
        stV4lBuf.length = kInputBufferPlaneNum;
    } else {
        stV4lBuf.bytesused = buf_length;
        stV4lBuf.length = mInputFormat.bufferSize;

        if(mInMemType == V4L2_MEMORY_DMABUF)
            stV4lBuf.m.fd = fd;
    }

    ALOGV("VIDIOC_QBUF OUTPUT BEGIN index=%d,len=%d, ts=%lld, input_id=%d\n",
        stV4lBuf.index, buf_length, (long long)ts, input->id);

    result = ioctl(mFd, VIDIOC_QBUF, &stV4lBuf);
    if(result < 0){
        ALOGE("VIDIOC_QBUF OUTPUT failed, index=%d, errno=%d",index, errno);
        mLock.unlock();
        return UNKNOWN_ERROR;
    }

    mFreeInputIndex.pop();
    mInputBufferMap[index].at_device = true;
    ALOGV("VIDIOC_QBUF OUTPUT END index=%d,len=%d, ts=%lld\n",
        stV4lBuf.index, buf_length, (long long)ts);

    if(!(v4l2_flags & IMX_V4L2_BUF_FLAG_HEADERS_ONLY))
        mInCnt++;

    mLock.unlock();

    if (!bInputStreamOn) {
        startInputStream();
    }

    if(!mPoller->isPolling()) {
        mPoller->startPolling();
    }

    if (mTsHandler && !input->csd) {
        mTsHandler->pushTimestamp(ts, buf_length, input->id);
    }

    return OK;
}
status_t V4l2Dec::dequeueInputBuffer()
{
    ATRACE_CALL();
    int result = 0;
    int input_id = -1;
    struct v4l2_buffer stV4lBuf;
    bool skip = false;

    if(!bInputStreamOn || mState != RUNNING )
        return OK;
    {
        Mutex::Autolock autoLock(mLock);

        if(!bInputStreamOn || mState != RUNNING)
            return OK;

#ifdef AMPHION_V4L2
        // always keep at least one buffer in input queue to avoid
        // vpu return poll error due to empty OUTPUT and CAPTURE queue
        if (mInputFormat.bufferNum - mFreeInputIndex.size() <= 1) {
            return OK;
        }
#endif
        memset(&stV4lBuf, 0, sizeof(stV4lBuf));
        stV4lBuf.type = mOutBufType;
        stV4lBuf.memory = mInMemType;

        if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType)) {
            struct v4l2_plane planes[kInputBufferPlaneNum];
            memset(planes, 0, sizeof(planes));
            stV4lBuf.m.planes = planes;
            stV4lBuf.length = kInputBufferPlaneNum;
        }

        ALOGV("VIDIOC_DQBUF OUTPUT BEGIN");
        result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
        if(result < 0){
            ALOGW("%s VIDIOC_DQBUF err=%d", __FUNCTION__, errno);
            return UNKNOWN_ERROR;
        }

        if(stV4lBuf.index >= mInputFormat.bufferNum)
            return BAD_INDEX;

        input_id = mInputBufferMap[stV4lBuf.index].input_id;

        ALOGV("VIDIOC_DQBUF OUTPUT END index=%d, flag=%x input_id=#%d",stV4lBuf.index, stV4lBuf.flags,input_id);
        if(mInputBufferMap[stV4lBuf.index].at_device){
            mInputBufferMap[stV4lBuf.index].at_device = false;
            mFreeInputIndex.push(stV4lBuf.index);
        }

        mInputBufferMap[stV4lBuf.index].input_id = -1;

        if(stV4lBuf.flags & V4L2_BUF_FLAG_ERROR){
            ALOGE("VIDIOC_DQBUF OUTPUT flag error");
            skip = true;
        }
    }

    if(input_id >= 0)
        NotifyInputBufferUsed(input_id);

    // amphion upstream driver returns input buffer of codec data with FLAG_ERROR and input_id=-1
    // need to avoid skipping input buffer and dropping timestamp in this case
    if(skip && input_id >= 0){
        ALOGD("NotifySkipInputBuffer id=%d",input_id);
        NotifySkipInputBuffer(input_id);
        if (mTsHandler)
            (void)mTsHandler->dropTimestampInc(1);
    }

    return OK;
}
status_t V4l2Dec::queueOutput(GraphicBlockInfo* pInfo)
{
    ATRACE_CALL();
    int result = 0;
    int32_t fd[kOutputBufferPlaneNum];
    uint64_t vaddr[kOutputBufferPlaneNum];
    uint64_t paddr[kOutputBufferPlaneNum];
    uint32_t offset[kOutputBufferPlaneNum];
    int32_t index = -1;

    if(!bFetchStarted || STOPPING == mState || FLUSHING == mState || RES_CHANGING == mState){
        ALOGV("queueOutput return 1");
        return OK;
    }

    ALOGV("queueOutput BEGIN id=%d",pInfo->mBlockId);
    mLock.lock();

    if(!bFetchStarted || STOPPING == mState || FLUSHING == mState || RES_CHANGING == mState){
        mLock.unlock();
        ALOGV("queueOutput return 2");
        return OK;
    }

    vaddr[0] = pInfo->mVirtAddr;
    paddr[0] = pInfo->mPhysAddr;
    offset[0] = 0;
    fd[0] = pInfo->mDMABufFd;

    //try to get index
    for(int32_t i = 0; i < mOutputBufferMap.size(); i++){
        if(pInfo->mPhysAddr == mOutputBufferMap[i].planes[0].paddr){
            index = i;
            break;
        }
    }

    //index not found
    if(index < 0){
        for(int32_t i = 0; i < mOutputBufferMap.size(); i++){
            if(0 == mOutputBufferMap[i].planes[0].paddr){
                mOutputBufferMap[i].planes[0].fd = fd[0];
                mOutputBufferMap[i].planes[0].vaddr = vaddr[0];
                mOutputBufferMap[i].planes[0].paddr = paddr[0];
                mOutputBufferMap[i].planes[0].offset = offset[0];
                mOutputBufferMap[i].picture_id = pInfo->mBlockId;
                mOutputBufferMap[i].at_device = false;
                index = i;
                ++mRegisteredOutBufNum;
                break;
            }
        }
    }

    if(index < 0){
        ALOGE("could not create index");
        mLock.unlock();
        return UNKNOWN_ERROR;
    }

    if(mOutputBufferMap[index].at_device){
        ALOGE("queueOutput at_device,index=%d, pInfo->mBlockId=%d",index,pInfo->mBlockId);
    }

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[kOutputBufferPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(&planes, 0, sizeof(planes));

    stV4lBuf.index = index;
    stV4lBuf.type = mCapBufType;
    stV4lBuf.memory = mOutMemType;
    stV4lBuf.flags = 0;

    if (V4L2_TYPE_IS_MULTIPLANAR(mCapBufType)) {
        if (mOutMemType == V4L2_MEMORY_DMABUF) {
            planes[0].m.fd = fd[0];
        } else if(mOutMemType == V4L2_MEMORY_USERPTR) {
            planes[0].m.userptr = vaddr[0];
        }

        planes[0].length = mOutputBufferMap[index].planes[0].size;
        planes[0].data_offset = mOutputBufferMap[index].planes[0].offset;

        stV4lBuf.m.planes = &planes[0];
        stV4lBuf.length = kOutputBufferPlaneNum;
    } else {
        if (mOutMemType == V4L2_MEMORY_USERPTR) {
            stV4lBuf.length = mOutputPlaneSize[0];
            stV4lBuf.m.userptr = (unsigned long)vaddr[0];
        } else if (mOutMemType == V4L2_MEMORY_DMABUF) {
            stV4lBuf.length = mOutputPlaneSize[0];
            stV4lBuf.m.fd = fd[0];
        }
    }

    ALOGV("VIDIOC_QBUF CAPTURE BEGIN index=%d blockId=%d fd=%d\n",index, pInfo->mBlockId, fd[0]);

    result = ioctl(mFd, VIDIOC_QBUF, &stV4lBuf);
    if(result < 0){
        ALOGE("VIDIOC_QBUF CAPTURE failed, index=%d, errno=%d",index, errno);
        mLock.unlock();
        return UNKNOWN_ERROR;
    }

    ALOGV("VIDIOC_QBUF CAPTURE END index=%d blockId=%d\n",index, pInfo->mBlockId);
    GraphicBlockSetState(pInfo->mBlockId, GraphicBlockInfo::State::OWNED_BY_VPU);

    mVpuOwnedOutputBufferNum++;
    mOutputBufferMap[index].at_device = true;
    mLock.unlock();

    if (!bOutputStreamOn) {
        startOutputStream();
        Mutex::Autolock autoLock(mLock);
        if (RES_CHANGED == mState) {
            mState = RUNNING;
        }
        if (!bInputStreamOn || bPendingFlush) {
            DecPostSem(&mSemaphore);
        }
    }

    return OK;
}

status_t V4l2Dec::startInputStream()
{
    ALOGV("%s", __FUNCTION__);
    DecWaitSem(&mSemaphore, 3/*timeout*/);
    Mutex::Autolock autoLock(mLock);
    if(!bInputStreamOn){
        enum v4l2_buf_type buf_type = mOutBufType;
        if(0 == ioctl(mFd, VIDIOC_STREAMON, &buf_type)){
            bInputStreamOn = true;
            ALOGV("%s OK", __FUNCTION__);
        }
    }
    return OK;
}
status_t V4l2Dec::stopInputStream()
{
    ALOGV("%s", __FUNCTION__);
    Mutex::Autolock autoLock(mLock);
    if(bInputStreamOn){
        enum v4l2_buf_type buf_type = mOutBufType;
        if(0 == ioctl(mFd, VIDIOC_STREAMOFF, &buf_type)){
            bInputStreamOn = false;
            ALOGV("%s OK", __FUNCTION__);
        }
    }

    while(!mFreeInputIndex.empty())
        mFreeInputIndex.pop();

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        mInputBufferMap[i].at_device = false;
        mInputBufferMap[i].input_id = -1;
        mFreeInputIndex.push(i);
    }

    bInputStreamOn = false;
    return OK;
}
status_t V4l2Dec::startOutputStream()
{
    ALOGV("%s", __FUNCTION__);
    Mutex::Autolock autoLock(mLock);
    if(!bOutputStreamOn){
        enum v4l2_buf_type buf_type = mCapBufType;
        if(0 == ioctl(mFd, VIDIOC_STREAMON, &buf_type)){
            bOutputStreamOn = true;
            ALOGV("%s OK", __FUNCTION__);
        }
    }
    return OK;
}
status_t V4l2Dec::stopOutputStream()
{
    ALOGV("%s", __FUNCTION__);
    Mutex::Autolock autoLock(mLock);
    //call VIDIOC_STREAMOFF and ignore the result
    enum v4l2_buf_type buf_type = mCapBufType;
    (void)ioctl(mFd, VIDIOC_STREAMOFF, &buf_type);
    ALOGV("%s OK", __FUNCTION__);
    bOutputStreamOn = false;

    // return capture buffer to component
    for(int32_t i = 0; i < mOutputBufferMap.size(); i++){
        if(mOutputBufferMap[i].planes[0].paddr > 0 && mOutputBufferMap[i].at_device) {
            GraphicBlockSetState(mOutputBufferMap[i].picture_id, GraphicBlockInfo::State::OWNED_BY_COMPONENT);
            mOutputBufferMap[i].at_device = false;
            ALOGV("return capture buffer %d ", mOutputBufferMap[i].picture_id);
        }
    }

    mVpuOwnedOutputBufferNum = 0;

    while(sem_trywait(&mSemaphore) == 0){
        ALOGV("sem_trywait again");
    };

    return OK;
}

status_t V4l2Dec::dequeueOutputBuffer()
{
    ATRACE_CALL();
    int result = 0;
    int byteused = 0;
    int64_t ts = 0;
    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[kOutputBufferPlaneNum];
    uint32_t seq = 0;
    uint32_t steps = 0;
    uint32_t id = 0;

    if(!bOutputStreamOn || mState != RUNNING)
        return OK;
    {
        Mutex::Autolock autoLock(mLock);

        if(!bOutputStreamOn || mState != RUNNING)
            return OK;

        if (bNewSegment)
            bNewSegment = false;

        memset(&stV4lBuf, 0, sizeof(stV4lBuf));
        memset(planes, 0, sizeof(planes));
        stV4lBuf.type = mCapBufType;
        stV4lBuf.memory = mOutMemType;

        if (V4L2_TYPE_IS_MULTIPLANAR(mCapBufType)) {
            stV4lBuf.m.planes = planes;
            stV4lBuf.length = kOutputBufferPlaneNum;
        }

        ALOGV("VIDIOC_DQBUF CAPTURE BEGIN");
        result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
        if(result < 0) {
            if(EPIPE == errno && bSawInputEos){
                handleDecEos();
                return OK;
            }else{
                ALOGW("%s VIDIOC_DQBUF err=%d", __FUNCTION__, errno);
                return UNKNOWN_ERROR;
            }
        }

        if(stV4lBuf.index >= kMaxOutputBufferCount) {
            ALOGI("dequeueOutputBuffer error");
            return BAD_INDEX;
        }

        if (V4L2_TYPE_IS_MULTIPLANAR(mCapBufType))
            byteused = stV4lBuf.m.planes[0].bytesused;
        else
            byteused = stV4lBuf.bytesused;

        // use input id instead of timestamp since mTsHandler will use ts manager to calculate it
        if (mTsHandler) {
            id = stV4lBuf.timestamp.tv_sec;
            mTsHandler->validTimestamp(id);
        } else {
            ts = (int64_t)stV4lBuf.timestamp.tv_sec *1000000;
            ts += stV4lBuf.timestamp.tv_usec;
        }

        mOutputBufferMap[stV4lBuf.index].at_device = false;
        mVpuOwnedOutputBufferNum--;
        mOutCnt ++;
        seq = stV4lBuf.sequence;
    }

    if(mLastOutputSeq <= seq)
        steps = seq - mLastOutputSeq;
    else//overflow, should never goes into here
        steps = 0xFFFFFFFF - mLastOutputSeq + seq;

    mLastOutputSeq = seq;

    if(steps > mOutputFormat.bufferNum)
        steps = 0;

    while(steps > 1){
        ALOGD("NotifySkipInputBuffer steps=%d",steps);
        NotifySkipInputBuffer(-1/*unused*/);
        if (mTsHandler)
            (void)mTsHandler->dropTimestampInc(1);
        steps --;
    }

    if (mTsHandler)
        ts = mTsHandler->popTimestamp();

    ALOGV("VIDIOC_DQBUF CAPTURE END index=%d ts=%lld byteused=%d flags=0x%x seq=%d id=%d",
            stV4lBuf.index, (long long)ts, byteused, stV4lBuf.flags, stV4lBuf.sequence, id);

    if(!bHasResChange && byteused > 0){
        ALOGD("bHasResChange FALSE trig source change");
        handleFormatChanged();
    }

    if (byteused > 0) {
        dumpOutputBuffer((void*)(uintptr_t)mOutputBufferMap[stV4lBuf.index].planes[0].vaddr, byteused);
        NotifyPictureReady(mOutputBufferMap[stV4lBuf.index].picture_id, (uint64_t)ts);
    } else {
        returnOutputBufferToDecoder(mOutputBufferMap[stV4lBuf.index].picture_id);
    }

    if(stV4lBuf.flags & V4L2_BUF_FLAG_LAST) {
        handleDecEos();
    }

    return OK;
}
status_t V4l2Dec::onDequeueEvent()
{
    int result = 0;
    struct v4l2_event event;
    memset(&event, 0, sizeof(struct v4l2_event));

    result = ioctl(mFd, VIDIOC_DQEVENT, &event);
    if(result == 0){
        ALOGD("onDequeueEvent type=%x",event.type);
        switch(event.type){
            case V4L2_EVENT_SOURCE_CHANGE:
                if(event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION){
                    if(STOPPING != mState){
                        handleFormatChanged();
                    }
                }
                break;
            default:
                break;
        }
    }

    return OK;
}

void V4l2Dec::handleDecEos() {
    ALOGI("decoder got output eos \n");
    if (STOPPED == mState)
        return; // already in stopped state

    if (STOPPING != mState)
        NotifyEOS();//send eos event

    mPoller->stopPolling();
}

status_t V4l2Dec::DoSetConfig(DecConfig index, void* pConfig) {
    if (!pConfig)
        return BAD_VALUE;

    status_t ret = OK;

    switch (index) {
        case DEC_CONFIG_VC1_SUB_FORMAT: {
            if (strcmp(mMime, MEDIA_MIMETYPE_VIDEO_VC1) != 0) {
                ALOGE("DoSetConfig DEC_CONFIG_VC1_SUB_FORMAT only support for VC1");
                return BAD_VALUE;
            }

            int* format = (int*)pConfig;
            if (*format == 3 /*eWMVFormat9*/)
                mVc1Format = V4L2_PIX_FMT_VC1_ANNEX_L;
            else if (*format == 5 /*eWMVFormatVC1*/)
                mVc1Format = V4L2_PIX_FMT_VC1_ANNEX_G;

            ALOGV("vc1 sub-format 0x%x mVc1Format %d", *format, mVc1Format);
            break;
        }
        case DEC_CONFIG_FORCE_PIXEL_FORMAT: {
            bForcePixelFormat = true;
            break;
        }
        case DEC_CONFIG_LOW_LATENCY: {
            int32_t* enable = (int32_t*)pConfig;
            ALOGV("set DEC_CONFIG_LOW_LATENCY enable=%d",*enable);
            if(1 == *enable)
                bLowLatency = true;
            else
                bLowLatency = false;
            break;
        }
        case DEC_CONFIG_SECURE_MODE: {
            if (*(int*)pConfig) {
                bSecureMode = true;
                nOutBufferUsage = (uint64_t)(GRALLOC_USAGE_PRIVATE_2 | C2MemoryUsage::READ_PROTECTED);
                if (OK != pDev->EnableSecureMode(bSecureMode))
                    ALOGW("EnableSecureMode failed");
            }
            break;
        }
        case DEC_CONFIG_INPUT_BUFFER_TYPE:{
            int* type = (int*)pConfig;
            if(*type == 1 || *type == 2)
                mInMemType = V4L2_MEMORY_MMAP;
            else
                mInMemType = V4L2_MEMORY_DMABUF;
            ALOGD("set mInMemType %d",mInMemType);
            break;
        }
        default:
            ret = BAD_VALUE;
            break;
    }
    return ret;
}

status_t V4l2Dec::DoGetConfig(DecConfig index, void* pConfig) {
    if (!pConfig)
        return BAD_VALUE;

    status_t ret = OK;

    switch (index) {
        case DEC_CONFIG_OUTPUT_DELAY: {
            int *pOutputDelayValue = (int*)pConfig;
            if(mOutputFormat.bufferNum > kDefaultOutputBufferCount)
                *pOutputDelayValue = mOutputFormat.bufferNum;
            else
                *pOutputDelayValue = kDefaultOutputBufferCount;
            ALOGV("DoGetConfig DEC_CONFIG_OUTPUT_DELAY =%d, mOutputFormat.bufferNum=%d",
                *pOutputDelayValue,mOutputFormat.bufferNum);
            break;
        }
        case DEC_CONFIG_INPUT_DELAY: {
            int *pInputDelayValue = (int*)pConfig;
            *pInputDelayValue = mInputFormat.bufferNum;
            break;
        }
        case DEC_CONFIG_COLOR_ASPECTS:{
            if (!bHasColorAspect)
                return BAD_VALUE;
            DecIsoColorAspects* isocolor = (DecIsoColorAspects*)pConfig;

            isocolor->colourPrimaries = mIsoColorAspect.colourPrimaries;
            isocolor->transferCharacteristics = mIsoColorAspect.transferCharacteristics;
            isocolor->matrixCoeffs = mIsoColorAspect.matrixCoeffs;
            isocolor->fullRange = mIsoColorAspect.fullRange;
            break;
        }
        case DEC_CONFIG_HDR10_STATIC_INFO: {
            if (bHasHdr10StaticInfo | bHasColorAspect)
                memcpy(pConfig, &sHdr10StaticInfo, sizeof(DecStaticHDRInfo));
            else
                ret = BAD_VALUE;
            break;
        }
        case DEC_CONFIG_SECURE_BUFFER_MODE: {
            uint32_t *mode = (uint32_t*)pConfig;
            #ifdef HANTRO_V4L2
            *mode = 2;//C2Config::SM_READ_PROTECTED_WITH_ENCRYPTED;
            #else
            //TODO: set securty buffer type for other vpu decoder
            *mode = 1;//C2Config::SM_UNPROTECTED;
            #endif
            break;
        }
        case DEC_CONFIG_OUTPUT_UV_OFFSET: {
            uint32_t *value = (uint32_t*)pConfig;
            *value = mUVOffset;
            break;
        }
        default:
            ret = BAD_VALUE;
            break;
    }

    return ret;
}

status_t V4l2Dec::freeOutputBuffers() {

    ALOGV("%s", __FUNCTION__);
    Mutex::Autolock autoLock(mLock);

    if(mGraphicBlocks.empty())
        return OK;

    for (auto& info : mGraphicBlocks) {
        ALOGV("freeOutputBuffers fd=%d,id=%d",info.mDMABufFd,info.mBlockId);

        if (info.mVirtAddr > 0 && info.mCapacity > 0)
            munmap((void*)info.mVirtAddr, info.mCapacity);

        if (info.mGraphicBlock.get()) {
            ALOGV("info.mGraphicBlock reset");
            info.mGraphicBlock.reset();
        }
    }

    mGraphicBlocks.clear();
    return OK;
}

status_t V4l2Dec::handleFormatChanged() {
    ATRACE_CALL();
    status_t ret = OK;
    int pre_state;
    Mutex::Autolock autoThreadLock(mThreadLock);
    ALOGV("outputFormatChanged BEGIN");
    {
        Mutex::Autolock autoLock(mLock);

        if (bNewSegment)
            bNewSegment = false;

        pre_state = mState;
        mState = RES_CHANGING;
    }
    // stop fetch thread & capture stream, destroy output buffers
    if (bFetchStarted) {
        destroyFetchThread();
    }
    if (bOutputStreamOn) {
        stopOutputStream();
    }
    destroyOutputBuffers();

    ret = getOutputParams();
    if(ret){
        if(ret == BAD_VALUE){
            bReceiveError = true;
            ALOGE("output params not support");
            NotifyError(UNKNOWN_ERROR);
        }
        return ret;
    }

    if(pre_state == STOPPING){
        ALOGI("do not handle resolution while stopping");
        return OK;
    }

    SetOutputFormats();
    outputFormatChanged();

    {
        Mutex::Autolock autoLock(mLock);
        mState = RES_CHANGED;
    }

    ALOGV("outputFormatChanged end");
    return OK;
}
status_t V4l2Dec::getOutputParams()
{

    Mutex::Autolock autoLock(mLock);
    status_t ret = OK;
    int result = 0;
    struct v4l2_format format;
    uint32_t pixel_format = 0;
    uint32_t v4l2_pixel_format = 0;
    uint32_t newWidth, newHeight, newBytesperline;
    memset(&format, 0, sizeof(struct v4l2_format));

    format.type = mCapBufType;
    result = ioctl (mFd, VIDIOC_G_FMT, &format);

    if(result < 0)
        return BAD_VALUE;

    if (V4L2_TYPE_IS_MULTIPLANAR(mCapBufType)) {
        if (format.fmt.pix_mp.num_planes > 1) {
            uint32_t contiguous_fmt;
            if (pDev->GetContiguousV4l2Format(format.fmt.pix_mp.pixelformat, &contiguous_fmt) != OK) {
                ALOGE("can't support noncontiguous format 0x%x", format.fmt.pix_mp.pixelformat);
                return BAD_VALUE;
            }
            format.fmt.pix_mp.num_planes = 1;
            format.fmt.pix_mp.pixelformat = contiguous_fmt;
            if (ioctl (mFd, VIDIOC_S_FMT, &format) < 0 || ioctl (mFd, VIDIOC_G_FMT, &format) < 0) {
                ALOGE("change to contiguous format failed");
                return BAD_VALUE;
            }
        }
        v4l2_pixel_format = format.fmt.pix_mp.pixelformat;
        newWidth = format.fmt.pix_mp.width;
        newHeight = format.fmt.pix_mp.height;
        newBytesperline = format.fmt.pix_mp.plane_fmt[0].bytesperline;
        mOutputPlaneSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
        mOutputFormat.bufferSize = mOutputPlaneSize[0];
        mOutputFormat.interlaced = ((format.fmt.pix_mp.field == V4L2_FIELD_SEQ_TB) ? true: false);

        ret = pDev->GetColorAspectsInfo(format.fmt.pix_mp.colorspace,
                                   format.fmt.pix_mp.xfer_func,
                                   format.fmt.pix_mp.ycbcr_enc,
                                   format.fmt.pix_mp.quantization,
                                   &mIsoColorAspect);
    } else {
        v4l2_pixel_format = format.fmt.pix.pixelformat;
        newWidth = format.fmt.pix.width;
        newHeight = format.fmt.pix.height;
        newBytesperline = format.fmt.pix.bytesperline;
        mOutputPlaneSize[0] = format.fmt.pix.sizeimage;
        mOutputFormat.bufferSize = mOutputPlaneSize[0];
        mOutputFormat.interlaced = false;

        ret = pDev->GetColorAspectsInfo(format.fmt.pix.colorspace,
                                   format.fmt.pix.xfer_func,
                                   format.fmt.pix.ycbcr_enc,
                                   format.fmt.pix.quantization,
                                   &mIsoColorAspect);
    }

    if (OK == ret)
        bHasColorAspect = true;

    bool forceNV12 = false;

#ifdef CUT_10BIT_TO_8BIT
    forceNV12 = true;
#else
    if (mOutFormat == V4L2_PIX_FMT_NV12 && bForcePixelFormat)
        forceNV12 = true;
#endif

    if (forceNV12 && V4L2_PIX_FMT_NV12X == v4l2_pixel_format) {
        v4l2_pixel_format = V4L2_PIX_FMT_NV12;
        mOutputPlaneSize[0] = newWidth * newHeight * 3 / 2;
        mOutputFormat.bufferSize = mOutputPlaneSize[0];
    }

#if 0
    //test for HAL_PIXEL_FORMAT_YCBCR_P010 output
    if(V4L2_PIX_FMT_NV12X == v4l2_pixel_format){
        v4l2_pixel_format = V4L2_PIX_FMT_P010;
    }
#endif
    ret = pDev->GetColorFormatByV4l2(v4l2_pixel_format, &pixel_format);
    if(ret != OK)
        return ret;

    mOutFormat = v4l2_pixel_format;
    mOutputFormat.pixelFormat = static_cast<int>(pixel_format);

#ifdef AMPHION_V4L2
    if(mOutputFormat.interlaced)
        mFrameAlignH = AMPHION_FRAME_ALIGN_HEIGHT * 4;
#endif

    if(newWidth > MAX_FRM_WIDTH || newHeight > MAX_FRM_HEIGHT)
        return BAD_VALUE;

    if(pDev->Is10BitV4l2Format(mOutFormat)){
        mOutputFormat.width = newWidth;
        mOutputFormat.height = Align(newHeight, mFrameAlignH);
        mOutputFormat.stride = Align(mOutputFormat.width*5/4, mFrameAlignW);
    }
    else{
        mOutputFormat.width = Align(newWidth, mFrameAlignW);
        mOutputFormat.height = Align(newHeight, mFrameAlignH);
        mOutputFormat.stride = mOutputFormat.width;
    }

    //for 10bit video, stride is larger than width, should use stride to allocate buffer
    if(mOutputFormat.width < newBytesperline){
        mOutputFormat.stride = newBytesperline;
    }

    // update capture buffer count
    getV4l2MinBufferCount(mCapBufType);

#ifdef AMPHION_V4L2
    mOutputFormat.bufferNum += 1;
    if(bAdaptiveMode && mOutputFormat.bufferNum < mMaxOutputBufferCnt)
        mOutputFormat.bufferNum = mMaxOutputBufferCnt;

#endif
#ifdef HANTRO_V4L2
    // vsi vpu need more buffer because:
    // 1. 4K HDR10 video reach performance
    // 2. pass android.media.cts.MediaCodecPlayerTest#testPlaybackSwitchViews
    mOutputFormat.bufferNum += FRAME_SURPLUS;

    // surfaceflinger do sw csc for 422sp, need cached buffer to improve performance
    if(mOutputFormat.pixelFormat == HAL_PIXEL_FORMAT_YCbCr_422_SP){
        ALOGI("YUV422SP: use cached buffer");
        nOutBufferUsage |= C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE;
    }
#endif

    if(mOutputFormat.bufferNum > kMaxOutputBufferCount)
        mOutputFormat.bufferNum = kMaxOutputBufferCount;

    // query hdr10 meta
    struct v4l2_ext_control ctrl;
    struct v4l2_ext_controls ctrls;
    struct v4l2_hdr10_meta hdr10meta;
    memset(&hdr10meta, 0, sizeof(struct v4l2_hdr10_meta));

    ctrls.controls = &ctrl;
    ctrls.count = 1;
    ctrl.id = V4L2_CID_HDR10META;
    ctrl.ptr = (void *)&hdr10meta;
    ctrl.size = sizeof(struct v4l2_hdr10_meta);
    result = ioctl(mFd, VIDIOC_G_EXT_CTRLS, &ctrls);
    if(0 == result && hdr10meta.hasHdr10Meta) {
        ALOGV("has hdr10 meta");
        bHasHdr10StaticInfo = true;
        sHdr10StaticInfo.mR[0] = (uint16_t)hdr10meta.redPrimary[0];
        sHdr10StaticInfo.mR[1] = (uint16_t)hdr10meta.redPrimary[1];
        sHdr10StaticInfo.mG[0] = (uint16_t)hdr10meta.greenPrimary[0];
        sHdr10StaticInfo.mG[1] = (uint16_t)hdr10meta.greenPrimary[1];
        sHdr10StaticInfo.mB[0] = (uint16_t)hdr10meta.bluePrimary[0];
        sHdr10StaticInfo.mB[1] = (uint16_t)hdr10meta.bluePrimary[1];
        sHdr10StaticInfo.mW[0] = (uint16_t)hdr10meta.whitePoint[0];
        sHdr10StaticInfo.mW[1] = (uint16_t)hdr10meta.whitePoint[1];
        sHdr10StaticInfo.mMaxDisplayLuminance = (uint16_t)(hdr10meta.maxMasteringLuminance/10000);
        sHdr10StaticInfo.mMinDisplayLuminance = (uint16_t)hdr10meta.minMasteringLuminance;
        sHdr10StaticInfo.mMaxContentLightLevel = (uint16_t)hdr10meta.maxContentLightLevel;
        sHdr10StaticInfo.mMaxFrameAverageLightLevel = (uint16_t)hdr10meta.maxFrameAverageLightLevel;
    }

    struct v4l2_selection sel;
    sel.type = mCapBufType;
    sel.target = V4L2_SEL_TGT_COMPOSE;

    result = ioctl (mFd, VIDIOC_G_SELECTION, &sel);
    if(result < 0) {
        ALOGE("g_selection fail, result=%d", result);
        return BAD_VALUE;
    }

    //seems decoder just be flushed
    if(sel.r.width == 0 && sel.r.height == 0){
        ALOGE("handleFormatChanged flushed return");
        return UNKNOWN_ERROR;
    }

    mOutputFormat.rect.right = sel.r.width;
    mOutputFormat.rect.bottom = sel.r.height;
    mOutputFormat.rect.top = sel.r.top;
    mOutputFormat.rect.left = sel.r.left;

    if(mOutputFormat.rect.right > MAX_FRM_WIDTH || mOutputFormat.rect.bottom > MAX_FRM_HEIGHT)
        return BAD_VALUE;

    if (mOutputFormat.pixelFormat != HAL_PIXEL_FORMAT_YCbCr_422_I) {
        if (mOutputFormat.interlaced) {
            uint32_t height = mOutputFormat.rect.bottom;
            mUVOffset = mOutputFormat.stride * Align(height, mFrameAlignH >> 1);
        } else
            mUVOffset = mOutputFormat.stride * mOutputFormat.height;
    } else
        mUVOffset = 0;

    ALOGD("outputFormatChanged w=%d,h=%d,s=%d minBufferNum=%d, bufferNum=%d, mOutputPlaneSize[0]=%d,, pixelFormat=0x%x",
        mOutputFormat.width, mOutputFormat.height,mOutputFormat.stride,
        mOutputFormat.minBufferNum, mOutputFormat.bufferNum,
        mOutputPlaneSize[0], mOutputFormat.pixelFormat);

    ALOGV("mIsoColorAspect c=%d,t=%d,m=%d,f=%d",mIsoColorAspect.colourPrimaries, mIsoColorAspect.transferCharacteristics,mIsoColorAspect.matrixCoeffs, mIsoColorAspect.fullRange);

    bHasResChange = true;

    return OK;
}

status_t V4l2Dec::onFlush()
{
    ATRACE_CALL();
    Mutex::Autolock autoThreadLock(mThreadLock);
    ALOGV("%s", __FUNCTION__);
    int pre_state;

    mLock.lock();
    // MA-20706 wait res change to turn on capture stream
    if (mState == RES_CHANGING || mState == RES_CHANGED) {
        bPendingFlush = true;
        mLock.unlock();
        DecWaitSem(&mSemaphore, 3/*timeout*/);
        mLock.lock();
    }
    pre_state = mState;
    if(mState != STOPPING)
        mState = FLUSHING;
    mLock.unlock();

    status_t ret = UNKNOWN_ERROR;
    bool inputStreamOn = bInputStreamOn;

    if(mPoller && mPoller->isPolling())
        mPoller->stopPolling();

    ret = stopInputStream();
    if(ret != OK)
        return ret;

    ret = stopOutputStream();
    if(ret != OK)
        return ret;

    {
        Mutex::Autolock autoLock(mLock);
        if(mState != STOPPED)
            mState = pre_state;
    }

    if (mTsHandler)
        mTsHandler->flushTimestamp();
    
    mCodecDataHandler->flush();

    mInCnt = 0;
    mOutCnt = 0;
    mLastOutputSeq = 0;
    bReceiveError = false;
    bSawInputEos = false;
    bNewSegment = true;
    bPendingFlush = false;

    // don't stream on capture here because it's stream on only after capture qbuf
    // stream on output if needed, wait until capture stream on.
    if (inputStreamOn) {
        (void)startInputStream();
    }

    ALOGV("%s end", __FUNCTION__);
    return ret;
}
status_t V4l2Dec::onStop()
{
    ATRACE_CALL();
    ALOGV("%s", __FUNCTION__);
    status_t ret = OK;
    Mutex::Autolock autoThreadLock(mThreadLock);
    {
        Mutex::Autolock autoLock(mLock);
        mState = STOPPING;
    }

    if(mPoller && mPoller->isPolling())
        mPoller->stopPolling();

    ret |= stopInputStream();

    ret |= stopOutputStream();

    if (mTsHandler)
        mTsHandler->flushTimestamp();

    mInCnt = 0;
    mOutCnt = 0;
    mLastOutputSeq = 0;
    bReceiveError = false;
    bSawInputEos = false;
    bNewSegment = true;
    bPendingFlush = false;

    // don't exit halfway, try to execute till end to avoid memory leak
    ret |= destroyFetchThread();

    ret |= destroyInputBuffers();

    ret |= destroyOutputBuffers();

    ret |= freeOutputBuffers();

    if (ret)
        ALOGW("%s ret=%d", __func__, ret);

    {
        Mutex::Autolock autoLock(mLock);
        mState = STOPPED;
    }

    mCodecDataHandler->clear();

    if(pDev != NULL)
        pDev->ResetDecoder();
    ALOGV("%s end", __FUNCTION__);
    return OK;
}
status_t V4l2Dec::onDestroy()
{
    ATRACE_CALL();
    status_t ret = UNKNOWN_ERROR;
    ALOGV("%s", __FUNCTION__);

    if(pDev == NULL)
        return UNKNOWN_ERROR;

    if(mState != STOPPED){
        onStop();
    }

    Mutex::Autolock autoLock(mLock);

    if(mFd > 0){
        pDev->Close();
        ALOGV("pDev->Close %d",mFd);
        mFd = 0;
    }

    if(pDev != NULL)
        delete pDev;
    pDev = NULL;

    if (mPoller) {
        mPoller.clear();
    }

    if (mTsHandler) {
        delete mTsHandler;
    }

    if (0 != sem_destroy(&mSemaphore))
        ALOGW("%s: sem_destroy failed", __func__);

    ALOGV("%s end", __FUNCTION__);
    return OK;
}

void V4l2Dec::ParseVpuLogLevel()
{
    int level=0;
    FILE* fpVpuLog;
    nDebugFlag = 0;

    fpVpuLog=fopen(VPU_DECODER_LOG_LEVELFILE, "r");
    if (NULL==fpVpuLog){
        return;
    }

    char symbol;
    int readLen = 0;

    readLen = fread(&symbol,1,1,fpVpuLog);
    if(feof(fpVpuLog) != 0){
        ;
    }
    else{
        level=atoi(&symbol);
        if((level<0) || (level>255)){
            level=0;
        }
    }
    fclose(fpVpuLog);

    nDebugFlag=level;

    if(nDebugFlag != 0)
        ALOGV("ParseVpuLogLevel nDebugFlag=%x",nDebugFlag);
    return;
}
void V4l2Dec::dumpInputBuffer(void* inBuf, uint32_t size)
{
    FILE * pfile = NULL;

    if(!(nDebugFlag & DUMP_DEC_FLAG_INPUT))
        return;

    if(!inBuf){
        ALOGV("dumpInputBuffer invalid fd");
        return;
    }

    pfile = fopen(DUMP_DEC_INPUT_FILE,"ab");

    if(pfile){

        if(mInFormat == V4L2_PIX_FMT_VP8 || mInFormat == V4L2_PIX_FMT_VP9){
            if(mInCnt == 0){
                char header[32];
                if(OK == getVPXHeader(mInputFormat.width, mInputFormat.width, mInFormat, &header[0]))
                    fwrite(&header,1,32,pfile);
            }
            uint64_t timestamp = 0;
            fwrite(&size,1,sizeof(uint32_t),pfile);
            fwrite(&timestamp,1,sizeof(uint64_t),pfile);
        }

        fwrite(inBuf,1,size,pfile);
        ALOGV("dumpInputBuffer write %d",size);
        fclose(pfile);
    }else
        ALOGV("dumpInputBuffer failed to open %s",DUMP_DEC_INPUT_FILE);
    return;
}
void V4l2Dec::dumpInputBuffer(int fd, uint32_t size)
{
    if(!(nDebugFlag & DUMP_DEC_FLAG_INPUT))
        return;

    FILE * pfile = NULL;
    void* buf = NULL;

    if(fd < 0 || size == 0){
        ALOGV("dumpBuffer invalid parameters");
        return;
    }

    pfile = fopen(DUMP_DEC_INPUT_FILE,"ab");

    if(pfile){
        if(mInFormat == V4L2_PIX_FMT_VP8 || mInFormat == V4L2_PIX_FMT_VP9){
            if(mInCnt == 0){
                char header[32];
                if(OK == getVPXHeader(mInputFormat.width, mInputFormat.width, mInFormat, &header[0]))
                    fwrite(&header,1,32,pfile);
            }
            uint64_t timestamp = 0;
            fwrite(&size,1,sizeof(uint32_t),pfile);
            fwrite(&timestamp,1,sizeof(uint64_t),pfile);
        }

        buf = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
        fwrite(buf,1,size,pfile);
        munmap(buf, size);
        fclose(pfile);
    }else
        ALOGD("dumpBuffer failed to open INPUT_FILE");

    return;
}

void V4l2Dec::dumpOutputBuffer(void* inBuf, uint32_t size)
{
    FILE * pfile = NULL;

    if(!(nDebugFlag & DUMP_DEC_FLAG_OUTPUT))
        return;

    if(!inBuf){
        ALOGV("dumpOutputBuffer invalid fd");
        return;
    }

    pfile = fopen(DUMP_DEC_OUTPUT_FILE,"ab");

    if(pfile){
        fwrite(inBuf,1,size,pfile);
        ALOGV("dumpOutputBuffer write %d",size);
        fclose(pfile);
    }else
        ALOGV("dumpOutputBuffer failed to open %s",DUMP_DEC_OUTPUT_FILE);
    return;
}
bool V4l2Dec::OutputBufferFull() {
    if(mRegisteredOutBufNum >= mOutputFormat.bufferNum)
        return true;
    return false;
}

void V4l2Dec::migrateOutputBuffers()
{
    // only migrate output buffers allocated by BufferQueue
    if (!bNewSegment)
        return;
    {
        Mutex::Autolock autoLock(mLock);
        if (mState == STOPPING)
            return;
    }

    ALOGI("migrateOutputBuffers...");

    // 1. migrate mGraphicBlocks
    int32_t migrateBufNum = migrateGraphicBuffers();
    if (mVpuOwnedOutputBufferNum != migrateBufNum)
        ALOGW("migrateGraphicBuffers mismatch, own %d , migrate %d", mVpuOwnedOutputBufferNum, migrateBufNum);

    mRegisteredOutBufNum = 0;

    // 2. migrate outputbuffermap
    for (auto& outRecord : mOutputBufferMap) {
        GraphicBlockInfo *gbInfo = nullptr;
        uint64_t paddr = outRecord.planes[0].paddr;

        gbInfo = getGraphicBlockByPhysAddr(paddr);
        if (gbInfo) {
            ALOGD("migrate outputbuffermap %d -> %d ", outRecord.picture_id, gbInfo->mBlockId);
            outRecord.picture_id = gbInfo->mBlockId;
            ++mRegisteredOutBufNum;
        } else {
            memset(&outRecord, 0, sizeof(OutputRecord));
        }
    }

    bNewSegment = false;
}
bool V4l2Dec::allInputBuffersInVPU(){

    if(bInputStreamOn && mFreeInputIndex.empty())
        return true;

    return false;
}

bool V4l2Dec::isDeviceHang(uint32_t pollTimeOutCnt) {
    //detect vpu hang
    if (pollTimeOutCnt > 10 && allInputBuffersInVPU() && mVpuOwnedOutputBufferNum >= mOutputFormat.minBufferNum) {
        return true;
    }

    return false;
}

bool V4l2Dec::isDeviceEos() {
    return (bSawInputEos && mInCnt <= 1);
}
status_t V4l2Dec::getVPXHeader(uint32_t width, uint32_t height, uint32_t format, char* header){
    if(format != V4L2_PIX_FMT_VP8 && format != V4L2_PIX_FMT_VP9)
        return BAD_TYPE;
    if(!header)
        return BAD_TYPE;

    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';

    header[4] = 0x0;
    header[5] = 0x0;

    header[6] = 0x20;
    header[7] = 0x00;

    header[8] = 'V';
    header[9] = 'P';
    if(format == V4L2_PIX_FMT_VP8)
        header[10] = '8';
    else
        header[10] = '9';
    header[11] = '0';

    header[12] = width;
    header[13] = width >> 8;

    header[14] = height;
    header[15] = height >> 8;

    header[16] = 0x1E;
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;

    header[16] = 0x1E;
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;

    header[20] = 0x01;
    header[21] = 0x00;
    header[22] = 0x00;
    header[23] = 0x00;

    header[24] = 0xC8;
    header[25] = 0x00;
    header[26] = 0x00;
    header[27] = 0x00;

    return OK;
}

VideoDecoderBase * CreateVideoDecoderInstance(const char* mime) {
    return static_cast<VideoDecoderBase *>(new V4l2Dec(mime));
}
}
