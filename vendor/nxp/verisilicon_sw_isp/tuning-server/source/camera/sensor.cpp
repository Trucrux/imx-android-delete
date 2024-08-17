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

#include "camera/sensor.hpp"
#include "camera/halholder.hpp"
#include "common/exception.hpp"
#include "common/filesystem.hpp"
#include "common/macros.hpp"
#include <dlfcn.h>
#include <isi/isi_iss.h>
#include <vector>
#include <linux/videodev2.h>

using namespace camera;

Sensor::Sensor(clb::Sensor &clbSensor, int32_t index) : calibrationSensor (clbSensor) {
  TRACE_IN;
  index = index;
#ifndef REST_SIMULATE
  // configure sample edge on fpga
  HalWriteSysReg(pHalHolder->hHal, 0x0004,
                 0x601); // TODO: this crude write does probably overwrite
  // vital HW control signal settings

  // reference marvin software HAL
  DCT_ASSERT(!HalAddRef(pHalHolder->hHal));
#endif

  state = Init;

  TRACE_OUT;
}

Sensor::~Sensor() {
  TRACE_IN;

  // dereference marvin software HAL
  DCT_ASSERT(!HalDelRef(pHalHolder->hHal));

  DCT_ASSERT(!close());

  if (pLib) {
    DCT_ASSERT(!dlclose(pLib));
    pLib = nullptr;
  }

  TRACE_OUT;
}

int32_t Sensor::capsGet(IsiSensorCaps_t &sensorCaps) {
#ifdef REST_SIMULATE
  sensorCaps = config;
#else
  auto ret = IsiGetCapsIss(hSensor, &sensorCaps);
  REPORT(ret);
#endif

  return RET_SUCCESS;
}

Sensor &Sensor::checkValid() {
  if (state <= Init) {
    throw exc::LogicError(RET_WRONG_STATE, "Load sensor driver firstly");
  }

  return *this;
}

int32_t Sensor::configGet(IsiSensorConfig_t &sensorConfig) {
  sensorConfig = config;
  return RET_SUCCESS;
}

int32_t Sensor::configSet(IsiSensorConfig_t &sensorConfig) {
#ifdef REST_SIMULATE
  return RET_SUCCESS;
#endif

  auto ret = IsiSetupSensorIss(hSensor, &sensorConfig);
  REPORT(ret);

  config = sensorConfig;

  return RET_SUCCESS;
}

int32_t Sensor::close() {
  if (hSensor) {
    auto ret = IsiReleaseSensorIss(hSensor);
    REPORT(ret);
  }

  return RET_SUCCESS;
}

int32_t Sensor::driverChange(std::string driverFileName) {
  if (!fs::isExists(driverFileName)) {
    throw exc::LogicError(RET_INVALID_PARM,
                          "Select sensor driver file firstly");
  }

  close();


  if (pLib) {
    DCT_ASSERT(!dlclose(pLib));
    pLib = nullptr;
  }

  pLib = dlopen(driverFileName.c_str(), RTLD_LAZY);
  if (!pLib) {
    throw exc::LogicError(RET_INVALID_PARM, dlerror());
  }

  pCamDrvConfig = (IsiCamDrvConfig_t *)dlsym(pLib, "IsiCamDrvConfig");
  if (!pCamDrvConfig) {
    throw exc::LogicError(RET_INVALID_PARM, dlerror());
  }

  DCT_ASSERT(!pCamDrvConfig->pfIsiGetSensorIss(&pCamDrvConfig->IsiSensor));

  IsiSensorModeInfoArray_t SensorModeArry;
  pCamDrvConfig->pIsiHalQuerySensor(pHalHolder->hHal,&SensorModeArry);

  char pSensorCalibXmlName[64];
  HalGetSensorCalibXmlName(pHalHolder->hHal,pSensorCalibXmlName,sizeof(pSensorCalibXmlName));
  pCalibration->restore(pSensorCalibXmlName);
  pSensor = &pCamDrvConfig->IsiSensor;

#ifndef REST_SIMULATE
  auto ret = reset();
  REPORT(ret);

  ret = open();
  REPORT(ret);

 
#endif

  auto &clbSensor = pCalibration->module<clb::Sensors>().sensors[index];
  clbSensor.config.driverFileName = driverFileName;

  return RET_SUCCESS;
}

