/**
 *  Copyright 2019-2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "V4l2Enc"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>

#include "V4l2Enc.h"
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <C2Config.h>
#include "graphics_ext.h"
#include <sys/mman.h>
#include "C2_imx.h"
#include "IMXUtils.h"

#include <linux/imx_vpu.h>

namespace android {

#define VPU_ENCODER_LOG_LEVELFILE "/data/vpu_enc_level"
#define DUMP_ENC_INPUT_FILE "/data/temp_enc_in.yuv"
#define DUMP_ENC_OUTPUT_FILE "/data/temp_enc_out.bit"

#define DUMP_ENC_FLAG_INPUT     0x1
#define DUMP_ENC_FLAG_OUTPUT    0x2

#define Align(ptr,align)    (((uint32_t)(ptr)+(align)-1)/(align)*(align))
#define FRAME_ALIGN     (2)

#define DEFAULT_INPUT_BUFFER_COUNT (16)

typedef struct {
    int32_t c2Value;
    int32_t v4l2Value;
} C2V4l2Map;

static const C2V4l2Map HevcProfileMapTable[] = {
    {PROFILE_HEVC_MAIN,       V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN},
    {PROFILE_HEVC_MAIN_10,    V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10},
    {PROFILE_HEVC_MAIN_STILL, V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE},
};

static const C2V4l2Map HevcLevelMapTable[] = {
    {LEVEL_HEVC_MAIN_1,   V4L2_MPEG_VIDEO_HEVC_LEVEL_1},
    {LEVEL_HEVC_MAIN_2,   V4L2_MPEG_VIDEO_HEVC_LEVEL_2},
    {LEVEL_HEVC_MAIN_2_1, V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1},
    {LEVEL_HEVC_MAIN_3,   V4L2_MPEG_VIDEO_HEVC_LEVEL_3},
    {LEVEL_HEVC_MAIN_3_1, V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1},
    {LEVEL_HEVC_MAIN_4,   V4L2_MPEG_VIDEO_HEVC_LEVEL_4},
    {LEVEL_HEVC_MAIN_4_1, V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1},
    {LEVEL_HEVC_MAIN_5,   V4L2_MPEG_VIDEO_HEVC_LEVEL_5},
    {LEVEL_HEVC_MAIN_5_1, V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1},
};

static const C2V4l2Map H264ProfileMapTable[] = {
    {PROFILE_AVC_BASELINE,             V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE},
    {PROFILE_AVC_CONSTRAINED_BASELINE, V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE},
    {PROFILE_AVC_MAIN,                 V4L2_MPEG_VIDEO_H264_PROFILE_MAIN},
    {PROFILE_AVC_HIGH,                 V4L2_MPEG_VIDEO_H264_PROFILE_HIGH},
};

static const C2V4l2Map H264LevelMapTable[] = {
    {LEVEL_AVC_1,   V4L2_MPEG_VIDEO_H264_LEVEL_1_0},
    {LEVEL_AVC_1B,  V4L2_MPEG_VIDEO_H264_LEVEL_1B},
    {LEVEL_AVC_1_1, V4L2_MPEG_VIDEO_H264_LEVEL_1_1},
    {LEVEL_AVC_1_2, V4L2_MPEG_VIDEO_H264_LEVEL_1_2},
    {LEVEL_AVC_1_3, V4L2_MPEG_VIDEO_H264_LEVEL_1_3},
    {LEVEL_AVC_2,   V4L2_MPEG_VIDEO_H264_LEVEL_2_0},
    {LEVEL_AVC_2_1, V4L2_MPEG_VIDEO_H264_LEVEL_2_1},
    {LEVEL_AVC_2_2, V4L2_MPEG_VIDEO_H264_LEVEL_2_2},
    {LEVEL_AVC_3,   V4L2_MPEG_VIDEO_H264_LEVEL_3_0},
    {LEVEL_AVC_3_1, V4L2_MPEG_VIDEO_H264_LEVEL_3_1},
    {LEVEL_AVC_3_2, V4L2_MPEG_VIDEO_H264_LEVEL_3_2},
    {LEVEL_AVC_4,   V4L2_MPEG_VIDEO_H264_LEVEL_4_0},
    {LEVEL_AVC_4_1, V4L2_MPEG_VIDEO_H264_LEVEL_4_1},
    {LEVEL_AVC_4_2, V4L2_MPEG_VIDEO_H264_LEVEL_4_2},
    {LEVEL_AVC_5,   V4L2_MPEG_VIDEO_H264_LEVEL_5_0},
    {LEVEL_AVC_5_1, V4L2_MPEG_VIDEO_H264_LEVEL_5_1},
};

static int32_t C2ToV4l2ProfileLevel(int32_t c2value, const char* mime, bool isProfile)
{
    int i, tableLen;
    const C2V4l2Map * pTable;

    if (!strcmp(mime, MEDIA_MIMETYPE_VIDEO_HEVC)) {
        if (isProfile) {
            pTable = HevcProfileMapTable;
            tableLen = sizeof(HevcProfileMapTable)/sizeof(HevcProfileMapTable[0]);
        } else {
            pTable = HevcLevelMapTable;
            tableLen = sizeof(HevcLevelMapTable)/sizeof(HevcLevelMapTable[0]);
        }
    } else if (!strcmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
        if (isProfile) {
            pTable = H264ProfileMapTable;
            tableLen = sizeof(H264ProfileMapTable)/sizeof(H264ProfileMapTable[0]);
        } else {
            pTable = H264LevelMapTable;
            tableLen = sizeof(H264LevelMapTable)/sizeof(H264LevelMapTable[0]);
        }
    } else
        return 0;

    for (i = 0; i < tableLen; i++) {
        if (pTable[i].c2Value == c2value)
            return pTable[i].v4l2Value;
    }

    ALOGW("Can't convert c2 value %d, return a lowest value as default", c2value);
    return pTable[0].v4l2Value;
}

static int32_t C2ToV4l2BitrateMode(int32_t c2value)
{
#ifdef AMPHION_V4L2
    // amphion don't support VBR
    (void)c2value;
    return V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
#else
    switch (c2value) {
        case C2Config::BITRATE_CONST:
            return V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
        case C2Config::BITRATE_IGNORE:
            return V4L2_MPEG_VIDEO_BITRATE_MODE_CQ;
        case C2Config::BITRATE_VARIABLE:
            // fallthrough
        default:
            return V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
    }
#endif
}

/* table for max frame size for each video level */
typedef struct {
    int level;
    int size;    // mbPerFrame, (Width / 16) * (Height / 16)
}EncLevelSizeMap;

/* H264 level size map table, sync with h1 h264 encoder */
static const EncLevelSizeMap H264LevelSizeMapTable[] = {
    {LEVEL_AVC_1,   99},
    {LEVEL_AVC_1B,  99},
    {LEVEL_AVC_1_1, 396},
    {LEVEL_AVC_1_2, 396},
    {LEVEL_AVC_1_3, 396},
    {LEVEL_AVC_2,   396},
    {LEVEL_AVC_2_1, 792},
    {LEVEL_AVC_2_2, 1620},
    {LEVEL_AVC_3,   1620},
    {LEVEL_AVC_3_1, 3600},
    {LEVEL_AVC_3_2, 5120},
    {LEVEL_AVC_4,   8192},
    {LEVEL_AVC_4_1, 8192},
    {LEVEL_AVC_4_2, 8704},
    {LEVEL_AVC_5,   22080},
    {LEVEL_AVC_5_1, 65025},
};

