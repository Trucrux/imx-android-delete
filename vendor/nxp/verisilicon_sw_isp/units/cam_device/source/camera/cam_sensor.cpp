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
#include "cam_sensor.hpp"
#include "cam_common.hpp"
#include "calib_calibration.hpp"
#include "calib_features.hpp"
#include "ispcore_holder.hpp"
#include "exception.hpp"
#include "macros.hpp"
#include <dlfcn.h>
#include <isi/isi_iss.h>
#include <vector>
#include <vvsensor.h>

using namespace camdev;

SensorOps ::SensorOps (camdev::CalibSensor &calibrationSensor, HalHolder *pHalHolder_input) : calibrationSensor (calibrationSensor) {
    TRACE_IN;

    pHalHolder = pHalHolder_input;
    if (!pHalHolder) {
    //pHalHolder = new HalHolder();
        TRACE(CITF_ERR, "%s Error: pah hot initialized\n", __PRETTY_FUNCTION__);
    }
    // configure sample edge on fpga
    //HalWriteSysReg(pHalHolder->hHal, 0x0004,
    //                             0x601); // TODO: this crude write does probably overwrite
    // vital HW control signal settings

    // reference marvin software HAL
    //DCT_ASSERT(!HalAddRef(pHalHolder->hHal));

    state = Init;

    TRACE_OUT;
}

SensorOps::~SensorOps() {
    TRACE_IN;

    // dereference marvin software HAL
    //DCT_ASSERT(!HalDelRef(pHalHolder->hHal));

    DCT_ASSERT(!close());

    calibDb.uninstall();

    if (pLib) {
        DCT_ASSERT(!dlclose(pLib));
    }

    pHalHolder = nullptr;
    TRACE_OUT;
}

int32_t SensorOps::capsGet(IsiSensorCaps_t &sensorCaps) {

    int32_t ret = IsiGetCapsIss(hSensor, &sensorCaps);
    REPORT(ret);

    return RET_SUCCESS;
}

SensorOps &SensorOps::checkValid() {
    if (state <= Init) {
        throw exc::LogicError(RET_WRONG_STATE, "Load sensor driver firstly");
    }

    return *this;
}

int32_t SensorOps::configGet(IsiSensorConfig_t &sensorConfig) {
    sensorConfig = config;

    return RET_SUCCESS;
}

int32_t SensorOps::configSet(IsiSensorConfig_t &sensorConfig) {
    int32_t ret = IsiSetupSensorIss(hSensor, &sensorConfig);
    REPORT(ret);

    config = sensorConfig;

    return RET_SUCCESS;
}

int32_t SensorOps::close() {
    if (hSensor) {
        int32_t ret = IsiReleaseSensorIss(hSensor);
        REPORT(ret);

        /*Reset current mode to default*/
        uint32_t mode;
        DCT_ASSERT(!HalGetSensorDefaultMode(pHalHolder->hHal, &mode));
        DCT_ASSERT(!HalSetSensorMode(pHalHolder->hHal, mode));
    }

    return RET_SUCCESS;
}

