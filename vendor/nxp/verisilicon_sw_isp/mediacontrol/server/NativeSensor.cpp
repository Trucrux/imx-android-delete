/******************************************************************************\
|* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

#include "NativeSensor.h"

#include <iostream>
#include <fstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <memory.h>
#include <linux/videodev2.h>
#include "device.h"
#include "log.h"
#include "BufferManager.h"
#include "viv_video_kevent.h"

#define LOGTAG "NativeSensor"
#ifdef USE_SENSOR

NativeSensor::NativeSensor() {
    mSink.push_back(MediaPad(PAD_TYPE_IMAGE));
    mSrc.push_back(MediaPad(PAD_TYPE_IMAGE));
    mSrc.push_back(MediaPad(PAD_TYPE_IMAGE));
    StartExposure = 0;
    memset(&AWBStartConfig, 0 ,sizeof(AWBStartConfig));
}

NativeSensor::~NativeSensor() {
    close();
}

// queue all buffers before start streaming.  ispcore will handle internal buffer loop.
void NativeSensor::initBuffers() {
    uint64_t addr;
    auto& res = mPadCaps[PAD_OUTPUT][0];

    int index = 0;

    if (isEndpoint(0)) {
        while ((addr = GPOP(this, 1, 0)) != 0) {
            ALOGI("push external sink buffer %p into isp",  (void*) addr);
            BufIdentity* buf = new BufIdentity;
            MediaAddrBuffer temp;
            temp.vcreate(res.width, res.height, res.format);
            buf->address_usr = addr;
            buf->address_reg = addr;
            buf->buff_size = temp.mSize;
            buf->buffer_idx = index++;
            mDriver.bufferPoolAddEntity(ISPCORE_BUFIO_MP, buf);
            bufList.push_back(buf);
        }
    } else {
        MediaAddrBuffer* pBuffer = NULL;
        for (auto& remotePad : mConnections[0]) {
             int port = remotePad.nSinkIndex;
             auto pNext = remotePad.pNext;

             mDriver.bufferPoolClearBufList(ISPCORE_BUFIO_MP);
             for (auto buf : bufList) {
                 delete buf;
             }
             bufList.clear();

             pNext->mPadQueue[port].release();
             pNext->mPadQueue.clear();
             pNext->mPadQueue[port].create(res.width, res.height, res.format, 4);

             while ((pBuffer = (MediaAddrBuffer*)pNext->mPadQueue[port].dequeueBuffer()) != NULL) {
                ALOGI("push %s sink buffer %p into isp", pNext->getClassName(), (void*) pBuffer->baseAddress);
                BufIdentity* buf = new BufIdentity;
                buf->address_usr = pBuffer->baseAddress;
                buf->address_reg = pBuffer->baseAddress;
                buf->buff_size = pBuffer->mSize;
                buf->buffer_idx = index++;
                mDriver.bufferPoolAddEntity(ISPCORE_BUFIO_MP, buf);
                bufList.push_back(buf);
             }
        }
    }
}

// bypass dewarp, still has drop frame.
void NativeSensor::onGlobalPadBufferChanged(int type, int port) {
    if (!bRunning)
        return;
    uint64_t addr;
    while ((addr = GPOP(this, 1, port)) != 0) {
        ALOGI("%s: push external sink buffer %p into isp",  __func__, (void*) addr);
        auto iter = fullQueue.find(addr);
        if (iter != fullQueue.end()) {
            mDriver.QBUF(ISPCORE_BUFIO_MP, iter->second);
        }
    }
}

void NativeSensor::onConnectedPadBufferChanged(int port, MediaBufferQueue& queue, const char* remoteClass) {
    if (!bRunning)
        return;
    MediaAddrBuffer* pBuffer = NULL;
    if ((pBuffer = (MediaAddrBuffer*)queue.dequeueBuffer()) != NULL) {
        ALOGI("%s: push %s sink buffer %p into isp", __func__, remoteClass, (void*) pBuffer->baseAddress);
        auto iter = fullQueue.find(pBuffer->baseAddress);
        if (iter != fullQueue.end()) {
            mDriver.QBUF(ISPCORE_BUFIO_MP, iter->second);
        }
    }
}
int NativeSensor::setAEStartExposure(float exposure)
{
    Json::Value jRequest, jResponse;
    jRequest.clear();
    jRequest[SENSOR_START_EC] = exposure;
    mDriver.ioctl(ISPCORE_MODULE_SENSOR_SEC_SET, jRequest, jResponse);
    return jResponse["result"].asInt();
}

int NativeSensor::setAWBStartConfig(NativeSensorAWBConfig_t config)
{
    Json::Value jRequest, jResponse;
    jRequest.clear();
    jRequest[AWB_INDEX_PARAMS] = config.index;
    jRequest[AWB_DAMPING_PARAMS] = config.dumping;
    jRequest[AWB_MODE_PARAMS] = config.mode;
    return mDriver.ioctl(ISPCORE_MODULE_AWB_CFG_SET, jRequest, jResponse);
}

bool NativeSensor::start(bool warmup) {
    ALOGI("enter %s", __func__);

    if (bRunning) return false;

    int ret = mDriver.post<int>(ISPCORE_MODULE_DEVICE_PREVIEW_START, 0, 0);
    if (ret) {
        ALOGE("Start preview isp[%d] error", ispId);
    }

    mDriver.initOutChain(ISPCORE_BUFIO_MP, 0);
    mDriver.startOutChain(ISPCORE_BUFIO_MP);
    mDriver.attachChain(ISPCORE_BUFIO_MP);

    bRunning = true;

    return true;
}

bool NativeSensor::stop() {
    ALOGI("enter %s", __func__);

    if (!bRunning)
        return true;

    bRunning = false;
    bPreloaded = false;

    mDriver.post<int>(ISPCORE_MODULE_DEVICE_PREVIEW_STOP, 0, 0);
    mDriver.post<int>(ISPCORE_MODULE_DEVICE_CAMERA_DISCONNECT, 0, 0);
    bCameraConnect = false;

    mDriver.detachChain(ISPCORE_BUFIO_MP);
    mDriver.stopOutChain(ISPCORE_BUFIO_MP);
    mDriver.deInitOutChain(ISPCORE_BUFIO_MP);

    mDriver.bufferPoolClearBufList(ISPCORE_BUFIO_MP);
    GCLEAR(this);
    for (auto buf : bufList)
        delete buf;
    bufList.clear();

    return true;
}

void NativeSensor::loop() {
    buffCtrlEvent_t buffEvent;
    MediaBuffer_t * pBuf;
    while (bRunning) {
        int ret;
        ret = mDriver.waitForBufferEvent(ISPCORE_BUFIO_MP, &buffEvent, 100);
        if (ret !=0 ) {
            //ALOGE("%s waitForBufferEvent timeout", __func__);
            continue;
        }

        if(buffEvent.eventID == BUFF_CTRL_CMD_STOP) {
            ALOGW("get  BUFF_CTRL_CMD_STOP: %d", ret);
            break;
        }

        if(buffEvent.eventID != BUFF_CTRL_CMD_BUFFER_READY) {
            ALOGW("get BUFF_CTRL_CMD_READY: %d", ret);
            continue;
        }

        ret = mDriver.DQBUF(ISPCORE_BUFIO_MP, &pBuf);
        if (0 != ret) {
            ALOGW("%s DQBUF failed: %d", __func__, ret);
            continue;
        }

        auto res = mPadCaps[PAD_OUTPUT][0];
        MediaAddrBuffer mediaBuffer;
        mediaBuffer.vcreate(res.width, res.height, res.format);
        mediaBuffer.baseAddress = (uint64_t)pBuf->baseAddress;
        mediaBuffer.mPort = 0;
        mediaBuffer.mFrameNumber = frameid++;
        ALOGI("%s framenumber %d", __func__, frameid);
        if (captureFlag) {
            char szFile[256];
            sprintf(szFile, "capture_isp_%dx%d_%d.yuv", res.width, res.height, frameid);
            mediaBuffer.save(szFile);
            captureFlag = false;
        }
        mSrc[0].image = (MediaBuffer*)&mediaBuffer;
        fullQueue[pBuf->baseAddress] = pBuf;
        trigerNext(mediaBuffer.mPort);
    }
}

int NativeSensor::UpdateSensorModeInfo()
{
    int ret = getSensorMode();
    if (ret != 0) {
        ALOGE("getSensorMode failed, ret %d", ret);
        return ret;
    }

    ret = setModeInfoToVideo();
    if(ret != 0) {
        ALOGE("setModeInfoToVideo failed, ret %d", ret);
        return ret;
    }

    MediaCap InputResolution;
    InputResolution.width  = SensorMode.size.width;
    InputResolution.height = SensorMode.size.height;
    InputResolution.fps    = SensorMode.fps;

    SensorSize.bounds_width  = SensorMode.size.bounds_width;
    SensorSize.bounds_height = SensorMode.size.bounds_height;
    SensorSize.top           = SensorMode.size.top;
    SensorSize.left          = SensorMode.size.left;
    SensorSize.width         = SensorMode.size.width;
    SensorSize.height        = SensorMode.size.height;

    PipeLineCropSize.bounds_width  = SensorMode.size.width;
    PipeLineCropSize.bounds_height = SensorMode.size.height;
    PipeLineCropSize.top           = 0;
    PipeLineCropSize.left          = 0;
    PipeLineCropSize.width         = SensorMode.size.width;
    PipeLineCropSize.height        = SensorMode.size.height;


    CropSize.bounds_width  = SensorMode.size.width;
    CropSize.bounds_height = SensorMode.size.height;
    CropSize.top           = 0;
    CropSize.left          = 0;
    CropSize.width         = SensorMode.size.width;
    CropSize.height        = SensorMode.size.height;

    ScaleSize.bounds_width  = SensorMode.size.width;
    ScaleSize.bounds_height = SensorMode.size.height;
    ScaleSize.top           = 0;
    ScaleSize.left          = 0;
    ScaleSize.width         = SensorMode.size.width;
    ScaleSize.height        = SensorMode.size.height;

    switch(SensorMode.bit_width)
    {
        case 8:
            InputResolution.format = MEDIA_PIX_FMT_RAW8;
            break;
        case 10:
            InputResolution.format = MEDIA_PIX_FMT_RAW10;
            break;
        case 12:
            InputResolution.format = MEDIA_PIX_FMT_RAW12;
            break;
        default:
            InputResolution.format = MEDIA_PIX_FMT_RAW12;
            break;
    }

    setFormat(0,InputResolution,PAD_INPUT);
    return 0;
}

bool NativeSensor::open(int id, Json::Value& node) {
    int ret = -1;
    void *cam_device_handle;
    ispId = (id == 1 ? CAM_ISPCORE_ID_1 : CAM_ISPCORE_ID_0);
    if (mDriver.initHardware(ispId, &cam_device_handle)) {
        ALOGE("mDriver.initHardware failed, id %d", id);
        return false;
    }
    mDriver.initBufferPoolCtrl(ISPCORE_BUFIO_MP);
    mDriver.setBufferParameters(BUFF_MODE_USRPTR);

    vidDevice = openVideoDevice(id);
    ret = UpdateSensorModeInfo();
    if (ret != 0) {
        ALOGE("UpdateSensorModeInfo failed, ret %d", ret);
        return false;
    }

    return true;
}

bool NativeSensor::close() {
    if (bCameraConnect) {
        mDriver.post<int>(ISPCORE_MODULE_DEVICE_CAMERA_DISCONNECT, 0, 0);
        mDriver.bufferPoolClearBufList(ISPCORE_BUFIO_MP);
        GCLEAR(this);
        for (auto buf : bufList)
            delete buf;
        bufList.clear();
    }

    struct vvcam_constant_modeinfo modeinfo;
    memset(&modeinfo,0,sizeof(modeinfo));
    ioctl(vidDevice, VIV_VIDIOC_S_MODEINFO, &modeinfo);
    mDriver.releaseHardware();
    closeVideoDevice(&vidDevice);
    return true;
}

bool NativeSensor::run(uint32_t nSinkIndex) {
    if (mSink[nSinkIndex].image == NULL) {
        return false;
    }

    if (!getSinkState(nSinkIndex)) {
        return false;
    }

    setSinkState(nSinkIndex, false);
    return true;
}

void NativeSensor::setFormat(int nPort, MediaCap& res, int type) {
    if (type == PAD_INPUT)  {
        IMediaModule::setFormat(nPort, res, type);
        ALOGI("%s isp input: bounds %dX%d left %d top %d size %dX%d\n", __func__,
        SensorSize.bounds_width,SensorSize.bounds_height,SensorSize.left,SensorSize.top,SensorSize.width,SensorSize.height);
    } else {
        res.width  = ScaleSize.width;
        res.height = ScaleSize.height;
        res.fps = mPadCaps[PAD_INPUT][0].fps;
        IMediaModule::setFormat(nPort, res, type);
        ALOGI("%s isp crop: bounds %dX%d left %d top %d size %dX%d\n",__func__,
        CropSize.bounds_width,CropSize.bounds_height,CropSize.left,CropSize.top,CropSize.width,CropSize.height);
        ALOGI("%s isp scale: bounds %dX%d left %d top %d size %dX%d\n",__func__,
        ScaleSize.bounds_width,ScaleSize.bounds_height,ScaleSize.left,ScaleSize.top,ScaleSize.width,ScaleSize.height);

        Json::Value jRequest, jResponse;
        jRequest.clear();
        jResponse.clear();
        if (bCameraConnect == false) {
             mDriver.post<const char*>(ISPCORE_MODULE_DEVICE_CALIBRATIONLOADANDINIT,
                    "calibration.file", "");
            mDriver.ioctl(ISPCORE_MODULE_SENSOR_OPEN, jRequest, jResponse);
        } else {
            mDriver.post<int>(ISPCORE_MODULE_DEVICE_CAMERA_DISCONNECT, 0, 0);
        }

        initBuffers();
        mDriver.bufferPoolKernelAddrMap(ISPCORE_BUFIO_MP, false);
        mDriver.bufferPoolSetBufToEngine(ISPCORE_BUFIO_MP);

        struct v4l2_format format;
        unsigned int colorspace = 0;
        unsigned int colorrange = 0;
        memset(&format,0,sizeof(v4l2_format));
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(vidDevice, VIDIOC_G_FMT, &format);
        switch(format.fmt.pix.colorspace) {
            case V4L2_COLORSPACE_REC709:
                colorspace = 0;
                break;
            case V4L2_COLORSPACE_BT2020:
                colorspace = 1;
                break;
            default:
                colorspace = 0;
                break;
        }
        switch(format.fmt.pix.quantization) {
            case V4L2_QUANTIZATION_LIM_RANGE:
                colorrange = 0;
                break;
            case V4L2_QUANTIZATION_FULL_RANGE:
                colorrange = 1;
                break;
            default:
                colorrange = 0;
                break;
        }

        jRequest.clear();
        jRequest[DEVICE_CAMCONNECT_PREVIEW] = true;
        jRequest[DEVICE_CAMERA_SIZE][RECT_BOUNDS_WIDTH]  = SensorSize.bounds_width;
        jRequest[DEVICE_CAMERA_SIZE][RECT_BOUNDS_HEIGHT] = SensorSize.bounds_height;
        jRequest[DEVICE_CAMERA_SIZE][RECT_LEFT]          = SensorSize.left;
        jRequest[DEVICE_CAMERA_SIZE][RECT_TOP]           = SensorSize.top;
        jRequest[DEVICE_CAMERA_SIZE][RECT_WIDTH]         = SensorSize.width;
        jRequest[DEVICE_CAMERA_SIZE][RECT_HEIGHT]        = SensorSize.height;

        jRequest[DEVICE_ISP_CROP_SIZE][RECT_BOUNDS_WIDTH]  = CropSize.bounds_width;
        jRequest[DEVICE_ISP_CROP_SIZE][RECT_BOUNDS_HEIGHT] = CropSize.bounds_height;
        jRequest[DEVICE_ISP_CROP_SIZE][RECT_LEFT]          = CropSize.left;
        jRequest[DEVICE_ISP_CROP_SIZE][RECT_TOP]           = CropSize.top;
        jRequest[DEVICE_ISP_CROP_SIZE][RECT_WIDTH]         = CropSize.width;
        jRequest[DEVICE_ISP_CROP_SIZE][RECT_HEIGHT]        = CropSize.height;

        jRequest[DEVICE_ISP_SCALE_SIZE][RECT_BOUNDS_WIDTH]  = ScaleSize.bounds_width;
        jRequest[DEVICE_ISP_SCALE_SIZE][RECT_BOUNDS_HEIGHT] = ScaleSize.bounds_height;
        jRequest[DEVICE_ISP_SCALE_SIZE][RECT_LEFT]          = ScaleSize.left;
        jRequest[DEVICE_ISP_SCALE_SIZE][RECT_TOP]           = ScaleSize.top;
        jRequest[DEVICE_ISP_SCALE_SIZE][RECT_WIDTH]         = ScaleSize.width;
        jRequest[DEVICE_ISP_SCALE_SIZE][RECT_HEIGHT]        = ScaleSize.height;

        jRequest[DEVICE_FORMAT_PARAMS] = mPadCaps[PAD_OUTPUT][nPort].format;
        jRequest[DEVICE_FPS_PARAMS]    = mPadCaps[PAD_OUTPUT][nPort].fps;

        jRequest[DEVICE_COLORSPACE_PARAMS] = colorspace;
        jRequest[DEVICE_COLORRANGE_PARAMS] = colorrange;
        mDriver.ioctl(ISPCORE_MODULE_DEVICE_CAMERA_CONNECT, jRequest, jResponse);
        bCameraConnect = true;

    }
    ALOGI("setFormat %d %d %d,  type %d, port: %d", res.format, res.width, res.height, type, nPort);
}

int NativeSensor::getSensorMode()
{
    Json::Value request, response;

    memset(&SensorMode,0,sizeof(SensorMode));
    int ret = mDriver.ioctl(ISPCORE_MODULE_SENSOR_QUERY, request, response);
    if (ret != 0)
    {
        ALOGE("native sensor get sensor mode error");
        return ret;
    }

    unsigned int CurModeIndex = response["current"].asUInt();
    for (auto& item : response["caps"]) {
        if (CurModeIndex == item["index"].asUInt()) {
            SensorMode.index              = item["index"].asUInt();
            SensorMode.size.bounds_width  = item["size"]["bounds_width"].asUInt();
            SensorMode.size.bounds_height = item["size"]["bounds_height"].asUInt();
            SensorMode.size.top           = item["size"]["top"].asUInt();
            SensorMode.size.left          = item["size"]["left"].asUInt();
            SensorMode.size.width         = item["size"]["width"].asUInt();
            SensorMode.size.height        = item["size"]["height"].asUInt();
            SensorMode.fps                = item["fps"].asUInt();
            SensorMode.hdr_mode           = item["hdr_mode"].asUInt();
            SensorMode.stitching_mode     = item["stitching_mode"].asUInt();
            SensorMode.bayer_pattern      = item["bayer_pattern"].asUInt();
            SensorMode.bit_width          = item["bit_width"].asUInt();
            break;
        }
    }
    return 0;
}

int NativeSensor::setModeInfoToVideo() {
    struct vvcam_constant_modeinfo modeinfo;

    memset(&modeinfo,0,sizeof(modeinfo));

    modeinfo.index = SensorMode.index;
    modeinfo.size.bounds_width     = SensorMode.size.bounds_width;
    modeinfo.size.bounds_height    = SensorMode.size.bounds_height;
    modeinfo.size.top              = SensorMode.size.top;
    modeinfo.size.left             = SensorMode.size.left;
    modeinfo.size.width            = SensorMode.size.width;
    modeinfo.size.height           = SensorMode.size.height;
    modeinfo.fps                   = SensorMode.fps;
    modeinfo.bayer_pattern         = SensorMode.bayer_pattern;
    modeinfo.bit_width             = SensorMode.bit_width;

    ALOGI("VIV_VIDIOC_S_MODEINFO, index %d, bounds %dx%d, size (l %d, t %d, w %d, h %d), fps %d, bayer_pattern %d, bit_width %d",
        modeinfo.index, modeinfo.size.bounds_width, modeinfo.size.bounds_height,
        modeinfo.size.left, modeinfo.size.top, modeinfo.size.width, modeinfo.size.height,
        modeinfo.fps, modeinfo.bayer_pattern, modeinfo.bit_width);

    int ret = ioctl(vidDevice, VIV_VIDIOC_S_MODEINFO, &modeinfo);
    if (ret) {
        ALOGE("VIV_VIDIOC_S_MODEINFO failed, ret %d, %s", ret, strerror(errno));
    }

    return ret;
}

int NativeSensor::CaptureImage(int DeviceFd, char *ImageName) {
    int try_count = 10;
    int DumpStaus = DUMPBUF_ENABLE;
    ioctl(DeviceFd, VIV_VIDIOC_S_DUMPBUF_STATUS, &DumpStaus);

    do {
        usleep(100000);
        ioctl(DeviceFd, VIV_VIDIOC_G_DUMPBUF_STATUS, &DumpStaus);
        try_count--;
    } while((try_count > 0) && (DumpStaus != DUMPBUF_DONE));

    if (DumpStaus != DUMPBUF_DONE) {
        ALOGE("%s dump buf failed %d \n", __func__, DumpStaus);
        DumpStaus = DUMPBUF_DISABLE;
        ioctl(DeviceFd, VIV_VIDIOC_S_DUMPBUF_STATUS, &DumpStaus);
        return -1;
    }
    struct viv_caps_dump_buf_s DumpBuf;
    if (ioctl(DeviceFd, VIV_VIDIOC_DUMPBUF, &DumpBuf) != 0) {
        ALOGE("%s get dump buf failed \n", __func__);
        DumpStaus = DUMPBUF_DISABLE;
        ioctl(DeviceFd, VIV_VIDIOC_S_DUMPBUF_STATUS, &DumpStaus);
        return -1;
    }

    void * CaptureBuf = ::mmap(NULL, DumpBuf.size, PROT_READ | PROT_WRITE, MAP_SHARED, DeviceFd, DumpBuf.offset);
    if (CaptureBuf == MAP_FAILED) {
        DumpStaus = DUMPBUF_DISABLE;
        ioctl(DeviceFd, VIV_VIDIOC_S_DUMPBUF_STATUS, &DumpStaus);
        ALOGE("%s mmap dump buf failed \n", __func__);
        return -1;
    }

    FILE* pFile = NULL;;
    if (!(pFile = fopen(ImageName, "wb"))) {
        DumpStaus = DUMPBUF_DISABLE;
        ioctl(DeviceFd, VIV_VIDIOC_S_DUMPBUF_STATUS, &DumpStaus);
        ::munmap(CaptureBuf,DumpBuf.size);
        ALOGE("%s failed to create file %s", __func__, ImageName);
        return -1;
    }
    fwrite(CaptureBuf, 1, DumpBuf.size, pFile);
    fclose(pFile);

    ::munmap(CaptureBuf,DumpBuf.size);
    DumpStaus = DUMPBUF_DISABLE;
    ioctl(DeviceFd, VIV_VIDIOC_S_DUMPBUF_STATUS, &DumpStaus);

    return 0;
}

static std::unordered_map<int, std::string> V4l2FormatToStringTable = {
    {V4L2_PIX_FMT_NV16, "YUV422SP" },
    {V4L2_PIX_FMT_YUYV, "YUV422I"  },
    {V4L2_PIX_FMT_NV12, "YUV420SP" },
    {V4L2_PIX_FMT_SBGGR8, "RAW8"   },
    {V4L2_PIX_FMT_SGBRG8, "RAW8"   },
    {V4L2_PIX_FMT_SGRBG8, "RAW8"   },
    {V4L2_PIX_FMT_SRGGB8, "RAW8"   },
    {V4L2_PIX_FMT_SBGGR10, "RAW10"   },
    {V4L2_PIX_FMT_SGBRG10, "RAW10"   },
    {V4L2_PIX_FMT_SGRBG10, "RAW10"   },
    {V4L2_PIX_FMT_SRGGB10, "RAW10"   },
    {V4L2_PIX_FMT_SBGGR12, "RAW12"   },
    {V4L2_PIX_FMT_SGBRG12, "RAW12"   },
    {V4L2_PIX_FMT_SGRBG12, "RAW12"   },
    {V4L2_PIX_FMT_SRGGB12, "RAW12"   },
};

int NativeSensor::jsonRequest(int port, int ctrlId, Json::Value& request, Json::Value& response) {
    ALOGI("%s 0x%x", __func__, ctrlId);
    int ret = 0;
    bool pipelineEnable;
    if (ctrlId == VIV_V4L_MC_CAPTURE) {
        timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        tm nowTime;
        localtime_r(&time.tv_sec, &nowTime);

        v4l2_format format;
        memset(&format, 0, sizeof(format));
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(vidDevice, VIDIOC_G_FMT, &format);
        char CaptureImageName[64];
        if (request.isMember(SENSOR_FILENAME_PARAMS)) {
            strcpy(CaptureImageName, (request[SENSOR_FILENAME_PARAMS].asString() + ".yuv").c_str());
        } else {
            sprintf(CaptureImageName,"Image_%d_%d_%s_%04d%02d%02d%02d%02d%02d.yuv",
                format.fmt.pix.width, format.fmt.pix.height,
                V4l2FormatToStringTable[format.fmt.pix.pixelformat].c_str(),
                nowTime.tm_year + 1900, nowTime.tm_mon+1, nowTime.tm_mday,
                nowTime.tm_hour, nowTime.tm_min, nowTime.tm_sec);
        }

        ret = CaptureImage(vidDevice, CaptureImageName);
        response["CaptureImage"] = CaptureImageName;
        response["result"] = ret;
        return 0;
    }
    if (ctrlId == ISPCORE_MODULE_SENSOR_PRE_LOAD_LIB) {
        if (!bPreloaded) {
            ret = mDriver.post<const char*>(ISPCORE_MODULE_DEVICE_CALIBRATIONLOADANDINIT,
                    "calibration.file", "");
            ret |= mDriver.ioctl(ISPCORE_MODULE_SENSOR_OPEN, request, response);
            if (!ret)
                bPreloaded = true;
        }
        response["result"] = ret;
        return 0;
    }

    if (ctrlId == ISPCORE_MODULE_PIPELINE_WARM_UP) {
        pipelineEnable = request[PIPELINE_ENABLE_PARAMS].asBool();
        ALOGI("pipelineEnable=%d\n", pipelineEnable);
        ret = mDriver.post<bool>(ISPCORE_MODULE_DEVICE_STANDBY_ENABLE_SET, "standby.enable", !pipelineEnable);
        response["result"] = ret;

        return 0;
    }

    if (ctrlId == ISPCORE_MODULE_PIPELINE_CFG_DWE) {
        int enable = request["enable"].asBool() ? 1 : 0;
        if (!bRunning && vidDevice >= 0)
            ret = ioctl(vidDevice, VIV_VIDIOC_S_DWECFG, &enable);
        response["result"] = ret;
        return 0;
    }

    if (ctrlId == ISPCORE_MODULE_SENSOR_FPS_SET) {
        if ( !bRunning ) {
            mPadCaps[PAD_OUTPUT][0].fps = request[SENSOR_FPS_PARAMS].asUInt();
            mPadCaps[PAD_INPUT][0].fps = request[SENSOR_FPS_PARAMS].asUInt();
            return 0;
        }
    }

    if (ctrlId == ISPCORE_MODULE_SENSOR_SEC_SET) {
        if ( !bRunning ) {
            StartExposure = request[SENSOR_START_EC].asFloat();
            response["result"] = 0;
        } else {
            response["result"] = -1;
        }
        return 0;
    }

    if (ctrlId == ISPCORE_MODULE_SENSOR_SEC_GET){
        if ( !bRunning ){
            response["result"] = -1;
            response[SENSOR_START_EC] = StartExposure;
            return 0;
        }
    }

    if (ctrlId == ISPCORE_MODULE_AWB_CFG_SET){
        if ( !bRunning ){
            AWBStartConfig.mode    = request[AWB_MODE_PARAMS].asUInt();
            AWBStartConfig.index   = request[AWB_INDEX_PARAMS].asUInt();
            AWBStartConfig.dumping = request[AWB_DAMPING_PARAMS].asBool();
            response["result"] = 0;
            return 0;
        }
    }

    if (ctrlId == ISPCORE_MODULE_DEVICE_SET_CROP) {
        int DewarpConnect;
        ioctl(vidDevice,VIV_VIDIOC_G_DWECFG, &DewarpConnect);
        Json::Value node = request["crop"];
        PipeLineCropSize.left          = node["start_x"].asUInt();
        PipeLineCropSize.top           = node["start_y"].asUInt();
        PipeLineCropSize.width         = node["width"].asUInt();
        PipeLineCropSize.height        = node["height"].asUInt();

        if (DewarpConnect){
            request["crop"]["bounds_width"] = CropSize.bounds_width;
            request["crop"]["bounds_height"] = CropSize.bounds_height;
            return -1;
        } else {
            Json::Value node = request["crop"];
            CropSize.left          = PipeLineCropSize.left ;
            CropSize.top           = PipeLineCropSize.top ;
            CropSize.width         = PipeLineCropSize.width;
            CropSize.height        = PipeLineCropSize.height;

            ScaleSize.bounds_width  = CropSize.width;
            ScaleSize.bounds_height = CropSize.height;
            return 0;
        }
    }

    if (ctrlId == ISPCORE_MODULE_DEVICE_SET_SCALE) {
        Json::Value node = request["scale"];
        if ( (node["width"].asUInt() <= PipeLineCropSize.width) &&
             (node["height"].asUInt() <= PipeLineCropSize.height) ) {
            ScaleSize.width         = node["width"].asUInt() * CropSize.width / PipeLineCropSize.width ;
            ScaleSize.height        = node["height"].asUInt() * CropSize.height / PipeLineCropSize.height;
            ScaleSize.width = ALIGN_UP(ScaleSize.width, 16);
            ScaleSize.height = ALIGN_UP(ScaleSize.height, 8);
            return 0;
        } else {
            ScaleSize.width  = CropSize.width;
            ScaleSize.height = CropSize.height;
            return -1;
        }
    }

    if (ctrlId > ISPCORE_MODULE_DEFAULT && ctrlId < ISPCORE_MODULE_END)
        ret = mDriver.ioctl(ctrlId, request, response);

    if (ctrlId == ISPCORE_MODULE_SENSOR_MODE_SET)
    {
        ret |= UpdateSensorModeInfo();
    }

    return ret;
}

IMPLEMENT_DYNAMIC_CLASS(NativeSensor)

#endif