static uint32_t CheckH264Level(uint32_t width, uint32_t height, uint32_t level)
{
    // adjust H264 level based on video resolution because h1 encoder will check this.
    int i, mbPerFrame, tableLen;
    mbPerFrame = ((width + 15) / 16) * ((height + 15) / 16);
    tableLen = sizeof(H264LevelSizeMapTable)/sizeof(H264LevelSizeMapTable[0]);

    for (i = 0; i < tableLen; i++) {
        if (level == H264LevelSizeMapTable[i].level) {
          if (mbPerFrame <= H264LevelSizeMapTable[i].size)
            break;
        else if (i + 1 < tableLen)
            level = H264LevelSizeMapTable[i + 1].level;
        else
            break;
      }
    }

    return level;
}

V4l2Enc::V4l2Enc(const char* mime):
    mMime(mime),
    mPollThread(0),
    pDev(NULL),
    mFd(-1),
    mTargetFps(0),
    mInFormat(V4L2_PIX_FMT_NV12),
    mOutFormat(V4L2_PIX_FMT_H264),
    mWidthAlign(0),
    mHeightAlign(0),
    bPollStarted(false),
    bInputStreamOn(false),
    bOutputStreamOn(false),
    bSyncFrame(false),
    bHasCodecData(false),
    bStarted(false),
    mFrameInNum(0),
    mFrameOutNum(0),
    nDebugFlag(0){

    mInputFormat.bufferNum = DEFAULT_INPUT_BUFFER_COUNT;
    mInputFormat.bufferSize = DEFAULT_FRM_WIDTH * DEFAULT_FRM_HEIGHT * 3/2;
    mInputFormat.width = DEFAULT_FRM_WIDTH;
    mInputFormat.height = DEFAULT_FRM_HEIGHT;
    mInputFormat.interlaced = false;
    mInputFormat.pixelFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;

    mOutputFormat.width = DEFAULT_FRM_WIDTH;
    mOutputFormat.height = DEFAULT_FRM_HEIGHT;
    mOutputFormat.minBufferNum = 2;
    mOutputFormat.bufferNum = kDefaultOutputBufferCount;
    mOutputFormat.interlaced = false;

    mCropWidth = mInputFormat.width;
    mCropHeight = mInputFormat.height;

    mInBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    mOutBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    //V4L2_MEMORY_USERPTR; V4L2_MEMORY_DMABUF; V4L2_MEMORY_MMAP
    mInMemType = V4L2_MEMORY_DMABUF;
    mOutMemType = V4L2_MEMORY_MMAP;

    mInPlaneNum = kInputBufferPlaneNum;
    mOutPlaneNum = kOutputBufferPlaneNum;

    memset(&mEncParam, 0, sizeof(V4l2EncInputParam));
    memset(&mIsoColorAspect, 0, sizeof(VideoColorAspect));
    memset(&mInputPlaneSize[0], 0, kMaxInputBufferPlaneNum * sizeof(uint32_t));
    bNeedFrameConverter = false;
    mConverter = NULL;

    bPreProcess = false;
    ALOGV("mMime=%s",mMime);
    mState = UNINITIALIZED;

}
V4l2Enc::~V4l2Enc()
{
    ALOGV("V4l2Enc::~V4l2Enc");
    if(pDev != NULL)
        onStop();

    onDestroy();
}