int32_t Sensor::ecConfigGet(clb::Sensor::Config::Ec &ec) {
  IsiSensorIntTime_t SensorInt;
  IsiSensorGain_t    SensorGain;
  RESULT result = IsiGetIntegrationTimeIss(hSensor, &SensorInt);
  if (result != RET_SUCCESS) {
      return result;
  }

  result = IsiGetGainIss(hSensor, &SensorGain);
  if (result != RET_SUCCESS) {
      return result;
  }

  auto &clbEc = pCalibration->module<clb::Sensors>().sensors[index].config.ec;
  if(SensorInt.expoFrmType == ISI_EXPO_FRAME_TYPE_1FRAME) {
    clbEc.integrationTime = (float)SensorInt.IntegrationTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
    clbEc.gain = (float)SensorGain.gain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS); 
  } else if (SensorInt.expoFrmType == ISI_EXPO_FRAME_TYPE_2FRAMES) {
    clbEc.integrationTime = (float)SensorInt.IntegrationTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
    clbEc.gain = (float)SensorGain.gain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);

  } else if (SensorInt.expoFrmType == ISI_EXPO_FRAME_TYPE_3FRAMES) {
    clbEc.integrationTime = (float)SensorInt.IntegrationTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
    clbEc.gain = (float)SensorGain.gain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
  } 
  ec = clbEc;

  return RET_SUCCESS;
}

int32_t Sensor::ecConfigSet(clb::Sensor::Config::Ec ec) {
  uint64_t Exposure = 0;
  auto &hdr = pCalibration->module<clb::Hdr>();
  auto hdrRatio = static_cast<float>(hdr.config.exposureRatio);
  
  IsiSensorMode_t SensorMode;
  RESULT result = IsiGetSensorModeIss(hSensor,&SensorMode);
  if (result != RET_SUCCESS) {
    return result;
  }

  if (hdrRatio <= 1) {
    hdrRatio = (float)SensorMode.ae_info.hdr_ratio.ratio_s_vs / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
  }

  uint32_t SensorHdrRatio[2];
  if (SensorMode.hdr_mode != SENSOR_MODE_LINEAR) {
    switch(SensorMode.stitching_mode) {
      case SENSOR_STITCHING_DUAL_DCG:
      case SENSOR_STITCHING_3DOL:
      case SENSOR_STITCHING_LINEBYLINE:
        SensorHdrRatio[0] = hdrRatio * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        SensorHdrRatio[1] = hdrRatio * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        result = IsiSetHdrRatioIss(hSensor, 2, SensorHdrRatio);
        break;
      case SENSOR_STITCHING_16BIT_COMPRESS:
      case SENSOR_STITCHING_DUAL_DCG_NOWAIT:
      case SENSOR_STITCHING_2DOL:
      case SENSOR_STITCHING_L_AND_S:
        SensorHdrRatio[0] = hdrRatio * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        SensorHdrRatio[1] = 0;
        result = IsiSetHdrRatioIss(hSensor, 1, SensorHdrRatio);
        break;
      default:
          break;
    }
    if (result != RET_SUCCESS) {
      return result;
    }
  }

    IsiSensorAeInfo_t  AeInfo;
    result = IsiGetAeInfoIss(hSensor, &AeInfo);
    if ( result != RET_SUCCESS ) {
      return result;
    }

    IsiSensorIntTime_t SensorInt;
    IsiSensorGain_t    SensorGain;
    if (SensorMode.hdr_mode == SENSOR_MODE_LINEAR) {
      SensorInt.expoFrmType = ISI_EXPO_FRAME_TYPE_1FRAME;
      SensorGain.expoFrmType = ISI_EXPO_FRAME_TYPE_1FRAME;
      SensorInt.IntegrationTime.linearInt = ec.integrationTime * (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
      SensorGain.gain.linearGainParas = ec.gain * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
      
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
              ec.integrationTime * (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
            SensorGain.gain.triGainParas.triGain =
              ec.gain * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);

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

            SensorInt.IntegrationTime.triInt.triSIntTime = SensorInt.IntegrationTime.triInt.triIntTime / hdrRatio;
            SensorInt.IntegrationTime.triInt.triSIntTime =
              std::min(SensorInt.IntegrationTime.triInt.triSIntTime, AeInfo.maxIntTime.triInt.triSIntTime);
            SensorInt.IntegrationTime.triInt.triSIntTime =
             std::max(SensorInt.IntegrationTime.triInt.triSIntTime, AeInfo.minIntTime.triInt.triSIntTime);

            SensorGain.gain.triGainParas.triSGain = Exposure / (SensorInt.IntegrationTime.triInt.triSIntTime * hdrRatio);
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
              SensorInt.IntegrationTime.triInt.triLIntTime = SensorInt.IntegrationTime.triInt.triIntTime * hdrRatio;
            }
            SensorInt.IntegrationTime.triInt.triLIntTime =
             std::min(SensorInt.IntegrationTime.triInt.triLIntTime, AeInfo.maxIntTime.triInt.triLIntTime);
            SensorInt.IntegrationTime.triInt.triLIntTime =
              std::max(SensorInt.IntegrationTime.triInt.triLIntTime, AeInfo.minIntTime.triInt.triLIntTime);
            SensorGain.gain.triGainParas.triLGain = (Exposure * hdrRatio) / (SensorInt.IntegrationTime.triInt.triLIntTime);
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
              ec.integrationTime * (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
            SensorGain.gain.dualGainParas.dualGain =
              ec.gain * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);

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
              SensorInt.IntegrationTime.dualInt.dualSIntTime = SensorInt.IntegrationTime.dualInt.dualIntTime / hdrRatio;
            }
            SensorInt.IntegrationTime.dualInt.dualSIntTime =
              std::min(SensorInt.IntegrationTime.dualInt.dualSIntTime, AeInfo.maxIntTime.dualInt.dualSIntTime);
            SensorInt.IntegrationTime.dualInt.dualSIntTime =
              std::max(SensorInt.IntegrationTime.dualInt.dualSIntTime, AeInfo.minIntTime.dualInt.dualSIntTime);

            SensorGain.gain.dualGainParas.dualSGain = (Exposure / hdrRatio) /  SensorInt.IntegrationTime.dualInt.dualSIntTime;
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
        return result;
    }

    result = IsiSetGainIss(hSensor, &SensorGain);
    if (result != RET_SUCCESS) {
        return result;
    }

  auto &clbEc = pCalibration->module<clb::Sensors>().sensors[index].config.ec;

  clbEc = ec;

  return RET_SUCCESS;
}