int32_t SensorOps::driverChange(std::string driverFileName, std::string calibFileName) {
    if (!fs::isExists(driverFileName)) {
        throw exc::LogicError(
                RET_INVALID_PARM,
                "Select sensor driver file and calibration file firstly");
    }

    close();

    calibDb.uninstall();

    if (pLib) {
        DCT_ASSERT(!dlclose(pLib));
    }

    calibrationSensor.config.driverFileName = driverFileName;

    DCT_ASSERT(pLib = dlopen(calibrationSensor.config.driverFileName.c_str(), RTLD_LAZY));

    DCT_ASSERT(pCamDrvConfig = (IsiCamDrvConfig_t *)dlsym(pLib, "IsiCamDrvConfig"));

    DCT_ASSERT(!pCamDrvConfig->pfIsiGetSensorIss(&pCamDrvConfig->IsiSensor));

    pSensor = &pCamDrvConfig->IsiSensor;

    if (calibFileName.empty()) {
        calibrationSensor.config.calibFileName = driverFileName;
        calibrationSensor.config.calibFileName.replace(
                calibrationSensor.config.calibFileName.end() - 4,
                calibrationSensor.config.calibFileName.end(), ".xml");
    } else {
        calibrationSensor.config.calibFileName = calibFileName;
    }

    DCT_ASSERT(!calibDb.install(calibrationSensor.config.calibFileName));

    //int32_t ret = open(driverFileName);
    //REPORT(ret);

    int ret = reset();
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::ecConfigGet(camdev::CalibEc::Config &config) {
    IsiSensorIntTime_t SensorInt;
    IsiSensorGain_t    SensorGain;
    IsiSensorAeInfo_t  AeInfo;

    RESULT result = IsiGetIntegrationTimeIss(hSensor, &SensorInt);
    if (result != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s IsiGetIntegrationTimeIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    result = IsiGetGainIss(hSensor, &SensorGain);
    if (result != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s IsiGetGainIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    result = IsiGetAeInfoIss(hSensor, &AeInfo);
    if ( result != RET_SUCCESS ) {
        TRACE(CITF_ERR, "%s IsiGetAeInfoIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }


    if (SensorInt.expoFrmType == ISI_EXPO_FRAME_TYPE_1FRAME) {
        calibrationSensor.config.ec.config.integrationTime =
            (float)SensorInt.IntegrationTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.gain =
            (float)SensorGain.gain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.minInt =
            (float)AeInfo.minIntTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.maxInt = 
            (float)AeInfo.maxIntTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.minGain = 
            ((float)AeInfo.minAGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.minDGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        calibrationSensor.config.ec.config.maxGain =
            ((float)AeInfo.maxAGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.maxDGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
            
    } else if (SensorInt.expoFrmType == ISI_EXPO_FRAME_TYPE_2FRAMES) {
        calibrationSensor.config.ec.config.integrationTime =
            (float)SensorInt.IntegrationTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.gain =
            (float)SensorGain.gain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.minInt =
            (float)AeInfo.minIntTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.maxInt = 
            (float)AeInfo.maxIntTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.minGain = 
            ((float)AeInfo.minAGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.minDGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        calibrationSensor.config.ec.config.maxGain =
            ((float)AeInfo.maxAGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.maxDGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
    } else if (SensorInt.expoFrmType == ISI_EXPO_FRAME_TYPE_3FRAMES) {
        calibrationSensor.config.ec.config.integrationTime =
            (float)SensorInt.IntegrationTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.gain =
            (float)SensorGain.gain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.minInt =
            (float)AeInfo.minIntTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.maxInt = 
            (float)AeInfo.maxIntTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        calibrationSensor.config.ec.config.minGain = 
            ((float)AeInfo.minAGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.minDGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        calibrationSensor.config.ec.config.maxGain =
            ((float)AeInfo.maxAGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.maxDGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
    } 
    config = calibrationSensor.config.ec.config;

    return RET_SUCCESS;
}

int32_t SensorOps::ecConfigSet(camdev::CalibEc::Config config) {
    uint64_t Exposure = 0;
    IsiSensorMode_t SensorMode;
    RESULT result = IsiGetSensorModeIss(hSensor,&SensorMode);
    if (result != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s IsiGetSensorModeIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    if (config.hdrRatio <= 1) {
        config.hdrRatio = (float)SensorMode.ae_info.hdr_ratio.ratio_s_vs / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
    }

    uint32_t SensorHdrRatio[2];
    if (SensorMode.hdr_mode != SENSOR_MODE_LINEAR) {
        switch(SensorMode.stitching_mode) {
            case SENSOR_STITCHING_DUAL_DCG:
            case SENSOR_STITCHING_3DOL:
            case SENSOR_STITCHING_LINEBYLINE:
                SensorHdrRatio[0] = config.hdrRatio * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorHdrRatio[1] = config.hdrRatio * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
                result = IsiSetHdrRatioIss(hSensor, 2, SensorHdrRatio);
                break;
            case SENSOR_STITCHING_16BIT_COMPRESS:
            case SENSOR_STITCHING_DUAL_DCG_NOWAIT:
            case SENSOR_STITCHING_2DOL:
            case SENSOR_STITCHING_L_AND_S:
                 SensorHdrRatio[0] = config.hdrRatio * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
                 SensorHdrRatio[1] = 0;
                 result = IsiSetHdrRatioIss(hSensor, 1, SensorHdrRatio);
                break;
            default:
                break;
        }
        if (result != RET_SUCCESS) {
            TRACE(CITF_ERR, "%s IsiSetHdrRatioIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
            return result;
        }
    }

    IsiSensorAeInfo_t  AeInfo;
    result = IsiGetAeInfoIss(hSensor, &AeInfo);
    if ( result != RET_SUCCESS ) {
        TRACE(CITF_ERR, "%s IsiGetAeInfoIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    IsiSensorIntTime_t SensorInt;
    IsiSensorGain_t    SensorGain;
    if (SensorMode.hdr_mode == SENSOR_MODE_LINEAR) {
        SensorInt.expoFrmType = ISI_EXPO_FRAME_TYPE_1FRAME;
        SensorGain.expoFrmType = ISI_EXPO_FRAME_TYPE_1FRAME;
        SensorInt.IntegrationTime.linearInt = config.integrationTime * (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        SensorGain.gain.linearGainParas = config.gain * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        
        SensorInt.IntegrationTime.linearInt = std::min(SensorInt.IntegrationTime.linearInt, AeInfo.maxIntTime.linearInt);
        SensorInt.IntegrationTime.linearInt = std::max(SensorInt.IntegrationTime.linearInt, AeInfo.minIntTime.linearInt);
        SensorGain.gain.linearGainParas = 
            std::min(SensorGain.gain.linearGainParas,(AeInfo.maxAGain.linearGainParas *  AeInfo.maxDGain.linearGainParas) >> ISI_EXPO_PARAS_FIX_FRACBITS);
        SensorGain.gain.linearGainParas = 
            std::max(SensorGain.gain.linearGainParas,(AeInfo.minAGain.linearGainParas *  AeInfo.minDGain.linearGainParas) >> ISI_EXPO_PARAS_FIX_FRACBITS);
        
    } else {
        switch(SensorMode.stitching_mode) {
            case SENSOR_STITCHING_DUAL_DCG:
            case SENSOR_STITCHING_3DOL:
            case SENSOR_STITCHING_LINEBYLINE:
                SensorInt.expoFrmType = ISI_EXPO_FRAME_TYPE_3FRAMES;
                SensorGain.expoFrmType = ISI_EXPO_FRAME_TYPE_3FRAMES;

                SensorInt.IntegrationTime.triInt.triIntTime =
                    config.integrationTime * (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.triGainParas.triGain =
                    config.gain * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);

                SensorInt.IntegrationTime.triInt.triIntTime =
                    std::min(SensorInt.IntegrationTime.triInt.triIntTime, AeInfo.maxIntTime.triInt.triIntTime);
                SensorInt.IntegrationTime.triInt.triIntTime =
                    std::max(SensorInt.IntegrationTime.triInt.triIntTime, AeInfo.minIntTime.triInt.triIntTime);
                SensorGain.gain.triGainParas.triGain =
                    std::min(SensorGain.gain.triGainParas.triGain,
                    (AeInfo.maxAGain.triGainParas.triGain *
                     AeInfo.maxDGain.triGainParas.triGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.triGainParas.triGain =
                    std::max(SensorGain.gain.triGainParas.triGain,
                    (AeInfo.minAGain.triGainParas.triGain *
                     AeInfo.minDGain.triGainParas.triGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);

                Exposure =  (uint64_t)SensorInt.IntegrationTime.triInt.triIntTime * SensorGain.gain.triGainParas.triGain;

                SensorInt.IntegrationTime.triInt.triSIntTime = SensorInt.IntegrationTime.triInt.triIntTime / config.hdrRatio;
                SensorInt.IntegrationTime.triInt.triSIntTime =
                    std::min(SensorInt.IntegrationTime.triInt.triSIntTime, AeInfo.maxIntTime.triInt.triSIntTime);
                SensorInt.IntegrationTime.triInt.triSIntTime =
                    std::max(SensorInt.IntegrationTime.triInt.triSIntTime, AeInfo.minIntTime.triInt.triSIntTime);
                SensorGain.gain.triGainParas.triSGain = Exposure / (SensorInt.IntegrationTime.triInt.triSIntTime * config.hdrRatio);
                SensorGain.gain.triGainParas.triSGain =
                    std::min(SensorGain.gain.triGainParas.triSGain,
                    (AeInfo.maxAGain.triGainParas.triSGain *
                     AeInfo.maxDGain.triGainParas.triSGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.triGainParas.triSGain =
                    std::max(SensorGain.gain.triGainParas.triSGain,
                    (AeInfo.minAGain.triGainParas.triSGain *
                     AeInfo.minDGain.triGainParas.triSGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                if (SensorMode.stitching_mode == SENSOR_STITCHING_DUAL_DCG) {
                    SensorInt.IntegrationTime.triInt.triLIntTime = SensorInt.IntegrationTime.triInt.triIntTime;
                } else {
                    SensorInt.IntegrationTime.triInt.triLIntTime = SensorInt.IntegrationTime.triInt.triIntTime * config.hdrRatio;
                }
                SensorInt.IntegrationTime.triInt.triLIntTime =
                    std::min(SensorInt.IntegrationTime.triInt.triLIntTime, AeInfo.maxIntTime.triInt.triLIntTime);
                SensorInt.IntegrationTime.triInt.triLIntTime =
                    std::max(SensorInt.IntegrationTime.triInt.triLIntTime, AeInfo.minIntTime.triInt.triLIntTime);
                SensorGain.gain.triGainParas.triLGain = (Exposure * config.hdrRatio) / (SensorInt.IntegrationTime.triInt.triLIntTime);
                SensorGain.gain.triGainParas.triLGain =
                    std::min(SensorGain.gain.triGainParas.triLGain,
                    (AeInfo.maxAGain.triGainParas.triLGain *
                     AeInfo.maxDGain.triGainParas.triLGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.triGainParas.triLGain =
                    std::max(SensorGain.gain.triGainParas.triLGain,
                    (AeInfo.minAGain.triGainParas.triLGain *
                    AeInfo.minDGain.triGainParas.triLGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                break;
            case SENSOR_STITCHING_2DOL:
            case SENSOR_STITCHING_16BIT_COMPRESS:
            case SENSOR_STITCHING_DUAL_DCG_NOWAIT:
            case SENSOR_STITCHING_L_AND_S:
                SensorInt.expoFrmType = ISI_EXPO_FRAME_TYPE_2FRAMES;
                SensorGain.expoFrmType = ISI_EXPO_FRAME_TYPE_2FRAMES;
                SensorInt.IntegrationTime.dualInt.dualIntTime =
                    config.integrationTime * (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.dualGainParas.dualGain =
                    config.gain * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
                
                SensorInt.IntegrationTime.dualInt.dualIntTime =
                    std::min(SensorInt.IntegrationTime.dualInt.dualIntTime, AeInfo.maxIntTime.dualInt.dualIntTime);
                SensorInt.IntegrationTime.dualInt.dualIntTime =
                    std::max(SensorInt.IntegrationTime.dualInt.dualIntTime, AeInfo.minIntTime.dualInt.dualIntTime);
                SensorGain.gain.dualGainParas.dualGain =
                    std::min( SensorGain.gain.dualGainParas.dualGain,
                    (AeInfo.maxAGain.dualGainParas.dualGain *
                     AeInfo.maxDGain.dualGainParas.dualGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.dualGainParas.dualGain =
                    std::max( SensorGain.gain.dualGainParas.dualGain,
                    (AeInfo.minAGain.dualGainParas.dualGain *
                     AeInfo.minDGain.dualGainParas.dualGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                Exposure = (uint64_t)SensorInt.IntegrationTime.dualInt.dualIntTime * SensorGain.gain.dualGainParas.dualGain;
                if (SensorMode.stitching_mode == SENSOR_STITCHING_DUAL_DCG_NOWAIT) {
                    SensorInt.IntegrationTime.dualInt.dualSIntTime = SensorInt.IntegrationTime.dualInt.dualIntTime ;
                } else {
                    SensorInt.IntegrationTime.dualInt.dualSIntTime = SensorInt.IntegrationTime.dualInt.dualIntTime / config.hdrRatio;
                }
                SensorInt.IntegrationTime.dualInt.dualSIntTime =
                    std::min(SensorInt.IntegrationTime.dualInt.dualSIntTime, AeInfo.maxIntTime.dualInt.dualSIntTime);
                SensorInt.IntegrationTime.dualInt.dualSIntTime =
                    std::max(SensorInt.IntegrationTime.dualInt.dualSIntTime, AeInfo.minIntTime.dualInt.dualSIntTime);
                SensorGain.gain.dualGainParas.dualSGain = (Exposure / config.hdrRatio) /  SensorInt.IntegrationTime.dualInt.dualSIntTime;
                SensorGain.gain.dualGainParas.dualSGain =
                    std::min( SensorGain.gain.dualGainParas.dualSGain,
                    (AeInfo.maxAGain.dualGainParas.dualSGain *
                     AeInfo.maxDGain.dualGainParas.dualSGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                SensorGain.gain.dualGainParas.dualSGain =
                    std::max( SensorGain.gain.dualGainParas.dualSGain,
                    (AeInfo.minAGain.dualGainParas.dualSGain *
                     AeInfo.minDGain.dualGainParas.dualSGain) >> ISI_EXPO_PARAS_FIX_FRACBITS);
                
                break;
            default:
                break;
        }
    }
    result = IsiSetIntegrationTimeIss(hSensor, &SensorInt);
    if (result != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s IsiSetIntegrationTimeIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    result = IsiSetGainIss(hSensor, &SensorGain);
    if (result != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s IsiSetGainIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    calibrationSensor.config.ec.config = config;

    return RET_SUCCESS;
}

int32_t SensorOps::ecStatusGet(camdev::CalibEc::Status &status) {

    IsiSensorMode_t SensorMode;
    IsiSensorAeInfo_t  AeInfo;
    int32_t result = IsiGetSensorModeIss(hSensor,&SensorMode);
    if (result != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s IsiGetSensorModeIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }
    result = IsiGetAeInfoIss(hSensor, &AeInfo);
    if ( result != RET_SUCCESS ) {
        TRACE(CITF_ERR, "%s IsiGetAeInfoIss failed, ret is %d\n", __PRETTY_FUNCTION__, result);
        return result;
    }

    if (SensorMode.hdr_mode == SENSOR_MODE_LINEAR) {
        status.integrationTime.min  = (float)AeInfo.minIntTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        status.integrationTime.max  = (float)AeInfo.maxIntTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        status.integrationTime.step = (float)AeInfo.oneLineExpTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        status.gain.min  =
            ((float)AeInfo.minAGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.minDGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        status.gain.max  =
            ((float)AeInfo.maxAGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
            ((float)AeInfo.maxDGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        status.gain.step = (float)AeInfo.gainStep / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        
    } else {
        switch(SensorMode.stitching_mode) {
            case SENSOR_STITCHING_DUAL_DCG:
            case SENSOR_STITCHING_3DOL:
            case SENSOR_STITCHING_LINEBYLINE:
                status.integrationTime.min  = (float)AeInfo.minIntTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS); 
                status.integrationTime.max  = (float)AeInfo.maxIntTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                status.integrationTime.step = (float)AeInfo.oneLineExpTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                status.gain.min  =
                    ((float)AeInfo.minAGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
                    ((float)AeInfo.minDGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
                status.gain.max  =
                    ((float)AeInfo.minAGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
                    ((float)AeInfo.minDGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
                status.gain.step = (float)AeInfo.gainStep / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
                break;
            case SENSOR_STITCHING_2DOL:
            case SENSOR_STITCHING_16BIT_COMPRESS:
            case SENSOR_STITCHING_DUAL_DCG_NOWAIT:
            case SENSOR_STITCHING_L_AND_S:
                status.integrationTime.min  = (float)AeInfo.minIntTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                status.integrationTime.max  = (float)AeInfo.maxIntTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                status.integrationTime.step = (float)AeInfo.oneLineExpTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
                status.gain.min  =
                    ((float)AeInfo.minAGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
                    ((float)AeInfo.minDGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
                status.gain.max  =
                    ((float)AeInfo.maxAGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
                    ((float)AeInfo.maxDGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
                status.gain.step = (float)AeInfo.gainStep / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
                break;
            default:
                break;
        }
    }
    return RET_SUCCESS;
}

int32_t SensorOps::focusGet(uint &focus) {

    return RET_SUCCESS;
}

int32_t SensorOps::focusSet(uint focus) {

    return RET_SUCCESS;
}

int32_t
SensorOps::illuminationProfilesGet(std::vector<CamIlluProfile_t *> &profiles) {
    profiles.clear();

    CamCalibDbHandle_t calibDbHandle = calibDb.GetCalibDbHandle();
    int32_t count = 0;

    int32_t ret = CamCalibDbGetNoOfIlluminations(calibDbHandle, &count);
    REPORT(ret);

    for (int32_t i = 0; i < count; ++i) {
        CamIlluProfile_t *pIlluProfile = NULL;

        ret = CamCalibDbGetIlluminationByIdx(calibDbHandle, i, &pIlluProfile);
        DCT_ASSERT(ret == RET_SUCCESS);

        profiles.push_back(pIlluProfile);
    }

    return RET_SUCCESS;
}

bool SensorOps::isConnected() {
    int32_t ret = IsiCheckSensorConnectionIss(hSensor);

    return ret == RET_SUCCESS ? BOOL_TRUE : BOOL_FALSE;
}

bool SensorOps::isTestPattern() { return calibrationSensor.config.isTestPattern; }

int32_t SensorOps::maxTestPatternGet() {
#ifndef REST_SIMULATE
  return ISI_TPG_MAX;
#endif
}

int32_t SensorOps::nameGet(std::string &name) {
    if (!pCamDrvConfig) {
        return RET_NOTAVAILABLE;
    }

    name = pCamDrvConfig->IsiSensor.pszName;

    return RET_SUCCESS;
}

int32_t SensorOps::registerDescriptionGet(uint addr, IsiRegDescription_t &description) {
    const IsiRegDescription_t *ptReg = pRegisterTable;

    while (ptReg->Flags != eTableEnd) {
        if (ptReg->Addr == addr) {
            description = *ptReg;

            return RET_SUCCESS;
        }

        ++ptReg;
    }

    REPORT(RET_FAILURE);

    return RET_SUCCESS;
}

int32_t SensorOps::revisionGet(uint &revId) {
    int32_t ret = IsiGetSensorRevisionIss(hSensor, &revId);
    REPORT(ret);

    return RET_SUCCESS;
}


#ifdef SUBDEV_V4L2
#include <linux/videodev2.h>
#include <linux/media.h>
#endif

int32_t SensorOps::open() {
    int32_t ret = RET_SUCCESS;

    HalGetSensorDrvName(pHalHolder->hHal, szDriverName, 256);
    if(!strlen(szDriverName)) {
        TRACE(CITF_ERR, "%s error(L%d): name of sensor drv is empty\n", __func__, __LINE__);
        return RET_INVALID_PARM;
    }

    HalGetSensorCalibXmlName(pHalHolder->hHal, szCalibFile, 256);
    if(!strlen(szCalibFile)) {
        TRACE(CITF_ERR, "%s error(L%d): name of calibration xml is empty\n", __func__, __LINE__);
        return RET_INVALID_PARM;
    }

    ret = driverChange(szDriverName, szCalibFile);
    if (ret != RET_SUCCESS) {
        TRACE(CITF_ERR, "%s driverChange failed, ret is %d\n", __func__, ret);
        return ret;
    }

    IsiSensorInstanceConfig_t sensorInstanceConfig;
    REFSET(sensorInstanceConfig, 0);

    IsiSensorModeInfoArray_t SensorModeArry;
    DCT_ASSERT(!pCamDrvConfig->pIsiHalQuerySensor(pHalHolder->hHal, &SensorModeArry));
    DCT_ASSERT(!HalGetSensorCurrMode(pHalHolder->hHal, &(sensorInstanceConfig.SensorModeIndex)));

    sensorInstanceConfig.HalHandle = pHalHolder->hHal;
    sensorInstanceConfig.pSensor = &pCamDrvConfig->IsiSensor;
    ret = IsiCreateSensorIss(&sensorInstanceConfig);
    REPORT(ret);

    hSensor = sensorInstanceConfig.hSensor;
    state = Idle;

    ret =IsiGetSensorModeIss(hSensor,&mode);
    REPORT(ret);

    SensorHdrMode = mode.hdr_mode;
    SensorHdrStichMode = mode.stitching_mode;

    IsiGetCapsIss(hSensor,&config);

    switch (mode.bayer_pattern) {
        case BAYER_BGGR:
            if (mode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SBGGR10;
            else
                csiFormat = V4L2_PIX_FMT_SBGGR12;
            break;
        case BAYER_GBRG:
            if (mode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SGBRG10;
            else
                csiFormat = V4L2_PIX_FMT_SGBRG12;
            break;
        case BAYER_GRBG:
            if (mode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SGRBG10;
            else
                csiFormat = V4L2_PIX_FMT_SGRBG12;
            break;
        case BAYER_RGGB:
            if (mode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SRGGB10;
            else
                csiFormat = V4L2_PIX_FMT_SRGGB12;
            break;
        default:
            csiFormat = V4L2_PIX_FMT_SBGGR12;
            break;
    }

    if (mode.data_compress.enable)
    {
        sensor_expand_curve_t sensor_expand_curve;

        sensor_expand_curve.x_bit = mode.data_compress.y_bit;
        sensor_expand_curve.y_bit = mode.data_compress.x_bit;
        IsiSensorGetExpandCurve(hSensor,&sensor_expand_curve);
        expand_curve.enable = 1;
        expand_curve.in_bit = sensor_expand_curve.x_bit;
        expand_curve.out_bit = sensor_expand_curve.y_bit;
        memcpy(expand_curve.px, sensor_expand_curve.expand_px,sizeof(sensor_expand_curve.expand_px));
        memcpy(expand_curve.x_data, sensor_expand_curve.expand_x_data,sizeof(sensor_expand_curve.expand_x_data));
        memcpy(expand_curve.y_data, sensor_expand_curve.expand_y_data,sizeof(sensor_expand_curve.expand_y_data));

    }else{
        expand_curve.enable = 0;
    }

    memset(&compress_curve, 0, sizeof(compress_curve));
    if (mode.data_compress.enable || (mode.hdr_mode == SENSOR_MODE_HDR_STITCH)) {
        IsiSensorCompressCurve_t CompressCurve;
        memset(&CompressCurve, 0 ,sizeof(CompressCurve));
        if (mode.hdr_mode == SENSOR_MODE_HDR_STITCH) {
            switch (mode.stitching_mode){
                case SENSOR_STITCHING_DUAL_DCG:
                case SENSOR_STITCHING_3DOL:
                case SENSOR_STITCHING_LINEBYLINE:
                    CompressCurve.x_bit = 20;
                    CompressCurve.y_bit = 12;
                    break;
                case SENSOR_STITCHING_2DOL:
                case SENSOR_STITCHING_16BIT_COMPRESS:
                case SENSOR_STITCHING_DUAL_DCG_NOWAIT:
                case SENSOR_STITCHING_L_AND_S:
                    CompressCurve.x_bit = 16;
                    CompressCurve.y_bit = 12;
                    break;
                default:
                    CompressCurve.x_bit = 20;
                    CompressCurve.y_bit = 12;
                    break;
            }
        } else {
            CompressCurve.x_bit =  mode.data_compress.x_bit;
            CompressCurve.y_bit = 12;
        }

        ret = IsiSensorGetCompressCure(hSensor,&CompressCurve);
        if (ret != RET_SUCCESS) {
            TRACE(CITF_ERR, "%s line-%d: get compress curve error\n", __func__, __LINE__);
            compress_curve.enable = 0;
        } else {
            compress_curve.enable = 1;
        }
        compress_curve.in_bit =  CompressCurve.x_bit;
        compress_curve.out_bit = CompressCurve.y_bit;
        memcpy(compress_curve.px, CompressCurve.compress_px, sizeof(CompressCurve.compress_px));
        memcpy(compress_curve.x_data, CompressCurve.compress_x_data, sizeof(CompressCurve.compress_x_data));
        memcpy(compress_curve.y_data, CompressCurve.compress_y_data, sizeof(CompressCurve.compress_y_data));
    }

    return RET_SUCCESS;
}

int32_t SensorOps::registerDump2File(std::string &filename) {
    int32_t ret = IsiDumpAllRegisters(hSensor, (const uint8_t *)filename.c_str());
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::registerRead(uint addr, uint &value) {
    int32_t ret = IsiReadRegister(hSensor, addr, &value);
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::registerWrite(uint addr, uint value) {
    int32_t ret = IsiWriteRegister(hSensor, addr, value);
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::reset() {
    return RET_SUCCESS;
}

int32_t SensorOps::resolutionDescriptionListGet(std::list<Resolution> &list) {
    int32_t ret = RET_SUCCESS;

    vvcam_mode_info_array_t SensorInfo;
    ret = IsiQuerySensorIss(hSensor,&SensorInfo);
    REPORT(ret);

    for (uint32_t i = 0; i < SensorInfo.count; i++)
    {
        Resolution resolution;
        char StringResolution[128];

        sprintf(StringResolution,"%dX%d",SensorInfo.modes[i].size.width,SensorInfo.modes[i].size.height);
        resolution.value = SensorInfo.modes[i].index;
        resolution.description = StringResolution;
        list.push_back(resolution);
    }

    return RET_SUCCESS;
}

int32_t SensorOps::resolutionSupportListGet(std::list<Resolution> &list) {
    int32_t ret = RET_SUCCESS;
    
    vvcam_mode_info_array_t SensorInfo;
    ret = IsiQuerySensorIss(hSensor,&SensorInfo);
    REPORT(ret);

    for (uint32_t i = 0; i < SensorInfo.count; i++)
    {
        Resolution resolution;
        char StringResolution[128];

        sprintf(StringResolution,"%dX%d",SensorInfo.modes[i].size.width,SensorInfo.modes[i].size.height);
        resolution.value = SensorInfo.modes[i].index;
        resolution.description = StringResolution;
        list.push_back(resolution);
    }

    return RET_SUCCESS;
}

int32_t SensorOps::frameRateGet(uint32_t &fps) {
    int32_t ret = RET_SUCCESS;

    uint32_t sensor_fps;
    ret = IsiGetSensorFpsIss(hSensor, &sensor_fps);
    REPORT(ret);
    fps = sensor_fps / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);

    return RET_SUCCESS;
}

int32_t SensorOps::frameRateSet(uint32_t fps) {
    int32_t ret = RET_SUCCESS;

    uint32_t sensor_fps = fps * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
    ret = IsiSetSensorFpsIss(hSensor, sensor_fps);
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::modeSet(uint32_t modeIndex) {
    int32_t ret;

    ret = HalSetSensorMode(pHalHolder->hHal, modeIndex);
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::modeGet(uint32_t &modeIndex, uint32_t &modeCount) {
    int32_t ret = RET_SUCCESS;

    ret = HalGetSensorModeInfo(pHalHolder->hHal, &modeIndex, &modeCount);
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::resolutionSet(uint16_t width, uint16_t height) {
   
    return RET_NOTSUPP;
}

int32_t SensorOps::resolutionGet(uint16_t *pwidth, uint16_t *pheight) {
    
    *pwidth = mode.size.width;
    *pheight = mode.size.height;
    return RET_SUCCESS;
}

int32_t SensorOps::setup() {
    int32_t ret = IsiSetupSensorIss(hSensor, &config);
    REPORT(ret);
    IsiSensorTpgMode_e TpgMpde;
    if (calibrationSensor.config.isTestPattern)
        TpgMpde = ISI_TPG_MODE_0;
    else
       TpgMpde = ISI_TPG_DISABLE; 

    ret = IsiActivateTestPattern(hSensor, TpgMpde);
    REPORT(ret);

    return RET_SUCCESS;
}

int32_t SensorOps::streamEnableSet(bool isEnable) {
    int32_t ret = IsiSensorSetStreamingIss(hSensor, isEnable ? BOOL_TRUE : BOOL_FALSE);
    REPORT(ret);

    state = isEnable ? Running : Idle;

    return RET_SUCCESS;
}

int32_t SensorOps::testPatternSet(uint32_t mode) {
    int32_t ret;
    IsiSensorTpgMode_e TpgMpde;

    TpgMpde = (IsiSensorTpgMode_e)mode;
    ret = IsiActivateTestPattern(hSensor, TpgMpde);
    REPORT(ret);

    if (TpgMpde == ISI_TPG_DISABLE)
        calibrationSensor.config.isTestPattern = false;
    else
        calibrationSensor.config.isTestPattern = true;

    return RET_SUCCESS;
}

int32_t SensorOps::bayerPatternSet(uint8_t pattern) {
    return RET_NOTSUPP;
}

//NOTE: we must use static function which using before pOperation->sensor initialized.
int32_t SensorOps::queryCaps(HalHolder *pHal, vvcam_mode_info_array_t *pModeArray,
                uint32_t *pCurMode, uint32_t *pDefMode) {
    void *pLib = nullptr;
    char szDriverName[256];
    RESULT result;
    result = HalGetSensorDrvName(pHal->hHal, szDriverName, 256);
    if (result != RET_SUCCESS)
        return RET_WRONG_CONFIG;
    result = HalGetSensorCurrMode(pHal->hHal, pCurMode);
    if (result != RET_SUCCESS)
        return RET_WRONG_CONFIG;
    result = HalGetSensorDefaultMode(pHal->hHal, pDefMode);
    if (result != RET_SUCCESS)
        return RET_WRONG_CONFIG;
    TRACE(CITF_INF, "%s loading [%s]...\n", __func__, szDriverName);

    DCT_ASSERT(pLib = dlopen(szDriverName, RTLD_LAZY));
    if (!pLib) {
        TRACE(CITF_ERR, "dlopen error:%s\n",dlerror());
        TRACE(CITF_ERR, "%s open drv %s failed\n", __func__, szDriverName);
        return RET_INVALID_PARM;
    }

    IsiCamDrvConfig_t * pCamDrvConfig;
    DCT_ASSERT(pCamDrvConfig = (IsiCamDrvConfig_t *)dlsym(pLib, "IsiCamDrvConfig"));
    if(!pCamDrvConfig) {
        TRACE(CITF_ERR, "%s Get IsiCamDrvConfig_t error!\n", __func__);
        dlclose(pLib);
        return RET_INVALID_PARM;
    }
    DCT_ASSERT(!pCamDrvConfig->pIsiHalQuerySensor(pHal->hHal, pModeArray));
    dlclose(pLib);
    return 0;
}

int32_t SensorOps::setCapsMode(HalHolder *pHal, uint32_t mode, const char* CalibXmlName)
{
    RESULT result;
    result = HalSetSensorMode(pHal->hHal, mode);
    if (CalibXmlName != NULL)
    {
        HalSetSensorCalibXmlName(pHal->hHal, CalibXmlName);
    }
    return result;
}

int32_t SensorOps::setCapsModeLock(HalHolder *pHal)
{
    RESULT result;
    result = HaSensorModeLock(pHal->hHal);
    return result;
}

int32_t SensorOps::getStartExposureConfig(float *pexposure)
{
    RESULT result;
    uint64_t StartExposure = 0;
    result = IsiGetAeStartExposureIss(hSensor, &StartExposure);
    *pexposure = (float)StartExposure / (1024 * 1000000);
    return result;
}

int32_t SensorOps::setStartExposureConfig(float *pexposure)
{
    RESULT result;
    uint64_t StartExposure = *pexposure * 1000000 * 1024;
    result = IsiSetAeStartExposureIss(hSensor, StartExposure);
    return result;
}