status_t V4l2Enc::OpenDevice() {
    status_t ret = UNKNOWN_ERROR;

    if(pDev == NULL){
        pDev = new V4l2Dev();
    }
    if(pDev == NULL)
        return ret;

    mFd = pDev->Open(V4L2_DEV_ENCODER);
    ALOGV("onInit pV4l2Dev->Open fd=%d",mFd);

    if(mFd < 0)
        return ret;

    return OK;
}
status_t V4l2Enc::onInit(){
    status_t ret = UNKNOWN_ERROR;

    ret = pDev->GetVideoBufferType(&mInBufType, &mOutBufType);
    if (ret != OK)
        return ret;

    if(!strcmp(mMime, "video/avc") || !strcmp(mMime, "video/hevc"))
        bNeedFrameConverter = true;

    mFrameInNum = 0;
    mFrameOutNum = 0;

    ret = prepareOutputParams();

    ParseVpuLogLevel();

    if(bNeedFrameConverter){
        mConverter = new FrameConverter();
        if(mConverter == NULL){
            ret = UNKNOWN_ERROR;
            return ret;
        }
        ALOGV("NEW FrameConverter");

        ret = mConverter->Create(mOutFormat);
        if (ret != OK) {
            ALOGE("create converter failed, mOutFormat 0x%x", mOutFormat);
            return ret;
        }
    }

    ret = SetOutputFormats();
    if(ret != OK)
        return ret;

    ret = prepareOutputBuffers();
    ALOGV("onStart prepareInputBuffers ret=%d",ret);
    if(ret != OK)
        return ret;

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

    mEncParam.format = mOutFormat;
    ret = pDev->SetEncoderParam(&mEncParam);
    if(ret != OK){
        ALOGE("SetEncoderParam failed");
        return ret;
    }

    ret = pDev->SetFrameRate(mTargetFps);
    if(ret != OK){
        ALOGE("SetFrameRate failed");
        return ret;
    }

    mState = PREPARED;

    return ret;
}
status_t V4l2Enc::onStart()
{
    status_t ret = UNKNOWN_ERROR;

    bPollStarted = false;
    bInputStreamOn = false;
    bOutputStreamOn = false;
    bSyncFrame = false;
    bHasCodecData = false;

    mFrameInNum = 0;
    mFrameOutNum = 0;

    if(mInputBufferMap.empty() || (mInputFormat.bufferSize != mInputPlaneSize[0] + mInputPlaneSize[1])){
        ret = prepareInputBuffers();
        ALOGV("onStart prepareInputBuffers ret=%d",ret);
        if(ret != OK)
            return ret;
    }

    for(int32_t i = 0; i < mOutputFormat.bufferNum; i++){
        ret = allocateOutputBuffer(i);
        if(ret != OK){
            ALOGE("allocateOutputBuffer failed");
            return ret;
        }
    }

    mState = RUNNING;

    ALOGV("onStart ret=%d",ret);

    return ret;
}
status_t V4l2Enc::prepareOutputParams()
{
    status_t ret = UNKNOWN_ERROR;
    Mutex::Autolock autoLock(mLock);

    ret = pDev->GetStreamTypeByMime(mMime, &mOutFormat);
    if(ret != OK)
        return ret;

    ALOGV("mOutFormat=%x",mOutFormat);
    if(!pDev->IsCaptureFormatSupported(mOutFormat)){
        ALOGE("encoder format not suppoted");
        return ret;
    }

    if(mOutputFormat.bufferNum == 0){
        mOutputFormat.bufferNum = 2;
    }
    ALOGV("bufferNum=%d,bufferSize=%d",mOutputFormat.bufferNum, mOutputFormat.bufferSize);

    struct v4l2_frmsizeenum info;
    memset(&info, 0, sizeof(v4l2_frmsizeenum));
    if(OK != pDev->GetFormatFrameInfo(mOutFormat, &info)){
        ALOGE("GetFormatFrameInfo failed");
        return ret;
    }

    mWidthAlign = info.stepwise.step_width;
    mHeightAlign = info.stepwise.step_height;
    if(mWidthAlign < 1)
        mWidthAlign = 1;

    return OK;
}
status_t V4l2Enc::SetOutputFormats()
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    mOutputFormat.width = Align(mOutputFormat.width, mWidthAlign);
    mOutputFormat.height = Align(mOutputFormat.height, mHeightAlign);
    mOutputFormat.stride = mOutputFormat.width;

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = mOutBufType;

    if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
        format.fmt.pix_mp.num_planes = mOutPlaneNum;
        format.fmt.pix_mp.pixelformat = mOutFormat;
        format.fmt.pix_mp.width = mOutputFormat.width;
        format.fmt.pix_mp.height = mOutputFormat.height;
        format.fmt.pix_mp.field = V4L2_FIELD_NONE;
        format.fmt.pix_mp.plane_fmt[0].sizeimage = mOutputFormat.bufferSize;
    } else {
        format.fmt.pix.pixelformat = mOutFormat;
        format.fmt.pix.width = mOutputFormat.width;
        format.fmt.pix.height = mOutputFormat.height;
        format.fmt.pix.field = V4L2_FIELD_NONE;
        format.fmt.pix.sizeimage = mOutputFormat.bufferSize;
    }

    result = ioctl (mFd, VIDIOC_S_FMT, &format);
    if(result != 0)
        return UNKNOWN_ERROR;

    memset(&format, 0, sizeof(format));
    format.type = mOutBufType;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result != 0)
        return UNKNOWN_ERROR;

    uint32_t retFormat, retWidth, retHeight, retSizeimage;

    if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
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

    if(retFormat != mOutFormat){
        ALOGE("SetOutputFormats mOutFormat mismatch");
        return UNKNOWN_ERROR;
    }

    if(retWidth != mOutputFormat.width || retHeight != mOutputFormat.height){
        ALOGE("SetOutputFormats resolution mismatch");
        return UNKNOWN_ERROR;
    }

    if(retSizeimage != mOutputFormat.bufferSize){
        mOutputFormat.bufferSize = retSizeimage;
        ALOGW("SetOutputFormats bufferSize mismatch");
    }

    return OK;
}
status_t V4l2Enc::prepareInputParams()
{
    status_t ret = UNKNOWN_ERROR;
    Mutex::Autolock autoLock(mLock);

    ret = pDev->GetV4l2FormatByColor(mInputFormat.pixelFormat, &mInFormat);
    if(ret != OK){
        ALOGE("prepareInputParams failed, pixelFormat=%x,mInFormat=%x",mInputFormat.pixelFormat,mInFormat);
        return ret;
    }

    if (!pDev->IsOutputFormatSupported(mInFormat)) {
        ALOGE("IsOutputFormatSupported failed, unsupported input format: 0x%x", mInputFormat.pixelFormat);
        return BAD_VALUE;
    }

    //update output frame width & height
    mInputFormat.width = Align(mInputFormat.width, mWidthAlign);
    mInputFormat.height = Align(mInputFormat.height, mHeightAlign);

    if(mInputFormat.stride < mInputFormat.width)
        mInputFormat.stride = mInputFormat.width;
    if(mInputFormat.sliceHeight < mInputFormat.height)
        mInputFormat.sliceHeight = mInputFormat.height;

    memset(&mInputPlaneSize[0], 0, kMaxInputBufferPlaneNum * sizeof(uint32_t));
    ALOGV("prepareInputParams width=%d,height=%d,mWidthAlign=%d,mInFormat=0x%x",mInputFormat.width, mInputFormat.height, (int)mWidthAlign,mInFormat);

    if (mInFormat == V4L2_PIX_FMT_YUV420M) {
        // YUV420 buffer width is align with 32, so actual buffer width is Align(mInputFormat.width, 32)
        mInputFormat.stride = Align(mInputFormat.stride, 32);
        mInPlaneNum = 3;
        mInputPlaneSize[0] = mInputFormat.stride * mInputFormat.sliceHeight;
        mInputPlaneSize[1] = mInputPlaneSize[0] / 4;
        mInputPlaneSize[2] = mInputPlaneSize[1];
        mInputFormat.bufferSize = mInputPlaneSize[0] + mInputPlaneSize[1] + mInputPlaneSize[2];
    } else {
        mInputPlaneSize[0] = mInputFormat.width * mInputFormat.height * pxlfmt2bpp(mInputFormat.pixelFormat) / 8;
        mInputFormat.bufferSize = mInputPlaneSize[0];
    }

    return OK;
}
status_t V4l2Enc::SetInputFormats()
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = mInBufType;

    ALOGV("SetInputFormats stride=%d,w=%d,w align=%d,pixelFormat=0x%x",mInputFormat.stride,mInputFormat.width, mWidthAlign,mInputFormat.pixelFormat);

    if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
        format.fmt.pix_mp.num_planes = mInPlaneNum;
        format.fmt.pix_mp.pixelformat = mInFormat;
        format.fmt.pix_mp.width = mInputFormat.width;
        format.fmt.pix_mp.height = mInputFormat.height;
        format.fmt.pix_mp.field = V4L2_FIELD_NONE;

        for (int i = 0; i < format.fmt.pix_mp.num_planes; i++) {
            format.fmt.pix_mp.plane_fmt[i].bytesperline = mInputFormat.stride;
        }
    } else {
        format.fmt.pix.pixelformat = mInFormat;
        format.fmt.pix.width = mInputFormat.width;
        format.fmt.pix.height = mInputFormat.height;
        format.fmt.pix.bytesperline = mInputFormat.stride;
        format.fmt.pix.field = V4L2_FIELD_NONE;
    }

    pDev->SetColorAspectsInfo(&mIsoColorAspect, &format.fmt.pix_mp);

    result = ioctl (mFd, VIDIOC_S_FMT, &format);
    if(result != 0){
        ALOGE("SetInputFormats VIDIOC_S_FMT failed");
        return UNKNOWN_ERROR;
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = mInBufType;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result < 0)
        return UNKNOWN_ERROR;

    uint32_t retFormat;

    if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
        retFormat = format.fmt.pix_mp.pixelformat;
        mInPlaneNum = format.fmt.pix_mp.num_planes;
        mInputFormat.bufferSize = 0;

        for (int i = 0; i < format.fmt.pix_mp.num_planes; i++) {
            if (format.fmt.pix_mp.plane_fmt[i].sizeimage != mInputPlaneSize[i]) {
                ALOGW("SetInputFormats bufferSize mismatch, changed from %d to %d ",
                        mInputPlaneSize[i], format.fmt.pix_mp.plane_fmt[i].sizeimage);
                mInputPlaneSize[i] = format.fmt.pix_mp.plane_fmt[i].sizeimage;
            }
            mInputFormat.bufferSize += mInputPlaneSize[i];
            ALOGV("plane_fmt[%d] sizeimage=%d,bytesperline=%d",i, format.fmt.pix_mp.plane_fmt[i].sizeimage, format.fmt.pix_mp.plane_fmt[0].bytesperline);
        }

        if(mInputFormat.stride != format.fmt.pix_mp.plane_fmt[0].bytesperline){
            mInputFormat.stride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
            ALOGI("SetInputFormats reset mInputFormat.stride=%d",mInputFormat.stride);
        }

        ALOGV("SetInputFormats get mInputFormat.stride=%d,mInputFormat.bufferSize=%d",mInputFormat.stride,mInputFormat.bufferSize);
    } else {
        retFormat = format.fmt.pix.pixelformat;

        if(format.fmt.pix.sizeimage != mInputPlaneSize[0]){
            ALOGW("SetInputFormats bufferSize mismatch, changed from %d to %d",
                    mInputPlaneSize[0], format.fmt.pix.sizeimage);
            mInputPlaneSize[0] = format.fmt.pix.sizeimage;
        }
    }

    if(retFormat != mInFormat){
        ALOGE("SetInputFormats mInputFormat mismatch, expect 0x%x, actual 0x%x", mInFormat, retFormat);
        return UNKNOWN_ERROR;
    }

    if (mCropWidth != mInputFormat.width || mCropHeight != mInputFormat.height) {
        struct v4l2_selection selection;
        selection.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        selection.target = V4L2_SEL_TGT_CROP;
        selection.r.left = 0;
        selection.r.top = 0;
        selection.r.width = mCropWidth;
        selection.r.height = mCropHeight;

        result = ioctl(mFd, VIDIOC_S_SELECTION, &selection);
        ALOGV("VIDIOC_S_SELECTION ret=%d (%d %d %d %d)", result,
            selection.r.left, selection.r.top, selection.r.width, selection.r.height);
    }

    ALOGV("SetInputFormats success");
    return OK;
}

