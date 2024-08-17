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

#include "NativeDewarp.h"

#ifdef WITH_DWE

#include <assert.h>
#include <memory.h>

#include <map>
#include <fstream>
#include <string>
#include <vector>

#include "dewarpdev.h"
#include "DewarpDriver.h"
#include "DewarpConfigParser.h"
#include "device.h"
#include "log.h"

#define LOGTAG "NativeDewarp"

// only support 1 input, 1 output
NativeDewarp::NativeDewarp() {
    memset(&params, 0, sizeof(params));
    memset(dmap, 0, sizeof(dmap));
    mSink.resize(3, MediaPad(PAD_TYPE_IMAGE));
    mSrc.resize(3, MediaPad(PAD_TYPE_IMAGE));
}

NativeDewarp::~NativeDewarp() {
    close();
}

int NativeDewarp::GetDewarpSupportMode(const char *pCfgFileName)
{
    FILE* pFile;
    char  lineStr[LINE_STRING_SIZE_MAX];
    char cmd[LINE_STRING_SIZE_MAX];
    char params[LINE_STRING_SIZE_MAX];
    uint32_t mode_index = 0;

    if((pFile = fopen(pCfgFileName, "r")) == NULL)
    {
        ALOGE("%s: open %s failed!\n", __func__, pCfgFileName);
        return EXIT_FAILURE;
    }

    memset(&DewarpMode,0,sizeof(DewarpModeCfg_t));

    while(!feof(pFile))
    {
        if (NULL == fgets(lineStr, LINE_STRING_SIZE_MAX, pFile)) {
            continue;
        }

        if (lineStr[0] == '\n'){
            continue;
        }

        memset(params,0,sizeof(params));
        memset(cmd,0,sizeof(cmd));

        sscanf(lineStr+strspn(lineStr," \t"),"%[^ =\t]",cmd);

        if (strcmp(cmd,"mode") == 0)
        {
            sscanf(lineStr+strspn(lineStr," \t"),"%*[^=]=%*[\" \t]%[^\" \t\n\r]",params);
            DewarpMode.DewarpCurMode = atoi(params);
        }else if (strncmp(cmd,"[mode.",strlen("[mode.")) == 0)
        {
            sscanf(lineStr+strspn(lineStr," \t"),"[mode.%u]",&mode_index);
            DewarpMode.DewarpModeCount++;
            DewarpMode.mode[DewarpMode.DewarpModeCount - 1].index = mode_index;
        }else if (strcmp(cmd,"dwe") == 0)
        {
            sscanf(lineStr+strspn(lineStr," \t"),"%*[^=]=%*[\" \t]%[^\" \t\n\r]",params);
            memcpy(DewarpMode.mode[DewarpMode.DewarpModeCount - 1].CfgJsonFile,params,strlen(params));
        }
    }
    fclose(pFile);

    for (unsigned int i=0; i < DewarpMode.DewarpModeCount; i++)
    {
        char *JsonFile =  DewarpMode.mode[i].CfgJsonFile;
        if (access(JsonFile, F_OK) != 0)
        {
            ALOGE("%s: mode[%d] dwe json %s is not exist!\n", __func__, DewarpMode.mode[i].index, JsonFile);
            continue;
        }

        struct dewarp_json_params_array *parray = &(DewarpMode.mode[i].array);

        if ((JsonFile == NULL) || (dweJsonFileLoadParams(JsonFile,parray) == false))
        {
            ALOGE("%s: load mode[%d] JsonFile failed!\n", __func__, DewarpMode.mode[i].index);
        }
    }

    return EXIT_SUCCESS;
}

int NativeDewarp::GetDewarpMode(unsigned int *pindex)
{
    *pindex = DewarpMode.DewarpCurMode;
    return EXIT_SUCCESS;
}

int NativeDewarp::SetDewarpMode(unsigned int index)
{
    DewarpMode.DewarpCurMode = index;
    ALOGD("Set dewarp mode to %d.",DewarpMode.DewarpCurMode);

    return  EXIT_SUCCESS;
}