int32_t Sensor::ecStatusGet(clb::Sensor::Status::Ec &ec) {
  IsiSensorMode_t SensorMode;
  IsiSensorAeInfo_t  AeInfo;
  int32_t result = IsiGetSensorModeIss(hSensor,&SensorMode);
  if (result != RET_SUCCESS) {
    return result;
  }
  result = IsiGetAeInfoIss(hSensor, &AeInfo);
  if ( result != RET_SUCCESS ) {
    return result;
  }

  if (SensorMode.hdr_mode == SENSOR_MODE_LINEAR) {
    ec.integrationTime.min  = (float)AeInfo.minIntTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
    ec.integrationTime.max  = (float)AeInfo.maxIntTime.linearInt / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
    ec.integrationTime.step = (float)AeInfo.oneLineExpTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
    ec.gain.min  =
      ((float)AeInfo.minAGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
      ((float)AeInfo.minDGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
    ec.gain.max  =
      ((float)AeInfo.maxAGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
      ((float)AeInfo.maxDGain.linearGainParas / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
    ec.gain.step = (float)AeInfo.gainStep / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
      
  } else {
    switch(SensorMode.stitching_mode) {
      case SENSOR_STITCHING_DUAL_DCG:
      case SENSOR_STITCHING_3DOL:
      case SENSOR_STITCHING_LINEBYLINE:
        ec.integrationTime.min  = (float)AeInfo.minIntTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS); 
        ec.integrationTime.max  = (float)AeInfo.maxIntTime.triInt.triIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        ec.integrationTime.step = (float)AeInfo.oneLineExpTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        ec.gain.min  =
          ((float)AeInfo.minAGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
          ((float)AeInfo.minDGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        ec.gain.max  =
          ((float)AeInfo.maxAGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
          ((float)AeInfo.maxDGain.triGainParas.triGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        ec.gain.step = (float)AeInfo.gainStep / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        break;
      case SENSOR_STITCHING_2DOL:
      case SENSOR_STITCHING_16BIT_COMPRESS:
      case SENSOR_STITCHING_DUAL_DCG_NOWAIT:
      case SENSOR_STITCHING_L_AND_S:
        ec.integrationTime.min  = (float)AeInfo.minIntTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        ec.integrationTime.max  = (float)AeInfo.maxIntTime.dualInt.dualIntTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        ec.integrationTime.step = (float)AeInfo.oneLineExpTime / (1000000 << ISI_EXPO_PARAS_FIX_FRACBITS);
        ec.gain.min  =
          ((float)AeInfo.minAGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
          ((float)AeInfo.minDGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        ec.gain.max  =
          ((float)AeInfo.maxAGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS)) *
          ((float)AeInfo.maxDGain.dualGainParas.dualGain / (1 << ISI_EXPO_PARAS_FIX_FRACBITS));
        ec.gain.step = (float)AeInfo.gainStep / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
        break;
      default:
          break;
    }
  }

  return RET_SUCCESS;
}

int32_t Sensor::focusGet(uint32_t &focus) {
  return RET_SUCCESS;
}

int32_t Sensor::focusSet(uint32_t focus) {
  return RET_SUCCESS;
}

int32_t
Sensor::illuminationProfilesGet(std::vector<CamIlluProfile_t *> &profiles) {
  profiles.clear();

  CamCalibDbHandle_t hDb = pCalibration->dbLegacy.GetCalibDbHandle();
  auto count = 0;

  auto ret = CamCalibDbGetNoOfIlluminations(hDb, &count);
  REPORT(ret);

  for (auto i = 0; i < count; ++i) {
    CamIlluProfile_t *pIlluProfile = NULL;

    ret = CamCalibDbGetIlluminationByIdx(hDb, i, &pIlluProfile);
    DCT_ASSERT(ret == RET_SUCCESS);

    profiles.push_back(pIlluProfile);
  }

  return RET_SUCCESS;
}

bool Sensor::isConnected() {
  auto ret = IsiCheckSensorConnectionIss(hSensor);

#ifdef REST_SIMULATE
  return BOOL_TRUE;
#endif

  return ret == RET_SUCCESS ? BOOL_TRUE : BOOL_FALSE;
}

bool Sensor::isTestPattern() {
  return pCalibration->module<clb::Sensors>()
      .sensors[index]
      .config.isTestPattern;
}

int32_t Sensor::maxTestPatternGet() {
#ifndef REST_SIMULATE
  //return IsiGetSensorMaxTesetPattern(hSensor);
  return ISI_TPG_MAX;
#endif
}

int32_t Sensor::nameGet(std::string &name) {
  if (!pCamDrvConfig) {
    return RET_NOTAVAILABLE;
  }

  name = pCamDrvConfig->IsiSensor.pszName;

  return RET_SUCCESS;
}

int32_t Sensor::registerDescriptionGet(uint32_t addr,
                                       IsiRegDescription_t &description) {
  const auto *ptReg = pRegisterTable;

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

int32_t Sensor::revisionGet(uint32_t &revId) {
  auto ret = IsiGetSensorRevisionIss(hSensor, &revId);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::open() {
  auto ret = RET_SUCCESS;

  IsiSensorInstanceConfig_t sensorInstanceConfig;
  REFSET(sensorInstanceConfig, 0);

  sensorInstanceConfig.HalHandle = pHalHolder->hHal;
  sensorInstanceConfig.pSensor = &pCamDrvConfig->IsiSensor;
  ret = HalGetSensorCurrMode(pHalHolder->hHal,&(sensorInstanceConfig.SensorModeIndex));
  REPORT(ret);

  ret = IsiCreateSensorIss(&sensorInstanceConfig);
  REPORT(ret);

  hSensor = sensorInstanceConfig.hSensor;
  state = Idle;

  ret = IsiGetSensorModeIss(hSensor,&SensorMode);
  //REPORT(ret);
  SensorHdrMode = SensorMode.hdr_mode;
  SensorHdrStichMode = SensorMode.stitching_mode;
 
  IsiGetCapsIss(hSensor,&config);

  switch (SensorMode.bayer_pattern) {
        case BAYER_BGGR:
            if (SensorMode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SBGGR10;
            else
                csiFormat = V4L2_PIX_FMT_SBGGR12;
            break;
        case BAYER_GBRG:
            if (SensorMode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SGBRG10;
            else
                csiFormat = V4L2_PIX_FMT_SGBRG12;
            break;
        case BAYER_GRBG:
            if (SensorMode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SGRBG10;
            else
                csiFormat = V4L2_PIX_FMT_SGRBG12;
            break;
        case BAYER_RGGB:
            if (SensorMode.bit_width == 10)
                csiFormat = V4L2_PIX_FMT_SRGGB10;
            else
                csiFormat = V4L2_PIX_FMT_SRGGB12;
            break;
        default:
            csiFormat = V4L2_PIX_FMT_SBGGR12;
            break;
    }

  if (SensorMode.data_compress.enable) {
    sensor_expand_curve_t sensor_expand_curve;

    sensor_expand_curve.x_bit = SensorMode.data_compress.y_bit;
    sensor_expand_curve.y_bit = SensorMode.data_compress.x_bit;
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
  if (SensorMode.data_compress.enable || (SensorMode.hdr_mode == SENSOR_MODE_HDR_STITCH)) {
    IsiSensorCompressCurve_t CompressCurve;
    memset(&CompressCurve, 0 ,sizeof(CompressCurve));
    if (SensorMode.hdr_mode == SENSOR_MODE_HDR_STITCH) {
      switch (SensorMode.stitching_mode) {
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
      CompressCurve.x_bit =  SensorMode.data_compress.x_bit;
      CompressCurve.y_bit = 12;
    }

    ret = IsiSensorGetCompressCure(hSensor,&CompressCurve);
    if (ret != RET_SUCCESS) {
        printf("%s line-%d: get compress curve error\n", __func__, __LINE__);
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

int32_t Sensor::registerDump2File(std::string &filename) {
  auto ret = IsiDumpAllRegisters(hSensor, (const uint8_t *)filename.c_str());
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::registerRead(uint32_t addr, uint32_t &value) {
  auto ret = IsiReadRegister(hSensor, addr, &value);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::registerWrite(uint32_t addr, uint32_t value) {
  auto ret = IsiWriteRegister(hSensor, addr, value);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::reset() {

  return RET_SUCCESS;
}

int32_t Sensor::resolutionDescriptionListGet(std::list<Resolution> &list) {
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

int32_t Sensor::resolutionSupportListGet(std::list<Resolution> &list) {

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

int32_t Sensor::frameRateGet(uint32_t &fps) {
  int32_t ret = RET_SUCCESS;

  uint32_t sensor_fps;
  ret = IsiGetSensorFpsIss(hSensor, &sensor_fps);
  REPORT(ret);
  fps = sensor_fps / (1 << ISI_EXPO_PARAS_FIX_FRACBITS);

  return RET_SUCCESS;
}

int32_t Sensor::frameRateSet(uint32_t fps) {
  int32_t ret = RET_SUCCESS;

  uint32_t sensor_fps = fps * (1 << ISI_EXPO_PARAS_FIX_FRACBITS);
  ret = IsiSetSensorFpsIss(hSensor, sensor_fps);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::modeGet(uint32_t &modeIndex, uint32_t &modeCount) {
  int32_t ret = RET_SUCCESS;

  ret = HalGetSensorModeInfo(pHalHolder->hHal, &modeIndex, &modeCount);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::modeSet(uint32_t modeIndex) {
  int32_t ret = RET_SUCCESS;

  ret = HalSetSensorMode(pHalHolder->hHal, modeIndex);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Sensor::resolutionSet(uint16_t width, uint16_t height) {
  return RET_NOTSUPP;
}

int32_t Sensor::setup() {
#ifndef REST_SIMULATE
  auto ret = IsiSetupSensorIss(hSensor, &config);
  REPORT(ret);

  auto &clbSensor = pCalibration->module<clb::Sensors>().sensors[index];

  IsiSensorTpgMode_e TpgMpde;
  if (clbSensor.config.isTestPattern)
    TpgMpde = ISI_TPG_MODE_0;
  else
    TpgMpde = ISI_TPG_DISABLE; 

  ret = IsiActivateTestPattern(hSensor, TpgMpde);
  REPORT(ret);
#endif

  return RET_SUCCESS;
}

int32_t Sensor::streamEnableSet(bool isEnable) {
#ifndef REST_SIMULATE
  auto ret =
      IsiSensorSetStreamingIss(hSensor, isEnable ? BOOL_TRUE : BOOL_FALSE);
  REPORT(ret);
#endif

  state = isEnable ? Running : Idle;

  return RET_SUCCESS;
}

int32_t Sensor::testPatternEnableSet(bool isEnable, int32_t curpattern) {
  int32_t ret;
  IsiSensorTpgMode_e TpgMpde;

  if (isEnable)
    TpgMpde = (IsiSensorTpgMode_e)(ISI_TPG_MODE_0 + curpattern);
  else
    TpgMpde = ISI_TPG_DISABLE;
  ret = IsiActivateTestPattern(hSensor, TpgMpde);
  REPORT(ret);

  auto &clbSensor = pCalibration->module<clb::Sensors>().sensors[index];
  clbSensor.config.isTestPattern = isEnable;

  return RET_SUCCESS;
}