V4l2Enc::InputRecord::InputRecord()
    : at_device(false), input_id(-1), ts(-1) {
    memset(&planes, 0, kMaxInputBufferPlaneNum * sizeof(VideoFramePlane));
}

V4l2Enc::InputRecord::~InputRecord() {}

V4l2Enc::OutputRecord::OutputRecord()
    : at_device(false), picture_id(0), flag(0) {
    memset(&plane, 0, sizeof(VideoFramePlane));
}

V4l2Enc::OutputRecord::~OutputRecord() {
}
status_t V4l2Enc::prepareInputBuffers()
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = mInputFormat.bufferNum;
    reqbufs.type = mInBufType;
    reqbufs.memory = mInMemType;
    ALOGV("prepareInputBuffers VIDIOC_REQBUFS bufferNum=%d",reqbufs.count);
    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGE("VIDIOC_REQBUFS failed result=%d",result);
        return UNKNOWN_ERROR;
    }
    ALOGV("prepareInputBuffers mInputBufferMap resize=%d",reqbufs.count);
    mInputBufferMap.resize(reqbufs.count);

    ALOGV("prepareInputBuffers total input=%d size=%d",mInputFormat.bufferNum, mInputBufferMap.size());

    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        mInputBufferMap[i].at_device = false;
        mInputBufferMap[i].input_id = -1;
        mInputBufferMap[i].planes[0].fd = -1;
        mInputBufferMap[i].planes[0].addr = 0;
    }
    return OK;
}
status_t V4l2Enc::prepareOutputBuffers()
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = mOutputFormat.bufferNum;
    reqbufs.type = mOutBufType;
    reqbufs.memory = mOutMemType;

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0){
        ALOGE("VIDIOC_REQBUFS failed result=%d",result);
        return UNKNOWN_ERROR;
    }

    mOutputBufferMap.resize(reqbufs.count);

    for (size_t i = 0; i < mOutputBufferMap.size(); i++) {
        mOutputBufferMap[i].at_device = false;
        mOutputBufferMap[i].plane.fd = -1;
        mOutputBufferMap[i].plane.addr = 0;
        mOutputBufferMap[i].plane.size = mOutputFormat.bufferSize;
        mOutputBufferMap[i].plane.length = 0;
        mOutputBufferMap[i].plane.offset = 0;
        mOutputBufferMap[i].flag = 0;
    }

    ALOGV("prepareOutputBuffers output=%d size=%d",mOutputFormat.bufferNum, mOutputBufferMap.size());

    return OK;
}
status_t V4l2Enc::destroyInputBuffers()
{
    Mutex::Autolock autoLock(mLock);
    if (mInputBufferMap.empty())
        return OK;

    int result = 0;
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 0;
    reqbufs.type = mInBufType;
    reqbufs.memory = V4L2_MEMORY_MMAP;//use mmap to free buffer

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0)
        ALOGW("%s result=%d", __FUNCTION__, result);

    mInputBufferMap.clear();
    ALOGV("destroyInputBuffers success");
    return OK;
}
status_t V4l2Enc::destroyOutputBuffers()
{
    Mutex::Autolock autoLock(mLock);
    if (mOutputBufferMap.empty())
        return OK;

    int result = 0;
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 0;
    reqbufs.type = mOutBufType;
    reqbufs.memory = V4L2_MEMORY_MMAP;//use mmap to free buffer

    result = ioctl(mFd, VIDIOC_REQBUFS, &reqbufs);

    if(result != 0)
        ALOGW("%s result=%d", __FUNCTION__, result);

    mOutputBufferMap.clear();
    //clearOutputFrameBuffer();//call it here or in base class
    ALOGV("destroyOutputBuffers success");
    return OK;
}
status_t V4l2Enc::HandlePollThread()
{
    status_t ret = OK;
    int32_t poll_ret = 0;

    while(bPollStarted){
        ALOGV("pollThreadHandler BEGIN");
        poll_ret = pDev->Poll();
        //ALOGV("pollThreadHandler poll_ret=%x, in=%d,out=%d",poll_ret, mFrameInNum, mFrameOutNum);

        if(!bPollStarted)
            break;

        if(poll_ret & V4L2_DEV_POLL_EVENT){
            ret = onDequeueEvent();
        }
        if(poll_ret & V4L2_DEV_POLL_OUTPUT){
            ret = dequeueInputBuffer();
        }
        if(poll_ret & V4L2_DEV_POLL_CAPTURE){
            ret = dequeueOutputBuffer();
        }
        ALOGV("pollThreadHandler END ret=%x",ret);
    }
    ALOGV("HandlePollThread return");
    return OK;

}

// static
void *V4l2Enc::PollThreadWrapper(void *me) {
    return (void *)(uintptr_t)static_cast<V4l2Enc *>(me)->HandlePollThread();
}

status_t V4l2Enc::createPollThread()
{
    Mutex::Autolock autoLock(mLock);

    if(!bPollStarted){
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        bPollStarted = true;
        pthread_create(&mPollThread, &attr, PollThreadWrapper, this);
        pthread_attr_destroy(&attr);
    }
    return OK;
}
status_t V4l2Enc::destroyPollThread()
{
    ALOGV("destroyPollThread BEGIN");

    if(bPollStarted){
        bPollStarted = false;
        ALOGV("destroyPollThread bPollStarted FALSE");
        usleep(1000);
    }

    if(mPollThread){
        pthread_join(mPollThread, NULL);
        mPollThread = 0;
    }

    return OK;
}


