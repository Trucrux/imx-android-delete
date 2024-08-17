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


#include "camera/abstract.hpp"
#include "calibration/ae.hpp"
#include "calibration/af.hpp"
#include "calibration/awb.hpp"
#include "calibration/calibration.hpp"
#include "calibration/dehaze.hpp"
#include "calibration/ie.hpp"
#include "calibration/images.hpp"
#include "calibration/inputs.hpp"
#include "calibration/sensors.hpp"
#include "calibration/wdr.hpp"
#include "camera/halholder.hpp"
#include "camera/mapbuffer.hpp"
#include "camera/mapcaps.hpp"
#include "common/macros.hpp"
#include <cameric_reg_drv/cameric_reg_description.h>
#include <version/version.h>

#define DEHAZE_WDR3_GAIN_MAX 64
#define DEHAZE_WDR3_STRENGTH 128
#define DEHAZE_WDR3_STRENGTH_GLOBAL 0
#define DEHAZE_WDR3_ENABLE true

#define DEHAZE_CPROC_CONTRAST 1.5
#define DEHAZE_CPROC_BRIGHTNESS -80
#define DEHAZE_CPROC_SATURATION 1.2
#define DEHAZE_CPROC_ENABLE true

#define DEHAZE_GWDR_ENABLE false

using namespace camera;

static CamEngineWbGains_t defaultWbGains = {1.0f, 1.0f, 1.0f, 1.0f};

