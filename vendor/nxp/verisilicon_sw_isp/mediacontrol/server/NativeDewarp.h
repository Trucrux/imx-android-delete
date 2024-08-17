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

#ifndef DEVELOPER_MEDIACONTROL_SERVER_NATIVEDEWARP_H_
#define DEVELOPER_MEDIACONTROL_SERVER_NATIVEDEWARP_H_

#include "IMediaModule.h"

#ifdef WITH_DWE
#include "DewarpDriver.h"

#define DEWARP_SUPPORT_MODE_MAX 20
#define LINE_STRING_SIZE_MAX  128

typedef struct DewarpMode_s
{
   uint32_t index;
   char CfgJsonFile[LINE_STRING_SIZE_MAX];
   struct dewarp_json_params_array array;
}DewarpMode_t;

typedef struct DewarpModeCfg_s
{
   uint32_t DewarpCurMode;
   uint32_t DewarpModeCount;

   DewarpMode_t mode[DEWARP_SUPPORT_MODE_MAX];
}DewarpModeCfg_t;

typedef struct DewarpCrop_s
{
   uint16_t bounds_width;
   uint16_t bounds_height;
   uint16_t start_x;
   uint16_t start_y;
   uint16_t width;
   uint16_t height;
}DewarpCrop_t;

typedef struct DewarpScale_s
{
   uint16_t start_x;
   uint16_t start_y;
   uint16_t width;
   uint16_t height;
}DewarpScale_t;

class NativeDewarp : public IMediaModule {
    DECLARE_DYNAMIC_CLASS()
 public:
    NativeDewarp();
    virtual ~NativeDewarp();
    bool run(uint32_t nSinkIndex) override;
    const char* getClassName() override { return "NativeDewarp"; }
    void trigerNext(uint32_t nSrcIndex) override;

    bool start(bool warmup) override;
    bool stop() override;
    bool open(int id, Json::Value& node) override;
    bool close() override;
    void setFormat(int nPort, MediaCap& res, int type) override;
    void onFrameAvailable(uint64_t addrSrc, uint64_t addrDst, int port);
    int jsonRequest(int port, int ctrlId, Json::Value& jsonRequest, Json::Value& jsonResponse) override;
    void onGlobalPadBufferChanged(int type, int port) override;
    void onConnectedPadBufferChanged(int port, MediaBufferQueue& queue, const char* remoteClass) override;

    int GetDewarpSupportMode(const char *CfgFile);
    int GetDewarpMode(unsigned int *pindex);
    int SetDewarpMode(unsigned int index);

 protected:
    bool load(Json::Value& node) override;

 private:
    int dewarpFd = -1;
    bool bRunning = false;
    DewarpDriver mDriver;
    struct dewarp_parameters params;
    struct dewarp_distortion_map dmap[2];
    int mFrameNumber = 0;
    DewarpModeCfg_t DewarpMode;
    DewarpCrop_t CropSize;
    DewarpScale_t ScaleSize;
    DewarpCrop_t PipeLineCropSize;
};

#endif
#endif  // DEVELOPER_MEDIACONTROL_SERVER_NATIVEDEWARP_H_