status_t V4l2Enc::encodeInternal(std::unique_ptr<IMXInputBuffer> input)
{
    ATRACE_CALL();
    int result = 0;

    int32_t index = -1;
    uint32_t v4l2_flags = 0;

    if(input == nullptr)
        return BAD_VALUE;

    if(UNINITIALIZED == mState)
        return BAD_VALUE;

    if(STOPPED == mState || PREPARED == mState)
        onStart();

    if (!bOutputStreamOn)
        startOutputStream();

    mLock.lock();

    bool eos = input->eos;

    if(eos)
        v4l2_flags |= V4L2_BUF_FLAG_LAST;

    //handle eos
    if(eos && -1 == input->id && 0 == input->size){
        ALOGV("encodeInternal StopEncoder");
        pDev->StopEncoder();
        mLock.unlock();
        return OK;
    }

    if((int64_t)input->timestamp >= 0)
        v4l2_flags |= (V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TIMESTAMP_COPY);

    if(bSyncFrame || (input->flag & FLAG_SYNC_FRAME)){
        v4l2_flags |= V4L2_BUF_FLAG_KEYFRAME;
        bSyncFrame = false;
    }

    if(input->size > mInputFormat.bufferSize){
        ALOGE("invalid buffer size=%d,cap=%d",input->size, mInputFormat.bufferSize);
        mLock.unlock();
        return UNKNOWN_ERROR;
    }

    //try to get index
    for(int32_t i = 0; i < mInputBufferMap.size(); i++){
        if(mInputBufferMap[i].input_id == -1 && !mInputBufferMap[i].at_device){
            index = i;
            break;
        }
    }

    if(index < 0){
        mLock.unlock();
        ALOGE("encodeInternal invalid index");
        return UNKNOWN_ERROR;
    }

    mInputBufferMap[index].input_id = input->id;
    ALOGV("encodeInternal input_id=%d,input->BUF=%p,phys=%x, index=%d, len=%zu, ts=%lld, fd=%d",
        input->id, input->pInputVirt,input->pInputPhys, index, input->size, input->timestamp, input->fd);

    if(mInputBufferMap[index].at_device){
        ALOGE("onQueueInputBuffer index=%d, at_device",index);
    }

    if (v4l2_flags & V4L2_BUF_FLAG_KEYFRAME) {
        pDev->SetForceKeyFrame();
    }

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[kMaxInputBufferPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(&planes[0], 0, mInPlaneNum * sizeof(struct v4l2_plane));

    if (mInFormat == V4L2_PIX_FMT_YUV420M) {
        planes[0].bytesused = mInputPlaneSize[0];
        planes[0].length = mInputPlaneSize[0];
        planes[0].data_offset = 0;

        //GraphicInputBuffers allocate HAL_PIXEL_FORMAT_YV12, need to switch UV for HAL_PIXEL_FORMAT_YCbCr_420_P
        planes[2].bytesused = mInputPlaneSize[0] + mInputPlaneSize[1];
        planes[2].length = mInputPlaneSize[0] + mInputPlaneSize[1];
        planes[2].data_offset = mInputPlaneSize[0];

        planes[1].data_offset = mInputPlaneSize[0]+mInputPlaneSize[1];
        planes[1].bytesused = mInputPlaneSize[0] + mInputPlaneSize[1] + mInputPlaneSize[2];
        planes[1].length = mInputPlaneSize[0] + mInputPlaneSize[1] + mInputPlaneSize[2];
    } else {
        planes[0].bytesused = mInputFormat.bufferSize;
        planes[0].length = mInputPlaneSize[0];
        planes[0].data_offset = 0;
    }

    if(mInMemType == V4L2_MEMORY_USERPTR){
        planes[0].m.userptr = mInputBufferMap[index].planes[0].addr = (uint64_t)input->pInputVirt;
    }else if(mInMemType == V4L2_MEMORY_DMABUF){
        planes[0].m.fd = mInputBufferMap[index].planes[0].fd = input->fd;
        if (mInFormat == V4L2_PIX_FMT_YUV420M) {
            planes[1].m.fd = input->fd;
            planes[2].m.fd = input->fd;
        }
    }

    stV4lBuf.index = index;
    stV4lBuf.type = mInBufType;
    if((int64_t)input->timestamp > 0){
        stV4lBuf.timestamp.tv_sec = (int64_t)input->timestamp/1000000;
        stV4lBuf.timestamp.tv_usec = (int64_t)input->timestamp - stV4lBuf.timestamp.tv_sec * 1000000;
    }
    stV4lBuf.memory = mInMemType;
    stV4lBuf.m.planes = &planes[0];
    stV4lBuf.length = mInPlaneNum;
    stV4lBuf.flags = v4l2_flags;

    ALOGV("V4l2Enc OUTPUT VIDIOC_QBUF index=%d,len=%d, ts=%lld,fd0=%d,fd1=%d\n",
        stV4lBuf.index, planes[0].bytesused, (long long)input->timestamp,planes[0].m.fd, planes[1].m.fd);

    result = ioctl(mFd, VIDIOC_QBUF, &stV4lBuf);
    if(result < 0){
        ALOGE("VIDIOC_QBUF failed, index=%d, result=%d",index,errno);
        mLock.unlock();
        return UNKNOWN_ERROR;
    }

    mInputBufferMap[index].at_device = true;
    mFrameInNum ++;
    dumpInputBuffer(input->fd, mInputFormat.bufferSize);
    mLock.unlock();

    if (!bInputStreamOn)
        startInputStream();
    if (!bPollStarted)
        createPollThread();

    if(eos)
        pDev->StopEncoder();

    return OK;
}
status_t V4l2Enc::dequeueInputBuffer()
{
    ATRACE_CALL();
    int result = 0;
    int input_id = -1;

    if(!bInputStreamOn || mState != RUNNING )
        return OK;

    Mutex::Autolock autoLock(mLock);
    if(!bInputStreamOn  || mState != RUNNING )
        return OK;

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[mInPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(planes, 0, sizeof(planes));
    stV4lBuf.type = mInBufType;
    stV4lBuf.memory = mInMemType;
    stV4lBuf.m.planes = planes;
    stV4lBuf.length = mInPlaneNum;
    result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
    if(result < 0){
        ALOGW("%s VIDIOC_DQBUF err=%d", __FUNCTION__, errno);
        return UNKNOWN_ERROR;
    }

    ALOGV("V4l2Enc OUTPUT_MPLANE VIDIOC_DQBUF index=%d\n", stV4lBuf.index);

    if(stV4lBuf.index >= mInputFormat.bufferNum)
        return BAD_INDEX;

    if(!mInputBufferMap[stV4lBuf.index].at_device){
        ALOGE("dequeueInputBuffer index=%d, not at_device",stV4lBuf.index);
    }
    mInputBufferMap[stV4lBuf.index].at_device = false;
    input_id = mInputBufferMap[stV4lBuf.index].input_id;
    mInputBufferMap[stV4lBuf.index].input_id = -1;

    ALOGV("dequeueInputBuffer NotifyInputBufferUsed id=%d",input_id);
    NotifyInputBufferUsed(input_id);

    return OK;
}

status_t V4l2Enc::queueOutput(int buffer_id)
{
    ATRACE_CALL();
    int result = 0;
    int32_t index = -1;

    if(STOPPING == mState || FLUSHING == mState){
        ALOGV("%s: wrong state %d", __func__, mState);
        return OK;
    }

    if(V4L2_MEMORY_MMAP == mOutMemType) {
        //pass index from buffer_id
        index = buffer_id;
    }

    if(index < 0){
        ALOGE("%s: invalid index %d", __func__, index);
        return UNKNOWN_ERROR;
    }

    mOutputBufferMap[index].picture_id = buffer_id;

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[mOutPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(&planes[0], 0, sizeof(struct v4l2_plane));

    planes[0].length = mOutputBufferMap[index].plane.size;
    planes[0].data_offset = mOutputBufferMap[index].plane.offset;

    if(mOutMemType == V4L2_MEMORY_DMABUF){
        planes[0].m.fd = -1;
    }else if(mOutMemType == V4L2_MEMORY_USERPTR){
        planes[0].m.userptr = 0;
    }else if(mOutMemType == V4L2_MEMORY_MMAP){
        planes[0].m.mem_offset = mOutputBufferMap[index].plane.offset;
        planes[0].data_offset = 0;
    }

    stV4lBuf.index = index;
    stV4lBuf.type = mOutBufType;
    stV4lBuf.memory = mOutMemType;
    stV4lBuf.m.planes = &planes[0];
    stV4lBuf.length = mOutPlaneNum;
    stV4lBuf.flags = 0;

    ALOGV("CAPTURE VIDIOC_QBUF index=%d\n",index);

    result = ioctl(mFd, VIDIOC_QBUF, &stV4lBuf);
    if(result < 0){
        ALOGE("VIDIOC_QBUF CAPTURE failed, index=%d, result=%d",index, errno);
        return UNKNOWN_ERROR;
    }

    mOutputBufferMap[index].at_device = true;
    return OK;
}

status_t V4l2Enc::startInputStream()
{
    Mutex::Autolock autoLock(mLock);
    if(!bInputStreamOn){
        enum v4l2_buf_type buf_type = mInBufType;
        if(0 == ioctl(mFd, VIDIOC_STREAMON, &buf_type)){
            bInputStreamOn = true;
            ALOGV("VIDIOC_STREAMON OUTPUT_MPLANE success");
        }
    }
    return OK;
}
status_t V4l2Enc::stopInputStream()
{
    Mutex::Autolock autoLock(mLock);
    if(bInputStreamOn){
        enum v4l2_buf_type buf_type = mInBufType;
        if(0 == ioctl(mFd, VIDIOC_STREAMOFF, &buf_type)){
            bInputStreamOn = false;
            ALOGV("VIDIOC_STREAMOFF OUTPUT_MPLANE success");
        }
    }


    for (size_t i = 0; i < mInputBufferMap.size(); i++) {
        mInputBufferMap[i].at_device = false;
        mInputBufferMap[i].input_id = -1;
    }

    return OK;
}
status_t V4l2Enc::startOutputStream()
{
    Mutex::Autolock autoLock(mLock);
    if (!bOutputStreamOn) {
        for (int32_t i = 0; i < mOutputBufferMap.size(); i++) {
            if (mOutputBufferMap[i].plane.addr != 0 && !mOutputBufferMap[i].at_device)
                queueOutput(i);
        }
        enum v4l2_buf_type buf_type = mOutBufType;
        if(0 == ioctl(mFd, VIDIOC_STREAMON, &buf_type)){
            bOutputStreamOn = true;
            ALOGV("VIDIOC_STREAMON CAPTURE_MPLANE success");
        }
    }
    return OK;
}
status_t V4l2Enc::stopOutputStream()
{
    Mutex::Autolock autoLock(mLock);
    int result = 0;

    //call VIDIOC_STREAMOFF and ignore the result
    enum v4l2_buf_type buf_type = mOutBufType;
    result = ioctl(mFd, VIDIOC_STREAMOFF, &buf_type);
    bOutputStreamOn = false;
    ALOGV("VIDIOC_STREAMOFF CAPTURE_MPLANE ret %d", result);

    for (size_t i = 0; i < mOutputBufferMap.size(); i++) {
        mOutputBufferMap[i].at_device = false;
    }
    return OK;
}
status_t V4l2Enc::dequeueOutputBuffer()
{
    ATRACE_CALL();
    int result = 0;
    if(!bOutputStreamOn  || mState != RUNNING)
        return OK;

    Mutex::Autolock autoLock(mLock);
    if(!bOutputStreamOn  || mState != RUNNING)
        return OK;

    uint64_t ts = 0;
    uint32_t out_len = 0;
    int keyFrame = 0;
    uint32_t out_offset = 0;

    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[mOutPlaneNum];
    memset(&stV4lBuf, 0, sizeof(stV4lBuf));
    memset(planes, 0, sizeof(planes));
    stV4lBuf.type = mOutBufType;
    stV4lBuf.memory = mOutMemType;
    stV4lBuf.m.planes = planes;
    stV4lBuf.length = mOutPlaneNum;
    result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
    if(result < 0){
        if(EPIPE == errno){
            ALOGV("%s VIDIOC_DQBUF EOS", __FUNCTION__);
            NotifyEOS();
            bPollStarted = false;
            mState = STOPPED;
            return OK;
        }else{
            ALOGW("%s VIDIOC_DQBUF err=%d", __FUNCTION__, errno);
            return UNKNOWN_ERROR;
        }
    }

    if(stV4lBuf.index >= mOutputFormat.bufferNum)
        return BAD_INDEX;

    out_len = stV4lBuf.m.planes[0].bytesused;
    ts = (uint64_t)stV4lBuf.timestamp.tv_sec *1000000;
    ts += stV4lBuf.timestamp.tv_usec;
    keyFrame = (stV4lBuf.flags & V4L2_BUF_FLAG_KEYFRAME)?1:0;

    ALOGV("V4l2Enc CAPTURE_MPLANE VIDIOC_DQBUF index=%d, len=%d,ts=%lld,flag=%x",stV4lBuf.index, out_len, ts, stV4lBuf.flags);

    if(!bHasCodecData && mConverter != NULL){
        uint8_t* vAddr = (uint8_t*)mOutputBufferMap[stV4lBuf.index].plane.addr;
        if(OK ==  mConverter->CheckSpsPps(vAddr, out_len, &out_offset)) {
            bHasCodecData = true;
            ALOGE("CHECK SPSPPS success");
        }
        else
            ALOGE("CHECK SPSPPS fail");
    }

    if(out_offset > 0)
        out_len -= out_offset;

    int blockId = 0;
    int fd = -1;
    int byteused = 0;
    unsigned long nOutPhy;
    unsigned long nOutVirt;
    uint32_t outBufLen = mOutputFormat.bufferSize;

    if(mOutMemType == V4L2_MEMORY_MMAP){
        if (OK == FetchOutputBuffer(&blockId, &fd, &nOutPhy, &nOutVirt, &outBufLen))
            memcpy((void*)nOutVirt, (void*)(mOutputBufferMap[stV4lBuf.index].plane.addr + out_offset), out_len);
        else
            return UNKNOWN_ERROR;
        dumpOutputBuffer((void*)nOutVirt, out_len);
    }

    if (V4L2_TYPE_IS_MULTIPLANAR(mOutBufType))
        byteused = stV4lBuf.m.planes[0].bytesused;
    else
        byteused = stV4lBuf.bytesused;

    if(byteused == 0 || stV4lBuf.flags & V4L2_BUF_FLAG_LAST){
        ALOGI("get last frame");
        NotifyEOS();
        bPollStarted = false;
        mState = STOPPED;
        mOutputBufferMap[stV4lBuf.index].at_device = false;
        return OK;
    }

    ALOGV("dequeueOutputBuffer NotifyOutputFrameReady blockId=%d,len=%d,ts=%lld,keyFrame=%x,out_offset=%d",blockId, out_len, ts, keyFrame,out_offset);
    NotifyOutputFrameReady(blockId, out_len , ts, keyFrame, 0);

    mFrameOutNum ++;
    mOutputBufferMap[stV4lBuf.index].at_device = false;
    queueOutput(stV4lBuf.index);
    return OK;
}
status_t V4l2Enc::onDequeueEvent()
{
    int result = 0;
    struct v4l2_event event;
    memset(&event, 0, sizeof(struct v4l2_event));
    result = ioctl(mFd, VIDIOC_DQEVENT, &event);
    if(result == 0){
        ALOGV("onDequeueEvent type=%d",event.type);
        switch(event.type){
            case V4L2_EVENT_SOURCE_CHANGE:
                if(event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION){
                    //TODO: send event
                    //handleFormatChanged();
                }
                break;
            case V4L2_EVENT_CODEC_ERROR:
                NotifyError(UNKNOWN_ERROR);//send error event
                break;
            default:
                break;
        }
    }

    return OK;
}

status_t V4l2Enc::DoSetConfig(EncConfig index, void* pConfig) {

    status_t ret = OK;

    if (!pConfig)
        return BAD_VALUE;

    switch (index) {
        case ENC_CONFIG_BIT_RATE:
        {
            int32_t tar = (*(int*)pConfig);
            if(mEncParam.nBitRate > 0 && mEncParam.nBitRate != tar){
                Mutex::Autolock autoLock(mLock);
                ret = pDev->SetEncoderBitrate(tar);
            }
            ALOGV("SetConfig ENC_CONFIG_BIT_RATE src=%d,tar=%d",mEncParam.nBitRate,tar);
            mEncParam.nBitRate = tar;
            bSyncFrame = true;
            break;
        }
        case ENC_CONFIG_INTRA_REFRESH:
        {
            if(0 == (*(int*)pConfig))
                bSyncFrame = false;
            else
                bSyncFrame = true;
            break;
        }
        case ENC_CONFIG_COLOR_FORMAT:
        {
            mInputFormat.pixelFormat = (*(int*)pConfig);
            break;
        }
        case ENC_CONFIG_FRAME_RATE:
        {
            int frameRate = (*(int*)pConfig);
            if (mEncParam.nFrameRate != frameRate) {
                //hantro encoder do not support dynamic set fps, so just adjust the bitrate.
                #ifdef HANTRO_V4L2
                int bps;
                bps = mEncParam.nBitRate * mEncParam.nFrameRate / frameRate;
                if(bps > 60000000)
                    bps = 60000000;
                Mutex::Autolock autoLock(mLock);
                pDev->SetEncoderBitrate(bps);
                ALOGV("SetConfig ENC_CONFIG_FRAME_RATE, change bitrate %d -> %d. change frame rate %d -> %d",
                    mEncParam.nBitRate, bps, mEncParam.nFrameRate, frameRate);
                mEncParam.nBitRate = bps;
                mEncParam.nFrameRate = frameRate;
                bSyncFrame = true;
                #else
                Mutex::Autolock autoLock(mLock);
                mTargetFps = mEncParam.nFrameRate = frameRate;
                pDev->SetFrameRate(mTargetFps);
                ALOGV("SetConfig ENC_CONFIG_FRAME_RATE,  change frame rate %d -> %d",
                    mEncParam.nFrameRate, frameRate);
                #endif
            }
            break;
        }
        default:
            ret = BAD_VALUE;
            break;
    }
    return ret;
}

status_t V4l2Enc::DoGetConfig(EncConfig index, void* pConfig) {
    if (!pConfig)
        return BAD_VALUE;

    status_t ret = OK;

    return ret;
}
#if 0
status_t V4l2Enc::handleFormatChanged() {

    status_t ret = OK;

    {
        Mutex::Autolock autoLock(mLock);

        int result = 0;
        struct v4l2_format format;
        uint32_t pixel_format = 0;
        memset(&format, 0, sizeof(struct v4l2_format));

        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        result = ioctl (mFd, VIDIOC_G_FMT, &format);

        if(result < 0)
            return UNKNOWN_ERROR;

        ret = pDev->GetColorFormatByV4l2( format.fmt.pix_mp.pixelformat, &pixel_format);
        if(ret != OK)
            return ret;

        mOutputFormat.pixelFormat = static_cast<int>(pixel_format);

        mOutputFormat.width = Align(format.fmt.pix_mp.width, FRAME_ALIGN);
        mOutputFormat.height = Align(format.fmt.pix_mp.height, FRAME_ALIGN);
        mOutputFormat.interlaced = ((format.fmt.pix_mp.field == V4L2_FIELD_INTERLACED) ? true: false);

        mOutputPlaneSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
        mOutputPlaneSize[1] = format.fmt.pix_mp.plane_fmt[1].sizeimage;

        struct v4l2_control ctl;
        memset(&ctl, 0, sizeof(struct v4l2_control));

        ctl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
        result = ioctl(mFd, VIDIOC_G_CTRL, &ctl);
        if(result < 0)
            return UNKNOWN_ERROR;

        mOutputFormat.minBufferNum = ctl.value;
        if(mOutputFormat.minBufferNum > mOutputFormat.bufferNum)
            mOutputFormat.bufferNum = mOutputFormat.minBufferNum;

        struct v4l2_crop crop;
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

        result = ioctl (mFd, VIDIOC_G_CROP, &crop);
        if(result < 0)
            return UNKNOWN_ERROR;

        mOutputFormat.rect.right = crop.c.width;
        mOutputFormat.rect.bottom = crop.c.height;
        mOutputFormat.rect.top = crop.c.top;
        mOutputFormat.rect.left = crop.c.left;
    }

    outputFormatChanged();

    ALOGV("getOutputVideoInfo w=%d,h=%d,buf cnt=%d, buffer size[0]=%d,size[1]=%d",
        mOutputFormat.width, mOutputFormat.height, mOutputFormat.minBufferNum, mOutputPlaneSize[0], mOutputPlaneSize[1]);
    return OK;
}
#endif

status_t V4l2Enc::onFlush()
{
    status_t ret = UNKNOWN_ERROR;
    ALOGV("onFlush BEGIN");
    int pre_state;
    {
        Mutex::Autolock autoLock(mLock);
        pre_state = mState;
        mState = FLUSHING;
    }

    ret = stopInputStream();
    if(ret != OK)
        return ret;

    ret = stopOutputStream();
    if(ret != OK)
        return ret;

    mState = pre_state;
    ALOGV("onFlush END ret=%d",ret);

    return ret;
}
status_t V4l2Enc::onStop()
{
    status_t ret = UNKNOWN_ERROR;

    ALOGV("onStop BEGIN");
    {
        Mutex::Autolock autoLock(mLock);
        mState = STOPPING;
    }
    ret = onFlush();

    // don't exit halfway, try to execute till end to avoid memory leak
    ret |= destroyPollThread();

    ret |= destroyInputBuffers();

    ret |= freeOutputBuffers();

    ret |= destroyOutputBuffers();

    if(mConverter != NULL){
        mConverter->Destroy();
        delete mConverter;
    }
    ALOGV("onStop END ret=%d",ret);

    if (OK == ret)
        mState = STOPPED;
    return ret;
}
status_t V4l2Enc::onDestroy()
{
    status_t ret = UNKNOWN_ERROR;

    ALOGV("onDestroy");
    Mutex::Autolock autoLock(mLock);

    if(pDev == NULL)
        return UNKNOWN_ERROR;

    if(mFd > 0){
        pDev->Close();
        mFd = 0;
    }

    if(pDev != NULL)
        delete pDev;
    pDev = NULL;

    ALOGV("onDestroy END");
    return OK;
}

void V4l2Enc::initEncInputParamter(EncInputParam *pInPara) {
    if(pInPara == NULL)
        return;

    memset(&mEncParam, 0, sizeof(V4l2EncInputParam));

    mTargetFps = pInPara->nFrameRate;
    mEncParam.nRotAngle = pInPara->nRotAngle;
    mEncParam.nBitRate = pInPara->nBitRate;
    mEncParam.nBitRateMode = C2ToV4l2BitrateMode(pInPara->nBitRateMode);
    mEncParam.nGOPSize = pInPara->nGOPSize;
    mEncParam.nIntraFreshNum = pInPara->nRefreshIntra;
    mEncParam.nFrameRate = pInPara->nFrameRate;

    mEncParam.nProfile = C2ToV4l2ProfileLevel(pInPara->nProfile, mMime, true);
    mEncParam.nLevel = C2ToV4l2ProfileLevel(pInPara->nLevel, mMime, false);

    if (pInPara->qpValid) {
        mEncParam.qpValid = pInPara->qpValid;
        mEncParam.i_qp = pInPara->i_qp;
        mEncParam.p_qp = pInPara->p_qp;
        mEncParam.b_qp = pInPara->b_qp;
        mEncParam.min_qp = pInPara->min_qp;
        mEncParam.max_qp = pInPara->max_qp;
    }

    mWidth = pInPara->nPicWidth;
    mHeight = pInPara->nPicHeight;
    mCropWidth = pInPara->nPicWidth;
    mCropHeight = pInPara->nPicHeight;

    mInputFormat.stride = pInPara->nWidthStride;
    mInputFormat.sliceHeight = pInPara->nHeightStride;

    mInputFormat.bufferNum = DEFAULT_INPUT_BUFFER_COUNT;
    mInputFormat.bufferSize = mInputFormat.stride * mInputFormat.sliceHeight * pxlfmt2bpp(pInPara->eColorFormat) / 8;
    mInputFormat.width = mWidth;
    //v4l2 api only has width, height, stride, so set sliceHeight to height
    mInputFormat.height = mInputFormat.sliceHeight;
    mInputFormat.interlaced = false;
    mInputFormat.pixelFormat = pInPara->eColorFormat;

    mOutputFormat.width = mWidth;
    mOutputFormat.height = mHeight;
    mOutputFormat.minBufferNum = 2;
    mOutputFormat.bufferNum = kDefaultOutputBufferCount;
    mOutputFormat.interlaced = false;

    mIsoColorAspect.colourPrimaries = pInPara->sColorAspects.primaries;
    mIsoColorAspect.transferCharacteristics = pInPara->sColorAspects.transfer;
    mIsoColorAspect.matrixCoeffs = pInPara->sColorAspects.matrixCoeffs;
    mIsoColorAspect.fullRange = pInPara->sColorAspects.fullRange;

    ALOGD("initEncInputParamter nRotAngle=%d,nBitRate=%d,nGOPSize=%d,nRefreshIntra=%d,mTargetFps=%d",
        pInPara->nRotAngle, pInPara->nBitRate, pInPara->nGOPSize, pInPara->nRefreshIntra,mTargetFps);

    return;
}
status_t V4l2Enc::getCodecData(uint8_t** pCodecData, uint32_t* size) {
    ALOGV("getCodecData");
    if(bHasCodecData && mConverter != NULL)
        return mConverter->GetSpsPpsPtr(pCodecData, size);
    else
        return UNKNOWN_ERROR;
}
bool V4l2Enc::checkIfPreProcessNeeded(int pixelFormat)
{
#if 1
    switch (pixelFormat) {
#ifndef HANTRO_VC8000E
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        ALOGV("bPreProcess TRUE");
        bPreProcess = true;
        return true;
#endif
    default:
        bPreProcess = false;
        return false;
    }

#else
    //check format with v4l2 driver format
    uint32_t v4l2_format = 0;
    if(OK != pDev->GetV4l2FormatByColor(pixelFormat, &v4l2_format)){
        bPreProcess = true;
        return true;
    }

    if(pDev->IsOutputFormatSupported(v4l2_format)){
        bPreProcess = false;
        ALOGV("checkIfPreProcessNeeded v4l2_format=0x%x, bPreProcess = false",v4l2_format);
        return false;
    }

    ALOGV("checkIfPreProcessNeeded v4l2_format=0x%x, bPreProcess = true",v4l2_format);
    bPreProcess = true;
    return true;
#endif

}

status_t V4l2Enc::allocateOutputBuffer(int32_t index)
{
    int result = 0;
    Mutex::Autolock autoLock(mLock);

    uint8_t * ptr = NULL;
    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[mOutPlaneNum];

    if(index > mOutputFormat.bufferNum)
        return UNKNOWN_ERROR;

    stV4lBuf.type = mOutBufType;
    stV4lBuf.memory = mOutMemType;
    stV4lBuf.index = index;
    stV4lBuf.length = mOutPlaneNum;
    stV4lBuf.m.planes = planes;

    result = ioctl(mFd, VIDIOC_QUERYBUF, &stV4lBuf);
    if(result < 0)
        return UNKNOWN_ERROR;

    planes[0].length = mOutputFormat.bufferSize;

    ptr = (uint8_t *)mmap(NULL, planes[0].length,
        PROT_READ | PROT_WRITE, /* recommended */
        MAP_SHARED,             /* recommended */
        mFd, planes[0].m.mem_offset);
    if(ptr != MAP_FAILED){
       mOutputBufferMap[index].plane.addr = (uint64_t)ptr;
    }else
        return UNKNOWN_ERROR;

    mOutputBufferMap[index].plane.fd = -1;
    mOutputBufferMap[index].plane.size = planes[0].length;
    mOutputBufferMap[index].plane.offset = planes[0].m.mem_offset;
    mOutputBufferMap[index].at_device = false;
    ALOGV("allocateOutputBuffer index=%d,size=%d,offset=%x,addr=%x",
        index,planes[0].length, planes[0].m.mem_offset, ptr);
    return OK;
}
status_t V4l2Enc::freeOutputBuffers()
{
    Mutex::Autolock autoLock(mLock);

    if(mOutputBufferMap.empty())
        return OK;

    for(int32_t i = 0; i < mOutputFormat.bufferNum; i++){
        if(0 != mOutputBufferMap[i].plane.addr){
            ALOGV("munmap %p",mOutputBufferMap[i].plane.addr);
            munmap((void*)mOutputBufferMap[i].plane.addr,mOutputBufferMap[i].plane.size);
            mOutputBufferMap[i].plane.addr = 0;
        }

        if(mOutputBufferMap[i].plane.fd > 0)
            close(mOutputBufferMap[i].plane.fd);

        mOutputBufferMap[i].plane.offset = 0;
        mOutputBufferMap[i].at_device = false;
    }
    mOutputBufferMap.clear();
    return OK;
}
void V4l2Enc::ParseVpuLogLevel()
{
    int level=0;
    FILE* fpVpuLog;
    nDebugFlag = 0;

    fpVpuLog=fopen(VPU_ENCODER_LOG_LEVELFILE, "r");
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
void V4l2Enc::dumpInputBuffer(int fd, uint32_t size)
{
    FILE * pfile = NULL;
    void* buf = NULL;

    if(fd <= 0){
        ALOGV("dumpInputBuffer invalid fd");
        return;
    }

    if(!(nDebugFlag & DUMP_ENC_FLAG_INPUT))
        return;

    if(mFrameOutNum < 200)
        pfile = fopen(DUMP_ENC_INPUT_FILE,"ab");

    if(pfile){
        buf = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
        fwrite(buf,1,size,pfile);
        ALOGV("dumpInputBuffer write %d",size);
        munmap(buf, size);
        fclose(pfile);
    }else
        ALOGE("dumpInputBuffer failed to open %s",DUMP_ENC_INPUT_FILE);
    return;
}
void V4l2Enc::dumpOutputBuffer(void* buf, uint32_t size)
{
    FILE * pfile = NULL;
    if(buf == NULL)
        return;

    if(!(nDebugFlag & DUMP_ENC_FLAG_OUTPUT))
        return;

    if(mFrameOutNum < 200)
        pfile = fopen(DUMP_ENC_OUTPUT_FILE,"ab");

    if(pfile){
        fwrite(buf,1,size,pfile);
        fclose(pfile);
    }
    return;
}

VideoEncoderBase * CreateVideoEncoderInstance(const char* mime) {
    return static_cast<VideoEncoderBase *>(new V4l2Enc(mime));
}
}