#define ANDROID_ISP_CONFIG_DIR  "/vendor/etc/configs/isp"
#define SENSOR_INFO_FILE_NAME_SIZE  64
bool NativeDewarp::open(int id, Json::Value& node) {
    dewarpFd = mDriver.open(id);
    if (dewarpFd == -1) {
        ALOGE("failed to open dewarp driver.");
        return false;
    }
    mDriver.registerBufferCallback(std::bind(&NativeDewarp::onFrameAvailable, this,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    char cfgSensorFile[SENSOR_INFO_FILE_NAME_SIZE];
#if defined(__ANDROID__)
    snprintf(cfgSensorFile, SENSOR_INFO_FILE_NAME_SIZE, "%s/Sensor%d_Entry.cfg", ANDROID_ISP_CONFIG_DIR, id);
#else
    snprintf(cfgSensorFile, SENSOR_INFO_FILE_NAME_SIZE, "Sensor%d_Entry.cfg", id);
#endif

    int ret = EXIT_SUCCESS;
    ret = GetDewarpSupportMode(cfgSensorFile);

    memset(&PipeLineCropSize,0,sizeof(PipeLineCropSize));
    memset(&CropSize,0,sizeof(CropSize));
    memset(&ScaleSize,0,sizeof(ScaleSize));
    return ret ? false : true;
}

bool NativeDewarp::close() {
    mDriver.removeBufferCallback();
    mDriver.close(dewarpFd);
    dewarpFd = -1;
    return true;
}

bool NativeDewarp::start(bool warmup) {
    mRefCount++;
    if (bRunning) return false;

    if (!mDriver.setParams(&params)) {
        ALOGE("failed to set params");
        return false;
    }

    if (!mDriver.setDistortionMap(dmap)) {
        ALOGE("failed to set distortion map");
        return false;
    }

    mFrameNumber = 0;

    if (!mDriver.start()) {
        ALOGE("failed to start dewarp driver");
        return false;
    }
    bRunning = true;
    return true;
}

bool NativeDewarp::stop() {
    ALOGI("enter %s", __func__);
    if (--mRefCount <= 0) {
        if (!bRunning) return false;
        bRunning = false;
        mDriver.stop();
        for (auto& item : mPadQueue) {
            item.second.release();
        }
        mPadQueue.clear();

    }
    ALOGI("leave %s", __func__);
    return true;
}

bool NativeDewarp::load(Json::Value& node) {
    ALOGD("enter %s", __func__);
    memset(&params, 0, sizeof(params));
    memset(dmap, 0, sizeof(dmap));
    std::vector<uint32_t> userMap;
    if (!dweParseJsonNode(node, userMap, dmap, params))
        return false;
    return true;
}

void NativeDewarp::onFrameAvailable(uint64_t addrSrc, uint64_t addrDst, int port) {
    auto& res = mPadCaps[PAD_OUTPUT][port];
    // ALOGD("onFrameAvailable: w:%d h:%d  y_base: %p", res.width, res.height, (void*)addrDst);
    GPOP(&mDriver, 2, port);
    MediaAddrBuffer buffer;
    buffer.vcreate(res.width, res.height, res.format);
    buffer.baseAddress = addrDst;
    mSrc[port].image = &buffer;
    ALOGI("%s framenumber %d", __func__, mFrameNumber);
    mFrameNumber++;
    if (captureFlag) {
        char szFile[256];
        sprintf(szFile, "capture_dewarp_%dx%d_%d.yuv", res.width, res.height, mFrameNumber);
        buffer.save(szFile);
        sprintf(szFile, "capture_dewarp_map_%d.txt", mFrameNumber);
        FILE* pMatFile  = fopen(szFile, "w");
        for (int i = 0; i < 9; i++)
            fprintf(pMatFile, "%.4f, ", dmap[port].camera_matrix[i]);
        for (int i = 0; i < 8; i++)
            fprintf(pMatFile, "%.4f, ", dmap[port].distortion_coeff[i]);
        fclose(pMatFile);
        captureFlag = false;
    }
    releasePadBuffer(port, addrSrc);
    IMediaModule::trigerNext(port);
    mSrc[port].image = NULL;
}

void NativeDewarp::onGlobalPadBufferChanged(int type, int port) {
    uint64_t dstAddress = 0;
    while ( (dstAddress = GPOP(this, type, port)) != 0) {
        ALOGI("%s: push external sink buffer %p into dewarp", __func__, (void*) dstAddress);
        GPUSH(&mDriver, type, port, dstAddress);
    }
}

void NativeDewarp::onConnectedPadBufferChanged(int port, MediaBufferQueue& queue, const char* remoteClass) {
    MediaAddrBuffer* pBuffer = NULL;
    if ((pBuffer = (MediaAddrBuffer*)queue.dequeueBuffer()) != NULL) {
        ALOGI("%s: push %s sink buffer %p into dewarp", __func__, remoteClass, (void*) pBuffer->baseAddress);
        GPUSH(&mDriver, 1, port, BASEADDR(pBuffer));
    }
}

bool NativeDewarp::run(uint32_t nSinkIndex) {
    if (!bRunning) {
        return false;
    }

    if (!getSinkState(nSinkIndex))
        return false;
    MediaBuffer* pBuffer = mSink[nSinkIndex].image;

    for (auto& item : mPadQueue[nSinkIndex].mBuffers) {
        if (BASEADDR(item) == BASEADDR(pBuffer)) {
            mPadQueue[nSinkIndex].queueBuffer(item);
            //ALOGD("queue one src buffer channel %d w:%d h:%d s:%d size:%d  y_base: %p  %d %d  %d", i,
                //   item->mWidth, item->mHeight,item->mStride, item->mSize,
                //  item->getBuffer(), pBuffer->mStride, pBuffer->mSize, pBuffer->mBufferType);
            MediaBuffer* temp = mPadQueue[nSinkIndex].acquiredBuffer();
            if (temp) GPUSH(&mDriver, 0, nSinkIndex, BASEADDR(temp) );
            break;
        }
    }

    setSinkState(nSinkIndex, false);
    return true;
}

void NativeDewarp::trigerNext(uint32_t nSrcIndex) {
    //ALOGI("Hooked passdown at NativeResize pad: %d",  nSrcIndex);
}

void NativeDewarp::setFormat(int nPort, MediaCap& res, int type) {
    IMediaModule::setFormat(nPort, res, type);
    ALOGI("setFormat %d %d %d,  type %d, port: %d", res.format, res.width, res.height, type, nPort);
    if (type == PAD_INPUT) {
        DewarpMode_t *pMode = NULL;
        for (unsigned int i=0; i<DewarpMode.DewarpModeCount; i++) {
            if(DewarpMode.mode[i].index == DewarpMode.DewarpCurMode) {
                pMode = &(DewarpMode.mode[i]);
                break;
            }
        }
        if (pMode == NULL) {
            ALOGE("Can not find dewarp mode[%d]",DewarpMode.DewarpCurMode);
            pMode = &(DewarpMode.mode[0]);
        }
        //Find the dewarp configuration based on the resolution
        struct dewarp_json_config* pdewarp_config = NULL;
        for (unsigned int i=0; i<pMode->array.count; i++) {
            if (pMode->array.dewarp_config[i].params.image_size.width == (unsigned int)res.width &&
                pMode->array.dewarp_config[i].params.image_size.height == (unsigned int)res.height) {
                pdewarp_config = &pMode->array.dewarp_config[i];
                break;
            }
        }
        /*If there no Dewarp configuration at the current resolution,
        * look for a Dewarp configuration that uses the same ratio for width and height
        */
        if (pdewarp_config == NULL) {
            for (unsigned int i=0; i<pMode->array.count; i++) {
                double map_params_ratio_h,map_params_ratio_v;
                map_params_ratio_h = (double)res.width / (pMode->array.dewarp_config[i].params.image_size.width);
                map_params_ratio_v = (double)res.height / (pMode->array.dewarp_config[i].params.image_size.height);
                if (map_params_ratio_h == map_params_ratio_v) {
                    pdewarp_config = &pMode->array.dewarp_config[i];
                    break;
                }
            }
        }
        //If no corresponding configuration is found, the first set of configurations is used
        if (pdewarp_config == NULL){
            pdewarp_config = &pMode->array.dewarp_config[0];
        }

        memcpy(&params,&pdewarp_config->params,sizeof(struct dewarp_parameters));
        memcpy(&dmap[0],&pdewarp_config->dmap,sizeof(struct dewarp_distortion_map));
        if (pdewarp_config->params.image_size.width != (unsigned int)res.width ||
            pdewarp_config->params.image_size.height != (unsigned int)res.height) {
                dmap[0].camera_matrix[0] = dmap[0].camera_matrix[0] * res.width /
                                        (pdewarp_config->params.image_size.width);
                dmap[0].camera_matrix[2] = dmap[0].camera_matrix[2] * res.width /
                                        (pdewarp_config->params.image_size.width);
                dmap[0].camera_matrix[4] = dmap[0].camera_matrix[4] * res.height /
                                        (pdewarp_config->params.image_size.height);
                dmap[0].camera_matrix[5] = dmap[0].camera_matrix[5] * res.height /
                                        (pdewarp_config->params.image_size.height);
        }

        params.image_size.width = res.width;
        params.image_size.height = res.height;
        params.pix_fmt_in = res.format;
        ALOGI("%s dewarp input: %dx%d %s\n", __func__,
                params.image_size.width,
                params.image_size.height,
                mediaFormatToStringTable[params.pix_fmt_in].c_str());
        params.roi_start.width = 0;
        params.roi_start.height = 0;

        if (PipeLineCropSize.width != 0 && PipeLineCropSize.height != 0) {
            CropSize.start_x = PipeLineCropSize.start_x * res.width / PipeLineCropSize.bounds_width;
            CropSize.start_y = PipeLineCropSize.start_y * res.height /PipeLineCropSize.bounds_height;
            CropSize.width   = PipeLineCropSize.width * res.width / PipeLineCropSize.bounds_width;
            CropSize.height   = PipeLineCropSize.height * res.height / PipeLineCropSize.bounds_height;
            params.roi_start.width =  CropSize.start_x;
            params.roi_start.height = CropSize.start_y;

            ALOGI("%s pipeline crop size  left %d top %d size %dX%d conversion to dewarp crop: left %d top %d size %dX%d\n", __func__,
                PipeLineCropSize.start_x, PipeLineCropSize.start_y, PipeLineCropSize.width, PipeLineCropSize.height,
                CropSize.start_x, CropSize.start_y, CropSize.width, CropSize.height);
        }

        params.scale_factor     = 4096;
        if (ScaleSize.width != 0 && ScaleSize.height != 0) {
            float hscale,vscale;
            float factors;
            if (CropSize.width != 0 && CropSize.height != 0) {
                hscale = (float)ScaleSize.width / CropSize.width;
                vscale = (float)ScaleSize.height / CropSize.height;
            } else {
                hscale = (float)ScaleSize.width /  params.image_size.width;
                vscale = (float)ScaleSize.height / params.image_size.height;
            }

            factors = (hscale > vscale) ? hscale : vscale;
            if (factors < 1.0) {
                factors = 1.0;
                ALOGE("%s: scale factors is lower 1.0",__func__);
            } else if (factors > 4.0) {
                factors = 4.0;
                 ALOGE("%s: scale factors is greater than 4.0",__func__);
            }
            params.scale_factor     = factors*4096;
            ALOGI("%s: dewarp scale: left %d top %d size %dX%d\n", __func__,
                ScaleSize.start_x, ScaleSize.start_y, ScaleSize.width, ScaleSize.height);
        }
    } else {
        params.image_size_dst.width  = res.width;
        params.image_size_dst.height = res.height;
        params.pix_fmt_out           = res.format;
        ALOGI("%s dewarp output: %dx%d %s\n", __func__,
                params.image_size_dst.width,
                params.image_size_dst.height,
                mediaFormatToStringTable[params.pix_fmt_out].c_str());
    }
}

int NativeDewarp::jsonRequest(int port, int ctrlId, Json::Value& request, Json::Value& response) {
    int ret_val = 0;
    switch (ctrlId) {
    case VIV_V4L_DWE_SET_PARAM: {
        Json::Value node = request["dwe"];
        params.dewarp_type = node["mode"].asInt();
        params.hflip = node["hflip"].asBool();
        params.vflip = node["vflip"].asBool();
        params.bypass = node["bypass"].asBool();
        for (int i = 0; i < 9; i++) {
            dmap[port].camera_matrix[i] = node["mat"][i].asDouble();
        }
        for (int i = 9; i < 17; i++) {
             dmap[port].distortion_coeff[i-9] = node["mat"][i].asDouble();
        }

        if (!mDriver.setParams(&params)) {
            ALOGE("failed to set params");
        }

        if (!mDriver.setDistortionMap(dmap)) {
            ALOGE("failed to set distortion map");
        }

        break;
    }
    case VIV_V4L_DWE_GET_PARAM: {
        Json::Value ret;
        ret["mode"] = params.dewarp_type;
        ret["hflip"] = params.hflip;
        ret["vflip"] = params.vflip;
        ret["bypass"] = params.bypass;
        for (int i = 0; i < 9; i++) {
            ret["mat"][i] = dmap[port].camera_matrix[i];
        }
        for (int i = 9; i < 17; i++) {
            ret["mat"][i] = dmap[port].distortion_coeff[i-9];
        }

        response["dwe"] = ret;
        break;
    }
    case VIV_V4L_DWE_GET_HFLIP: {
        Json::Value node;
        node["hflip"] = params.hflip;
        response["dwe"] = node;
        break;
    }
    case VIV_V4L_DWE_SET_HFLIP: {
        Json::Value node = request["dwe"];
        params.hflip = node["hflip"].asBool();
        mDriver.setFlip(port, params.hflip, params.vflip);
        break;
    }
    case VIV_V4L_DWE_GET_VFLIP: {
        Json::Value node;
        node["vflip"] = params.vflip;
        response["dwe"] = node;
        break;
    }
    case VIV_V4L_DWE_SET_VFLIP : {
        Json::Value node = request["dwe"];
        params.vflip = node["vflip"].asBool();
        mDriver.setFlip(port, params.hflip, params.vflip);
        break;
    }
    case VIV_V4L_DWE_GET_BYPASS: {
        Json::Value node;
        node["bypass"] = params.bypass;
        response["dwe"] = node;
        break;
    }
    case VIV_V4L_DWE_SET_BYPASS : {
        Json::Value node = request["dwe"];
        params.bypass = node["bypass"].asBool();
        if (!mDriver.setParams(&params)) {
            ALOGE("failed to set params");
        }

        if (!mDriver.setDistortionMap(dmap)) {
            ALOGE("failed to set distortion map");
        }
        break;
    }
    case VIV_V4L_DWE_GET_MODE: {
        Json::Value node;
        node["mode"] = DewarpMode.DewarpCurMode;
        response["dwe"] = node;
        break;
    }
    case VIV_V4L_DWE_SET_MODE: {
        Json::Value node = request["dwe"];
        uint32_t mode_index = node["mode"].asInt();
        SetDewarpMode(mode_index);
        break;
    }
    case VIV_V4L_DWE_GET_TYPE: {
        Json::Value node;
        node["type"] =  params.dewarp_type;
        response["dwe"] = node;
        break;
    }
    case VIV_V4L_DWE_SET_TYPE: {
        Json::Value node = request["dwe"];
        params.dewarp_type = node["type"].asInt();
        if (!mDriver.setParams(&params)) {
            ALOGE("failed to set params");
        }

        if (!mDriver.setDistortionMap(dmap)) {
            ALOGE("failed to set distortion map");
        }
        break;
    }
    case VIV_V4L_DWE_GET_MATRIX: {
        Json::Value node;
        for (int i = 0; i < 9; i++) {
            node["mat"][i] = dmap[port].camera_matrix[i];
        }
        for (int i = 9; i < 17; i++) {
            node["mat"][i] = dmap[port].distortion_coeff[i-9];
        }
        response["dwe"] = node;
        break;
    }
    case VIV_V4L_DWE_SET_MATRIX: {
        Json::Value node = request["dwe"];
        for (int i = 0; i < 9; i++) {
            dmap[port].camera_matrix[i] = node["mat"][i].asDouble();
        }
        for (int i = 9; i < 17; i++) {
             dmap[port].distortion_coeff[i-9] = node["mat"][i].asDouble();
        }
        if (!mDriver.setDistortionMap(dmap)) {
            ALOGE("failed to set distortion map");
        }
        break;
    }
    case VIV_V4L_DWE_SET_CROP:{
        Json::Value node = request["crop"];
        PipeLineCropSize.bounds_width = node["bounds_width"].asUInt();
        PipeLineCropSize.bounds_height = node["bounds_height"].asUInt();
        PipeLineCropSize.start_x = node["start_x"].asUInt();
        PipeLineCropSize.start_y = node["start_y"].asUInt();
        PipeLineCropSize.width   = node["width"].asUInt();
        PipeLineCropSize.height  = node["height"].asUInt();
        break;
    }
    case VIV_V4L_DWE_SET_SCALE:{
        Json::Value node = request["scale"];
        ScaleSize.start_x = node["start_x"].asUInt();
        ScaleSize.start_y = node["start_y"].asUInt();
        ScaleSize.width   = node["width"].asUInt();
        ScaleSize.height  = node["height"].asUInt();
        break;
    }
    case VIV_V4L_MC_CAPTURE:
        captureFlag = true;
        break;
    default: break;
    }
    response["result"] = response["result"].asInt() | ret_val;
    return ret_val;
}

IMPLEMENT_DYNAMIC_CLASS(NativeDewarp)

#endif