static CamEngineCcMatrix_t defaultCcMatrix = {
    {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};

static CamEngineCcOffset_t defaultCcOffset = {0, 0, 0};

static CamEngineBlackLevel_t defaultBlvl = {0U, 0U, 0U, 0U};

Abstract::Abstract(AfpsResChangeCb_t *pAfpsResChangeCb, const void *pUserCbCtx)
    : pAfpsResChangeCb(pAfpsResChangeCb), pUserCbCtx(pUserCbCtx) {
  TRACE_IN;

  auto &clbSensors = pCalibration->module<clb::Sensors>();

  for (uint32_t i = 0; i < clbSensors.sensors.size(); i++) {
    auto &clbSensor = clbSensors.sensors[i];

    sensors.push_back(new Sensor(clbSensor, i));

    if (!clbSensor.config.driverFileName.empty()) {
      sensors[i]->driverChange(clbSensor.config.driverFileName);
    }
  }

  auto &clbImages = pCalibration->module<clb::Images>();

  for (uint32_t i = 0; i < clbImages.images.size(); i++) {
    images.push_back(new Image());

    if (!clbImages.images[i].config.fileName.empty()) {
      images[i]->load(clbImages.images[i].config.fileName);
    }
  }

  DCT_ASSERT(pEngine = new Engine());
  pEngine->pAfpsResChangeRequestCb = afpsResChangeRequestCb;
  pEngine->pUserCbCtx = this;

  state = Init;

  TRACE_OUT;
}

Abstract::~Abstract() {
  TRACE_IN;

  if (pEngine) {
    delete pEngine;
  }

  for (auto p : images) {
    delete p;
  }

  images.clear();

  for (auto p : sensors) {
    delete p;
  }

  sensors.clear();

  TRACE_OUT;
}

int32_t Abstract::afpsResChangeRequestCb(uint16_t width, uint16_t height,
                                         const void *pParam) {
  DCT_ASSERT(pParam);

  auto ret = RET_SUCCESS;

  Abstract *pCamItf = (Abstract *)pParam;

  auto oldState = pCamItf->state;

  if (oldState == Running) {
    ret = pCamItf->streamingStop();
    REPORT(ret);
  }

  ret = pCamItf->resolutionSet(width,height);
  REPORT(ret);

  if (oldState == Running) {
    ret = pCamItf->streamingStart();
    REPORT(ret);
  }

  if (pCamItf->pAfpsResChangeCb) {
    pCamItf->pAfpsResChangeCb(pCamItf->pUserCbCtx);
  }

  return RET_SUCCESS;
}

uint32_t Abstract::bitstreamId() const {
  return HalReadSysReg(pHalHolder->hHal, 0x0000);
}

int32_t Abstract::bufferMap(const MediaBuffer_t *pSrcBuffer,
                            PicBufMetaData_t *pDstBuffer) {
  DCT_ASSERT(pSrcBuffer);
  DCT_ASSERT(pDstBuffer);

  auto *pMetaData = (PicBufMetaData_t *)(pSrcBuffer->pMetaData);
  DCT_ASSERT(pMetaData);

  auto ret = PicBufIsConfigValid(pMetaData);
  REPORT(ret);

  switch (pMetaData->Type) {
  case PIC_BUF_TYPE_RAW8:
  case PIC_BUF_TYPE_RAW16:
    if (mapRawBuffer(pHalHolder->hHal, pMetaData, pDstBuffer)) {
      return RET_SUCCESS;
    }
    break;

  case PIC_BUF_TYPE_YCbCr444:
  case PIC_BUF_TYPE_YCbCr422:
  case PIC_BUF_TYPE_YCbCr420:
  case PIC_BUF_TYPE_YCbCr32:
    if (mapYCbCrBuffer(pHalHolder->hHal, pMetaData, pDstBuffer)) {
      return RET_SUCCESS;
    }
    break;

  case PIC_BUF_TYPE_RGB888:
  case PIC_BUF_TYPE_RGB32:
  case PIC_BUF_TYPE_DATA:
  case PIC_BUF_TYPE_JPEG:
  default:
    break;
  }

  return RET_FAILURE;
}

int32_t Abstract::bufferUnmap(PicBufMetaData_t *pData) {
  DCT_ASSERT(pData);

  auto ret = PicBufIsConfigValid(pData);
  REPORT(ret);

  switch (pData->Type) {
  case PIC_BUF_TYPE_RAW8:
  case PIC_BUF_TYPE_RAW16:
    if (unmapRawBuffer(pHalHolder->hHal, pData)) {
      return RET_SUCCESS;
    }
    break;

  case PIC_BUF_TYPE_YCbCr444:
  case PIC_BUF_TYPE_YCbCr422:
  case PIC_BUF_TYPE_YCbCr420:
  case PIC_BUF_TYPE_YCbCr32:
    if (unmapYCbCrBuffer(pHalHolder->hHal, pData)) {
      return RET_SUCCESS;
    }
    break;

  case PIC_BUF_TYPE_DATA:
  case PIC_BUF_TYPE_JPEG:
  case PIC_BUF_TYPE_RGB888:
  case PIC_BUF_TYPE_RGB32:
  default:
    break;
  }

  return RET_FAILURE;
}

uint32_t Abstract::camerIcId() const {
  uint32_t numRegisters = 0;
  RegDescription_t *pRegistersMap = NULL;

  auto ret = CamerIcGetRegisterDescription(CAMERIC_MODULE_MAIN_CONTROL,
                                           &numRegisters, &pRegistersMap);
  REPORT(ret);

  uint32_t id = 0;

  //ret = CamerIcGetRegister(pRegistersMap[1].Address, &id);
  //REPORT(ret);

  return id;
}

int32_t Abstract::dehazeEnableGet(bool &isEnable) {
  auto &cproc = pCalibration->module<clb::Cproc>();
  auto &wdr = pCalibration->module<clb::Wdr>();

  auto &wdrConfig = wdr.holders[clb::Wdr::V3].config;

  if (wdrConfig.v3.gainMax == DEHAZE_WDR3_GAIN_MAX &&
      wdrConfig.v3.strength == DEHAZE_WDR3_STRENGTH &&
      wdrConfig.v3.strengthGlobal == DEHAZE_WDR3_STRENGTH_GLOBAL) {
    clb::Cproc::Config cprocConfig = cproc.config;

    if (cprocConfig.config.contrast == DEHAZE_CPROC_CONTRAST &&
        cprocConfig.config.brightness == DEHAZE_CPROC_BRIGHTNESS) {
      if (wdr.holders[clb::Wdr::V1].isEnable == DEHAZE_GWDR_ENABLE) {
        isEnable = true;

        return RET_SUCCESS;
      }
    }
  }

  isEnable = false;

  return RET_SUCCESS;
}

int32_t Abstract::dehazeEnableSet(bool isEnable) {
  auto &cproc = pCalibration->module<clb::Cproc>();
  auto &dehaze = pCalibration->module<clb::Dehaze>();
  auto &wdr = pCalibration->module<clb::Wdr>();

  if (isEnable) {
    dehaze.config.pWdr->holders[clb::Wdr::V3].config =
        wdr.holders[clb::Wdr::V3].config;
    dehaze.enable.isWdr3 = wdr.holders[clb::Wdr::V3].isEnable;

    if (wdr.holders[clb::Wdr::V3].isEnable) {
      auto ret = pEngine->wdrEnableSet(false, clb::Wdr::V3);
      REPORT(ret);
    }

    clb::Wdr::Config wdrConfig;

    wdrConfig.v3.gainMax = DEHAZE_WDR3_GAIN_MAX;
    wdrConfig.v3.strength = DEHAZE_WDR3_STRENGTH;
    wdrConfig.v3.strengthGlobal = DEHAZE_WDR3_STRENGTH_GLOBAL;
    wdrConfig.v3.isAuto = false;

    auto ret = pEngine->wdrConfigSet(wdrConfig, clb::Wdr::V3);
    REPORT(ret);

    if (DEHAZE_WDR3_ENABLE != wdr.holders[clb::Wdr::V3].isEnable) {
      ret = pEngine->wdrEnableSet(DEHAZE_WDR3_ENABLE, clb::Wdr::V3);
      REPORT(ret);
    }

    dehaze.config.pCproc->config = cproc.config;
    dehaze.enable.isCproc = cproc.isEnable;

    clb::Cproc::Config cprocConfig;

    cprocConfig.config.contrast = DEHAZE_CPROC_CONTRAST;
    cprocConfig.config.brightness = DEHAZE_CPROC_BRIGHTNESS;
    cprocConfig.config.saturation = DEHAZE_CPROC_SATURATION;

    ret = pEngine->cprocConfigSet(cprocConfig);
    REPORT(ret);

    if (DEHAZE_CPROC_ENABLE != cproc.isEnable) {
      ret = pEngine->cprocEnableSet(DEHAZE_CPROC_ENABLE);
      REPORT(ret);
    }

    dehaze.enable.isGwdr = wdr.holders[clb::Wdr::V1].isEnable;

    if (DEHAZE_GWDR_ENABLE != wdr.holders[clb::Wdr::V1].isEnable) {
      ret = pEngine->wdrEnableSet(DEHAZE_GWDR_ENABLE, clb::Wdr::V1);
      REPORT(ret);
    }
  } else {
    if (wdr.holders[clb::Wdr::V3].isEnable) {
      auto ret = pEngine->wdrEnableSet(false, clb::Wdr::V3);
      REPORT(ret);
    }

    auto ret = pEngine->wdrConfigSet(
        dehaze.config.pWdr->holders[clb::Wdr::V3].config, clb::Wdr::V3);
    REPORT(ret);

    if (dehaze.enable.isWdr3 != wdr.holders[clb::Wdr::V3].isEnable) {
      ret = pEngine->wdrEnableSet(dehaze.enable.isWdr3, clb::Wdr::V3);
      REPORT(ret);
    }

    ret = pEngine->cprocConfigSet(dehaze.config.pCproc->config);
    REPORT(ret);

    if (dehaze.enable.isCproc != cproc.isEnable) {
      ret = pEngine->cprocEnableSet(dehaze.enable.isCproc);
      REPORT(ret);
    }

    if (dehaze.enable.isGwdr != wdr.holders[clb::Wdr::V1].isEnable) {
      ret = pEngine->wdrEnableSet(dehaze.enable.isGwdr, clb::Wdr::V1);
      REPORT(ret);
    }
  }

  return RET_SUCCESS;
}

int32_t Abstract::ecmSet(bool isRestoreState) {
  if (state == Invalid) {
    return RET_WRONG_STATE;
  }

  auto ret = RET_SUCCESS;

  State oldState = state;

  if (state == Running) {
    ret = streamingStop();
    REPORT(ret);
  }

  clb::Ae &ae = pCalibration->module<clb::Ae>();

  ret = pEngine->aeEcmSet(ae.ecm);
  REPORT(ret);

  if (isRestoreState && oldState == Running) {
    ret = streamingStart();
    REPORT(ret);
  }

  return RET_SUCCESS;
}

int32_t Abstract::inputConnect() {
  TRACE_IN;

  auto ret = RET_SUCCESS;

  if (state != Init) {
    REPORT(RET_WRONG_STATE);
  }

  CamEngineConfig_t *pCamEngineConfig = &pEngine->camEngineConfig;

  clb::Paths &paths = pCalibration->module<clb::Paths>();

  MEMCPY(pCamEngineConfig->pathConfig, paths.config.config,
         sizeof(CamEnginePathConfig_t) * CAMERIC_MI_PATH_MAX);

  clb::Inputs &inputs = pCalibration->module<clb::Inputs>();

  if (inputs.input().config.type == clb::Input::Sensor) {
    ret = sensor().setup();
    REPORT(ret);

    pCamEngineConfig->type = CAM_ENGINE_CONFIG_SENSOR;

    struct CamEngineConfig_s::CamEngineConfigData_u::CamEngineConfigSensor_s
        *pCamEngineConfigSensor = &pCamEngineConfig->data.sensor;

    pCamEngineConfigSensor->hSensor = sensor().hSensor;
    // pCamEngineConfigSensor->hSubSensor = sensor().holders[1]->hSensor;
    pCamEngineConfigSensor->hCamCalibDb =
        pCalibration->dbLegacy.GetCalibDbHandle();

    IsiSensorConfig_t *pSensorConfig = &sensor().config;
    IsiSensorMode_t *pSensorMode = &sensor().SensorMode;

    pCamEngineConfigSensor->ifSelect = CAMERIC_ITF_SELECT_PARALLEL;

    if (sensor().SensorHdrMode == SENSOR_MODE_HDR_STITCH) 
    {
      pCamEngineConfigSensor->ifSelect = CAMERIC_ITF_SELECT_HDR;
      pCamEngineConfigSensor->stitchingMode = sensor().SensorHdrStichMode;
    }
    if (sensor().expand_curve.enable)
    {
        pCamEngineConfigSensor->expand_curve.enable = 1;
        pCamEngineConfigSensor->expand_curve.in_bit = sensor().expand_curve.in_bit;
        pCamEngineConfigSensor->expand_curve.out_bit = sensor().expand_curve.out_bit;
        memcpy(pCamEngineConfigSensor->expand_curve.px, sensor().expand_curve.px,sizeof(sensor().expand_curve.px));
        memcpy(pCamEngineConfigSensor->expand_curve.x_data, sensor().expand_curve.x_data,sizeof(sensor().expand_curve.x_data));
        memcpy(pCamEngineConfigSensor->expand_curve.y_data, sensor().expand_curve.y_data,sizeof(sensor().expand_curve.y_data));
    }else 
    {
        pCamEngineConfigSensor->expand_curve.enable = 0;
    }

    if (sensor().compress_curve.enable) {
      pCamEngineConfigSensor->compress_curve.enable = 1;
      pCamEngineConfigSensor->compress_curve.in_bit = sensor().compress_curve.in_bit;
      pCamEngineConfigSensor->compress_curve.out_bit = sensor().compress_curve.out_bit;
      memcpy(pCamEngineConfigSensor->compress_curve.px, sensor().compress_curve.px,sizeof(sensor().compress_curve.px));
      memcpy(pCamEngineConfigSensor->compress_curve.x_data, sensor().compress_curve.x_data,sizeof(sensor().compress_curve.x_data));
      memcpy(pCamEngineConfigSensor->compress_curve.y_data, sensor().compress_curve.y_data,sizeof(sensor().compress_curve.y_data));
    } else {
      pCamEngineConfigSensor->compress_curve.enable = 0;
    }

    REFSET(pCamEngineConfigSensor->acqWindow, 0);
    pCamEngineConfigSensor->acqWindow.bounds_width  = pSensorMode->size.bounds_width;
    pCamEngineConfigSensor->acqWindow.bounds_height = pSensorMode->size.bounds_height;
    pCamEngineConfigSensor->acqWindow.hOffset       = pSensorMode->size.left;
    pCamEngineConfigSensor->acqWindow.vOffset       = pSensorMode->size.top;
    pCamEngineConfigSensor->acqWindow.width         = pSensorMode->size.width;
    pCamEngineConfigSensor->acqWindow.height        = pSensorMode->size.height;
    REFSET(pCamEngineConfigSensor->outWindow, 0);
    pCamEngineConfigSensor->outWindow.hOffset = 0;
    pCamEngineConfigSensor->outWindow.vOffset = 0;
    pCamEngineConfigSensor->outWindow.width   = pSensorMode->size.width;
    pCamEngineConfigSensor->outWindow.height  = pSensorMode->size.height;
    REFSET(pCamEngineConfigSensor->isWindow, 0);
    pCamEngineConfigSensor->isWindow.hOffset = 0;
    pCamEngineConfigSensor->isWindow.vOffset = 0;
    pCamEngineConfigSensor->isWindow.width  = pSensorMode->size.width;
    pCamEngineConfigSensor->isWindow.height = pSensorMode->size.height;
    
    isiCapValue(pCamEngineConfigSensor->sampleEdge, pSensorConfig->Edge);
    isiCapValue(pCamEngineConfigSensor->hSyncPol, pSensorConfig->HPol);
    isiCapValue(pCamEngineConfigSensor->vSyncPol, pSensorConfig->VPol);
    isiCapValue(pCamEngineConfigSensor->bayerPattern, pSensorMode->bayer_pattern);
    isiCapValue(pCamEngineConfigSensor->subSampling, pSensorConfig->Conv422);
    isiCapValue(pCamEngineConfigSensor->seqCCIR, pSensorConfig->YCSequence);
    isiCapValue(pCamEngineConfigSensor->fieldSelection, pSensorConfig->FieldSelection);
    isiCapValue(pCamEngineConfigSensor->inputSelection, pSensorMode->bit_width);
    isiCapValue(pCamEngineConfigSensor->mode, ISI_MODE_BAYER);
    if (pSensorMode->bit_width == 8){
        isiCapValue(pCamEngineConfigSensor->mipiMode, ISI_MIPI_MODE_RAW_8);
    } else if (pSensorMode->bit_width == 10) {
        isiCapValue(pCamEngineConfigSensor->mipiMode, ISI_MIPI_MODE_RAW_10);
    } else {
        isiCapValue(pCamEngineConfigSensor->mipiMode, ISI_MIPI_MODE_RAW_12);
    }
 
    pCamEngineConfigSensor->enable3D = BOOL_FALSE;
    pCamEngineConfigSensor->enableTestpattern = sensor().isTestPattern() ? BOOL_TRUE : BOOL_FALSE;
    auto &ae = pCalibration->module<clb::Ae>();
    pCamEngineConfigSensor->flickerPeriod = ae.ecm.flickerPeriod;
    pCamEngineConfigSensor->enableAfps = ae.ecm.isAfps ? BOOL_TRUE : BOOL_FALSE;

    pCamEngineConfigSensor->csiPad = 0;
    pCamEngineConfigSensor->csiFormat = sensor().csiFormat;
    
  } else if (inputs.input().config.type == clb::Input::Image) {
    pCamEngineConfig->type = CAM_ENGINE_CONFIG_IMAGE;

    struct CamEngineConfig_s::CamEngineConfigData_u::CamEngineConfigImage_s
        *pCamEngineConfigImage = &pCamEngineConfig->data.image;

    pCamEngineConfigImage->pWbGains = &defaultWbGains;
    pCamEngineConfigImage->pCcMatrix = &defaultCcMatrix;
    pCamEngineConfigImage->pCcOffset = &defaultCcOffset;
    pCamEngineConfigImage->pBlvl = &defaultBlvl;

    PicBufMetaData_t *pPicBufMetaData = &image().config.picBufMetaData;

    pCamEngineConfigImage->fieldSelection = CAMERIC_ISP_FIELD_SELECTION_BOTH;
    pCamEngineConfigImage->is_lsb_align = image().config.isLsb ? 1 : 0;

    switch (pPicBufMetaData->Type) {
    case PIC_BUF_TYPE_RAW8:
    case PIC_BUF_TYPE_RAW10:
    case PIC_BUF_TYPE_RAW16:
      pCamEngineConfigImage->pBuffer = pPicBufMetaData->Data.raw.pData;
      pCamEngineConfigImage->width = pPicBufMetaData->Data.raw.PicWidthPixel;
      pCamEngineConfigImage->height = pPicBufMetaData->Data.raw.PicHeightPixel;

      if (pPicBufMetaData->Type == PIC_BUF_TYPE_RAW8) {
        pCamEngineConfigImage->inputSelection = CAMERIC_ISP_INPUT_8BIT_EX;
      } else if (pPicBufMetaData->Type == PIC_BUF_TYPE_RAW16) {
        pCamEngineConfigImage->inputSelection = CAMERIC_ISP_INPUT_12BIT;
      } else {
        pCamEngineConfigImage->inputSelection = CAMERIC_ISP_INPUT_10BIT_EX;
      }
      break;

    default:
      DCT_ASSERT(!"Not supported buffer type");
      break;
    }
    pCamEngineConfigImage->expand_curve.enable = 0;
    pCamEngineConfigImage->compress_curve.enable = 0;

    pCamEngineConfigImage->type = pPicBufMetaData->Type;
    pCamEngineConfigImage->layout = pPicBufMetaData->Layout;
  } else {
    DCT_ASSERT(!"KEY_INPUT is NULL");
  }

  ret = pEngine->start();
  REPORT(ret);

  ret = pipelineEnableSet(true);
  REPORT(ret);

  state = Idle;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Abstract::inputDisconnect() {
  TRACE_IN;

  if (state != Idle) {
    REPORT(RET_WRONG_STATE);
  }

  auto ret = RET_SUCCESS;

  ret = pipelineEnableSet(false);
  REPORT(ret);

  ret = pEngine->stop();
  REPORT(ret);

  state = Init;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Abstract::inputSwitch(int32_t index) {
  TRACE_IN;

  if (index >= ISP_INPUT_MAX) {
    return RET_INVALID_PARM;
  }

  auto &inputs = pCalibration->module<clb::Inputs>();

  inputs.config.index = index;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Abstract::pipelineEnableSet(bool isEnable) {
  auto ret = RET_SUCCESS;

  auto &inputs = pCalibration->module<clb::Inputs>();

  if (isEnable) {
    if (inputs.input().config.type == clb::Input::Sensor) {
      try {
        auto &ae = pCalibration->module<clb::Ae>();

        ret = pEngine->aeEnableSet(ae.isEnable);
        REPORT(ret);

        if (ae.config.isBypass) {
          ret = pEngine->aeConfigGet(ae.config);
          REPORT(ret);
        } else {
          ret = pEngine->aeConfigSet(ae.config);
          REPORT(ret);
        }
        if (!ae.isEnable) {
          ret = sensor().ecConfigSet(sensor().calibrationSensor.config.ec);
          REPORT(ret);
        }
      } catch (const std::exception &e) {
        TRACE(ITF_ERR, "%s \n", e.what());
      }
      try {
        auto &awb = pCalibration->module<clb::Awb>();
        pEngine->awbSetupIsiHandle();

        ret = pEngine->awbConfigSet(awb.config);
        REPORT(ret);

        if (awb.isEnable) {
          ret = pEngine->awbEnableSet(awb.isEnable);
          REPORT(ret);
        } else {
          auto &wb = pCalibration->module<clb::Wb>();

          ret = pEngine->wbConfigSet(wb.config);
          REPORT(ret);
        }

      } catch (const std::exception &e) {
        TRACE(ITF_ERR, "%s \n", e.what());
      }

      try {
        auto isAfAvailable = false;

        ret = pEngine->afAvailableGet(isAfAvailable);
        REPORT(ret);

        if (isAfAvailable) {
          auto &af = pCalibration->module<clb::Af>();

          ret = pEngine->afEnableSet(af.isEnable);
          REPORT(ret);

          ret = pEngine->afConfigSet(af.config);
          REPORT(ret);

        }
      } catch (const std::exception &e) {
        TRACE(ITF_ERR, "%s \n", e.what());
      }
    }

    try {
      auto &dpf = pCalibration->module<clb::Dpf>();

      ret = pEngine->dpfConfigSet(dpf.config);
      REPORT(ret);

      ret = pEngine->dpfEnableSet(dpf.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      auto &dpcc = pCalibration->module<clb::Dpcc>();

      ret = pEngine->dpccEnableSet(dpcc.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_AVS
    try {
      auto &avs = pCalibration->module<clb::Avs>();

      ret = pEngine->avsConfigSet(avs.config);
      REPORT(ret);

      ret = pEngine->avsEnableSet(avs.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      auto &hdr = pCalibration->module<clb::Hdr>();

      ret = pEngine->hdrConfigSet(hdr.config);
      REPORT(ret);

      if (sensor().SensorHdrMode == SENSOR_MODE_HDR_STITCH) {
        hdr.isEnable = 1;
      }
      ret = pEngine->hdrEnableSet(hdr.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      auto &bls = pCalibration->module<clb::Bls>();

      if (bls.config.isBypass) {
        ret = pEngine->blsConfigGet(bls.config);
        REPORT(ret);
      } else {
        ret = pEngine->blsConfigSet(bls.config);
        REPORT(ret);
      }
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      auto &lsc = pCalibration->module<clb::Lsc>();

      ret = pEngine->lscEnableSet(lsc.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_WDR_V2
    try {
      auto &wdr = pCalibration->module<clb::Wdr>();

      ret = pEngine->wdrEnableSet(wdr.holders[clb::Wdr::V2].isEnable,
                                  clb::Wdr::V2);
      REPORT(ret);

      ret =
          pEngine->wdrConfigSet(wdr.holders[clb::Wdr::V2].config, clb::Wdr::V2);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_WDR_V3
    try {
      auto &wdr = pCalibration->module<clb::Wdr>();

      ret = pEngine->wdrTableSet(wdr.holders[clb::Wdr::V3].table.jTable,
                                 clb::Wdr::V3);
      REPORT(ret);

      ret =
          pEngine->wdrConfigSet(wdr.holders[clb::Wdr::V3].config, clb::Wdr::V3);
      REPORT(ret);

      ret = pEngine->wdrEnableSet(wdr.holders[clb::Wdr::V3].isEnable,
                                  clb::Wdr::V3);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      auto &demosaic = pCalibration->module<clb::Demosaic>();

      ret = pEngine->demosaicConfigSet(demosaic.config);
      REPORT(ret);

      ret = pEngine->demosaicEnableSet(demosaic.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      auto &filter = pCalibration->module<clb::Filter>();

      ret = pEngine->filterTableSet(filter.table.jTable);
      REPORT(ret);

      ret = pEngine->filterConfigSet(filter.config);
      REPORT(ret);

      ret = pEngine->filterEnableSet(filter.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      auto &cac = pCalibration->module<clb::Cac>();

      ret = pEngine->cacEnableSet(cac.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      auto &cnr = pCalibration->module<clb::Cnr>();

      ret = pEngine->cnrConfigSet(cnr.config);
      REPORT(ret);

      ret = pEngine->cnrEnableSet(cnr.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_2DNR
    try {
      auto &dnr2 = pCalibration->module<clb::Dnr2>();

      ret = pEngine->dnr2TableSet(dnr2.holders[clb::Dnr2::V1].table.jTable,
                                  clb::Dnr2::V1);
      REPORT(ret);

      ret = pEngine->dnr2ConfigSet(dnr2.holders[clb::Dnr2::V1].config,
                                   clb::Dnr2::V1);
      REPORT(ret);

      ret = pEngine->dnr2EnableSet(dnr2.holders[clb::Dnr2::V1].isEnable,
                                   clb::Dnr2::V1);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_3DNR
    try {
      auto &dnr3 = pCalibration->module<clb::Dnr3>();

      ret = pEngine->dnr3TableSet(dnr3.holders[clb::Dnr3::V1].table.jTable,
                                  clb::Dnr3::V1);
      REPORT(ret);

      ret = pEngine->dnr3ConfigSet(dnr3.holders[clb::Dnr3::V1].config,
                                   clb::Dnr3::V1);
      REPORT(ret);

      ret = pEngine->dnr3EnableSet(dnr3.holders[clb::Dnr3::V1].isEnable,
                                   clb::Dnr3::V1);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_3DNR
    try {
      auto &dnr3 = pCalibration->module<clb::Dnr3>();

      ret = pEngine->dnr3ConfigSet(dnr3.holders[clb::Dnr3::V2].config,
                                   clb::Dnr3::V2);
      REPORT(ret);

      ret = pEngine->dnr3EnableSet(dnr3.holders[clb::Dnr3::V2].isEnable,
                                   clb::Dnr3::V2);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_3DNR
    try {
      auto &dnr3 = pCalibration->module<clb::Dnr3>();

      ret = pEngine->dnr3ConfigSet(dnr3.holders[clb::Dnr3::V3].config,
                                   clb::Dnr3::V3);
      REPORT(ret);

      ret = pEngine->dnr3EnableSet(dnr3.holders[clb::Dnr3::V3].isEnable,
                                   clb::Dnr3::V3);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#if defined ISP_WDR_V1 || defined ISP_GWDR
    try {
      auto &wdr = pCalibration->module<clb::Wdr>();

      ret =
          pEngine->wdrConfigSet(wdr.holders[clb::Wdr::V1].config, clb::Wdr::V1);
      REPORT(ret);

      ret = pEngine->wdrEnableSet(wdr.holders[clb::Wdr::V1].isEnable,
                                  clb::Wdr::V1);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      auto &gc = pCalibration->module<clb::Gc>();

      ret = pEngine->gcConfigSet(gc.config);
      REPORT(ret);

      ret = pEngine->gcEnableSet(gc.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_EE
    try {
      auto &ee = pCalibration->module<clb::Ee>();

      ret = pEngine->eeTableSet(ee.table.jTable);
      REPORT(ret);

      TRACE(ITF_INF, "%s ee strength enable = %u\n", __func__, ee.isEnable);
      ret = pEngine->eeEnableSet(ee.isEnable);
      REPORT(ret);

      if (ee.isEnable) {
        TRACE(ITF_INF, "%s ee strength config = %u\n", __func__, ee.config.config.strength);
        ret = pEngine->eeConfigSet(ee.config);
        REPORT(ret);
      }
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      auto &cproc = pCalibration->module<clb::Cproc>();

      ret = pEngine->cprocConfigSet(cproc.config);
      REPORT(ret);

      ret = pEngine->cprocEnableSet(cproc.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_IE
    try {
      auto &ie = pCalibration->module<clb::Ie>();

      ret = pEngine->ieConfigSet(ie.config);
      REPORT(ret);

      ret = pEngine->ieEnableSet(ie.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_SIMP
    try {
      auto &simp = pCalibration->module<clb::Simp>();

      ret = pEngine->simpConfigSet(simp.config);
      REPORT(ret);

      ret = pEngine->simpEnableSet(simp.isEnable);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

  } else {
    pCalibration->isReadOnly = true;

#ifdef ISP_SIMP
    try {
      ret = pEngine->simpEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_IE
    try {
      ret = pEngine->ieEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      ret = pEngine->cprocEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_EE
    try {
      ret = pEngine->eeEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      ret = pEngine->gcEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#if defined ISP_WDR_V1 || defined ISP_GWDR
    try {
      ret = pEngine->wdrEnableSet(false, clb::Wdr::V1);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_3DNR
    try {
      ret = pEngine->dnr3EnableSet(false, clb::Dnr3::V3);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->dnr3EnableSet(false, clb::Dnr3::V2);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->dnr3EnableSet(false, clb::Dnr3::V1);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_2DNR
    try {
      ret = pEngine->dnr2EnableSet(false, clb::Dnr2::V1);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      ret = pEngine->cnrEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->cacEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->filterEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->demosaicEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_WDR_V3
    try {
      ret = pEngine->wdrEnableSet(false, clb::Wdr::V3);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

#ifdef ISP_WDR_V2
    try {
      ret = pEngine->wdrEnableSet(false, clb::Wdr::V2);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      ret = pEngine->lscEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->hdrEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

#ifdef ISP_AVS
    try {
      ret = pEngine->avsEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }
#endif

    try {
      ret = pEngine->dpccEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    try {
      ret = pEngine->dpfEnableSet(false);
      REPORT(ret);
    } catch (const std::exception &e) {
      TRACE(ITF_ERR, "%s \n", e.what());
    }

    auto &inputs = pCalibration->module<clb::Inputs>();

    if (inputs.input().config.type == clb::Input::Sensor) {
      auto isAfAvailable = false;

      ret = pEngine->afAvailableGet(isAfAvailable);
      REPORT(ret);

      if (isAfAvailable) {
        ret = pEngine->afEnableSet(false);
        REPORT(ret);
      }

      ret = pEngine->awbEnableSet(false);
      REPORT(ret);

      ret = pEngine->aeEnableSet(false);
      REPORT(ret);
    }

    pCalibration->isReadOnly = false;
  }

  return RET_SUCCESS;
}

int32_t Abstract::reset() {
  TRACE_IN;

  if (state != Init) {
    REPORT(RET_WRONG_STATE);
  }

  pEngine->reset();

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Abstract::resolutionGet(uint16_t &width, uint16_t &height) {
  auto &inputs = pCalibration->module<clb::Inputs>();

  if (inputs.input().config.type == clb::Input::Sensor) {
    width = sensor().SensorMode.size.width;
    height = sensor().SensorMode.size.height;

    return RET_SUCCESS;
  } else if (inputs.input().config.type == clb::Input::Image) {
    auto *pRaw = &image().config.picBufMetaData.Data.raw;

    width = pRaw->PicWidthPixel;
    height = pRaw->PicHeightPixel;

    return RET_SUCCESS;
  } else {
    return RET_FAILURE;
  }
}

int32_t Abstract::resolutionSet(uint16_t width, uint16_t height) {
  if (state != Idle) {
    return RET_WRONG_STATE;
  }

  auto ret = RET_SUCCESS;

  uint8_t numFramesToSkip = 0;

  auto &inputs = pCalibration->module<clb::Inputs>();

  if (inputs.input().config.type == clb::Input::Sensor) {
    ret = sensor().resolutionSet(width, height);
    if (ret != RET_SUCCESS) {
      TRACE(ITF_ERR, "Sensor resolution set error: %d \n", ret);
    }
  }

  CamEngineWindow_t acqWindow;
  REFSET(acqWindow, 0);
  CamEngineWindow_t outWindow;
  REFSET(outWindow, 0);
  CamEngineWindow_t isWindow;
  REFSET(isWindow, 0);
  if (inputs.input().config.type == clb::Input::Sensor) {
    IsiSensorMode_t *pSensorMode = &sensor().SensorMode;
    acqWindow.bounds_width  = pSensorMode->size.bounds_width;
    acqWindow.bounds_height = pSensorMode->size.bounds_height;
    acqWindow.hOffset       = pSensorMode->size.left;
    acqWindow.vOffset       =  pSensorMode->size.top;
    acqWindow.width         = pSensorMode->size.width;
    acqWindow.height        = pSensorMode->size.height;

    outWindow.hOffset = 0;
    outWindow.vOffset = 0;
    outWindow.width   = pSensorMode->size.width;
    outWindow.height  = pSensorMode->size.height;

    isWindow.hOffset = 0;
    isWindow.vOffset = 0;
    isWindow.width   = width;
    isWindow.height  = height;

  } else {
    acqWindow.bounds_width = width;
    acqWindow.bounds_height = height;
    acqWindow.hOffset = 0;
    acqWindow.vOffset = 0;
    acqWindow.width   = width;
    acqWindow.height  = height;

    outWindow.hOffset = 0;
    outWindow.vOffset = 0;
    outWindow.width   = width;
    outWindow.height  = height;

    isWindow.hOffset = 0;
    isWindow.vOffset = 0;
    isWindow.width   = width;
    isWindow.height  = height;
  }

  ret = pEngine->resolutionSet(acqWindow, outWindow, isWindow, numFramesToSkip);
  if (ret != RET_SUCCESS) {
    TRACE(ITF_ERR, "Engine resolution set error: %d \n", ret);
  }

  return RET_SUCCESS;
}

std::string Abstract::softwareVersion() const { return GetVersion(); }

int32_t Abstract::streamingStart(uint32_t frames) {
  TRACE_IN;

  if (state >= Running) {
    return RET_SUCCESS;
  }

  auto ret = RET_SUCCESS;

  auto &inputs = pCalibration->module<clb::Inputs>();

  ret = pEngine->streamingStart(frames);
  REPORT(ret);

  if (inputs.input().config.type == clb::Input::Sensor) {
    ret = sensor().streamEnableSet(true);
    REPORT(ret);
  }






  state = Running;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Abstract::streamingStop() {
  TRACE_IN;

  if (state <= Idle) {
    return RET_SUCCESS;
  }

  auto ret = RET_SUCCESS;

  auto &inputs = pCalibration->module<clb::Inputs>();

  if (inputs.input().config.type == clb::Input::Sensor) {
    if (sensor().state == Running) {
      ret = sensor().streamEnableSet(false);
      REPORT(ret);
    }
  }

  usleep(400000);

  if (pEngine->state == Running) {
    ret = pEngine->streamingStop();
    REPORT(ret);
  }

  state = Idle;

  TRACE_OUT;

  return RET_SUCCESS;
}

// int32_t Abstract::getDemosaicMode(CamerIcIspDemosaicMode_t &mode,
//                                    uint8_t &threshold) const {
//     mode = pEngine->pCamEngineConfig->demosaicMode;
//     threshold = pEngine->pCamEngineConfig->demosaicThreshold;
// }

// int32_t Abstract::setDemosaicMode(const CamerIcIspDemosaicMode_t &mode,
//                                    uint8_t threshold) {
//     pEngine->pCamEngineConfig->demosaicMode = mode;
//     pEngine->pCamEngineConfig->demosaicThreshold = threshold;
// }
