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

#ifndef DEVELOPER_MEDIACONTROL_SERVER_NATIVESENSOR_H_
#define DEVELOPER_MEDIACONTROL_SERVER_NATIVESENSOR_H_

#include "IMediaModule.h"
#include "MediaEvent.h"
#include "thread"

#ifdef USE_SENSOR

#include "cam_device_api.hpp"

typedef struct NativeSensorSize {
	uint32_t bounds_width;
	uint32_t bounds_height;
	uint32_t top;
	uint32_t left;
	uint32_t width;
	uint32_t height;
} NativeSensorSize_t;

typedef struct NativeSensorMode
{
   uint32_t index;
   NativeSensorSize_t size;
   uint32_t fps;
   uint32_t hdr_mode;
   uint32_t stitching_mode;
   uint32_t bayer_pattern;
   uint32_t bit_width;
}NativeSensorMode_t;

typedef struct NativeSensorAWBConfig{
   uint32_t index;
   uint32_t mode;
   bool dumping;
}NativeSensorAWBConfig_t;

class NativeSensor : public IMediaModule {
    DECLARE_DYNAMIC_CLASS()
 public:
    NativeSensor();
    virtual ~NativeSensor();
    bool run(uint32_t nSinkIndex) override;
    const char* getClassName() override { return "NativeSensor"; }
    //void onFrameAvailable(void* data);

    bool start(bool warmup) override;
    bool stop() override;
    bool open(int id, Json::Value& node) override;
    bool close() override;
    void setFormat(int nPort, MediaCap& res, int type) override;
    int jsonRequest(int port, int ctrlId, Json::Value& jsonRequest, Json::Value& jsonResponse) override;
    void onGlobalPadBufferChanged(int type, int port) override;
    void onConnectedPadBufferChanged(int port, MediaBufferQueue& queue, const char* remoteClass) override;

 private:
    int getSensorMode();
    int setModeInfoToVideo();
    int UpdateSensorModeInfo();
    int setAEStartExposure(float exposure);
    int setAWBStartConfig(NativeSensorAWBConfig_t Config);
    int CaptureImage(int DeviceFd, char *ImageName);

 private:
    CAM_DEVICE_ID ispId;
    CAM_DEVICE mDriver;
    void initBuffers();
    std::thread mThread;
    void loop();
    bool bRunning = false;
    std::vector<BufIdentity*> bufList;
    int frameid = 0;
    std::map<uint64_t, MediaBuffer_t *> fullQueue;
    bool bPreloaded = false;
    int vidDevice = -1;
    NativeSensorMode_t SensorMode;
    float StartExposure  = 0;
    NativeSensorAWBConfig_t AWBStartConfig;
    NativeSensorSize_t SensorSize;
    NativeSensorSize_t CropSize;
    NativeSensorSize_t ScaleSize;
    NativeSensorSize_t PipeLineCropSize;
    bool bCameraConnect = false;
};

#endif

#endif  // DEVELOPER_MEDIACONTROL_SERVER_NATIVESENSOR_H_
