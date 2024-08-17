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

#include "camera/engine.hpp"
#include "camera/halholder.hpp"
#include "common/exception.hpp"
#include "common/macros.hpp"
#include <cameric_reg_drv/cameric_reg_description.h>
#include "../vvcam/isp/isp_version.h"

using namespace camera;

#define CAM_API_CAMENGINE_TIMEOUT 30000U // 30 s

Engine::Engine() {
  REFSET(camEngineConfig, 0);

  CamEngineInstanceConfig_t instanceConfig;
  REFSET(instanceConfig, 0);


  instanceConfig.maxPendingCommands = 4;
  instanceConfig.cbCompletion = cbCompletion;
  instanceConfig.cbAfpsResChange = afpsResChangeCb;
  instanceConfig.pUserCbCtx = (void *)this;
  instanceConfig.hHal = pHalHolder->hHal;
  instanceConfig.isSystem3D = BOOL_FALSE; // TODO:
  instanceConfig.hdr = BOOL_FALSE;

  auto ret = CamEngineInit(&instanceConfig);
  DCT_ASSERT(ret == RET_SUCCESS);

  hCamEngine = instanceConfig.hCamEngine;

  ret = osEventInit(&eventStart, 1, 0);
  ret |= osEventInit(&eventStop, 1, 0);
  ret |= osEventInit(&eventStartStreaming, 1, 0);
  ret |= osEventInit(&eventStopStreaming, 1, 0);
  ret |= osEventInit(&eventAcquireLock, 1, 0);
  ret |= osEventInit(&eventReleaseLock, 1, 0);
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osQueueInit(&queueAfpsResChange, 1, sizeof(uint32_t));
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osThreadCreate(&threadAfpsResChange, entryAfpsResChange, this);
  DCT_ASSERT(ret == OSLAYER_OK);

  state = Init;
}

Engine::~Engine() {
  auto ret = 0;

  // clean request
  do {
    uint32_t dummy = 0;

    ret = osQueueTryRead(&queueAfpsResChange, &dummy);
  } while (ret == OSLAYER_OK);

  uint32_t newRes = 0;
  ret = osQueueWrite(&queueAfpsResChange, &newRes); // send stop request
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osThreadClose(&threadAfpsResChange);
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osQueueDestroy(&queueAfpsResChange);
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osEventDestroy(&eventStart);
  ret |= osEventDestroy(&eventStop);
  ret |= osEventDestroy(&eventStartStreaming);
  ret |= osEventDestroy(&eventStopStreaming);
  ret |= osEventDestroy(&eventAcquireLock);
  ret |= osEventDestroy(&eventReleaseLock);
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = CamEngineShutDown(hCamEngine);
  DCT_ASSERT(ret == RET_SUCCESS);
}

int32_t Engine::aeConfigGet(clb::Ae::Config &config) {
  bool_t isRunning = BOOL_FALSE;
  CamEngineAecSemMode_t mode = CAM_ENGINE_AEC_SCENE_EVALUATION_INVALID;
  float setPoint = 0;
  float clmTolerance = 0;
  float dampOver = 0;
  float dampUnder = 0;
  uint8_t Weight[CAM_ENGINE_AEC_EXP_GRID_ITEMS];

  auto ret = CamEngineAecStatus(hCamEngine, &isRunning, &mode, &setPoint,
                                &clmTolerance, &dampOver, &dampUnder, Weight);
  REPORT(ret);

  clb::Ae &ae = pCalibration->module<clb::Ae>();

  ae.isEnable = isRunning == BOOL_TRUE;

  ae.config.mode = mode;
  ae.config.dampingOver = dampOver;
  ae.config.dampingUnder = dampUnder;
  ae.config.setPoint = setPoint;
  ae.config.tolerance = clmTolerance;
  memcpy(ae.config.Weight, Weight, sizeof(ae.config.Weight));

  config = ae.config;

  return RET_SUCCESS;
}

int32_t Engine::aeConfigSet(clb::Ae::Config config) {
  CamEngineAecSemMode_t mode = config.mode;
  float setPoint = config.setPoint;
  float clmTolerance = config.tolerance;
  float dampOver = config.dampingOver;
  float dampUnder = config.dampingUnder;

  auto ret = CamEngineAecConfigure(hCamEngine, mode, setPoint, clmTolerance,
                                   dampOver, dampUnder, config.Weight);
  REPORT(ret);

  if (!pCalibration->isReadOnly) {
    clb::Ae &ae = pCalibration->module<clb::Ae>();

    ae.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::aeEcmGet(clb::Ae::Ecm &ecm) {
  clb::Ae &ae = pCalibration->module<clb::Ae>();

  ecm = ae.ecm;

  return RET_SUCCESS;
}

int32_t Engine::aeEcmSet(clb::Ae::Ecm ecm) {
  auto ret = CamEngineSetEcm(hCamEngine, ecm.flickerPeriod,
                             ecm.isAfps ? BOOL_TRUE : BOOL_FALSE);
  REPORT(ret);

  if (!pCalibration->isReadOnly) {
    clb::Ae &ae = pCalibration->module<clb::Ae>();

    ae.ecm = ecm;
  }

  return RET_SUCCESS;
}

int32_t Engine::aeEnableGet(bool &isEnable) {
  bool_t isRunning = BOOL_FALSE;
  CamEngineAecSemMode_t mode;
  float setPoint = 0;
  float clmTolerance = 0;
  float dampOver = 0;
  float dampUnder = 0;
  uint8_t Weight[CAM_ENGINE_AEC_EXP_GRID_ITEMS];

  auto ret = CamEngineAecStatus(hCamEngine, &isRunning, &mode, &setPoint,
                                &clmTolerance, &dampOver, &dampUnder, Weight);
  REPORT(ret);

  isEnable = isRunning == BOOL_TRUE;

  pCalibration->module<clb::Ae>().isEnable = isEnable;

  return RET_SUCCESS;
}

int32_t Engine::aeEnableSet(bool isEnable) {
  auto ret = RET_SUCCESS;

  if (isEnable) {
    ret = CamEngineAecStart(hCamEngine);
    REPORT(ret);
  } else {
    ret = CamEngineAecStop(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    pCalibration->module<clb::Ae>().isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::aeFlickerPeriodGet(float &flickerPeriod) const {
  switch (pCalibration->module<clb::Ae>().ecm.flickerPeriod) {
  case CAM_ENGINE_FLICKER_OFF:
    flickerPeriod = (0.0000001);
    break; // this small value virtually turns of flicker avoidance

  case CAM_ENGINE_FLICKER_100HZ:
    flickerPeriod = (1.0 / 100.0);
    break;

  case CAM_ENGINE_FLICKER_120HZ:
    flickerPeriod = (1.0 / 120.0);
    break;

  default:
    flickerPeriod = (0.0000001);
    return RET_FAILURE;
  }

  return RET_SUCCESS;
}

int32_t Engine::aeStatus(clb::Ae::Status &status) {

  auto ret = CamEngineAecGetHistogram(hCamEngine, &status.histogram);
  ret |= CamEngineAecGetLuminance(hCamEngine, &status.luminance);
  ret |= CamEngineAecGetObjectRegion(hCamEngine, &status.objectRegion);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Engine::aeReset() { return CamEngineAecReset(hCamEngine); }

int32_t Engine::afAvailableGet(bool &isAvailable) {
  bool_t isAvailable2 = BOOL_FALSE;

  auto ret = CamEngineAfAvailable(hCamEngine, &isAvailable2);
  REPORT(ret);

  isAvailable = isAvailable2 == BOOL_TRUE;

  return RET_SUCCESS;
}

int32_t Engine::afConfigGet(clb::Af::Config &config) {
  bool_t isRunning = BOOL_FALSE;
  uint16_t startX, startY, width, height;

  auto ret = CamEngineAfStatus(hCamEngine, &isRunning, &config.searchAlgorithm, &config.mode, &config.pos);
  REPORT(ret);

  clb::Af &af = pCalibration->module<clb::Af>();

  // hardcode WINID, use WINA
  ret = CamEngineAfmGetMeasureWindow(hCamEngine, 1, &startX, &startY, &width, &height);
  REPORT(ret);

  af.config.searchAlgorithm = config.searchAlgorithm;
  af.config.mode = config.mode;
  af.config.pos = config.pos;
  af.isEnable = af.config.isOneshot ? af.isEnable : isRunning == BOOL_TRUE;

  af.config.win.startX = startX;
  af.config.win.startY = startY;
  af.config.win.width = width;
  af.config.win.height = height;

  config = af.config;

  return RET_SUCCESS;
}

int32_t Engine::afConfigSet(clb::Af::Config config) {
  int32_t ret = 0;
  bool winChange = false;
  if (!pCalibration->isReadOnly) {
    clb::Af &af = pCalibration->module<clb::Af>();
    if (af.isEnable) {
      if (config.mode == af.config.mode) {
        /*manual mode can change pos*/
        if (config.mode == CAM_ENGINE_AF_MODE_MANUAL) {
          ret = CamEngineAfSetPos(hCamEngine, config.pos);
          if (ret != RET_SUCCESS) {
            REPORT(ret);
          }
        }
      }

      if ( config.win.startX != af.config.win.startX ||  config.win.startY != af.config.win.startY || 
                config.win.width != af.config.win.width ||  config.win.height != af.config.win.height ) {
          winChange = true;;
        }

      /* work mode change, mode not change, but oneshot or search algorithm change, af need to reset */
      if (config.mode != af.config.mode 
            || (config.mode == CAM_ENGINE_AF_MODE_AUTO 
                && ( (config.isOneshot != af.config.isOneshot || config.searchAlgorithm != af.config.searchAlgorithm ) 
                    || winChange ) ) ) {
        ret = CamEngineAfStop(hCamEngine);
        REPORT(ret);

        if (winChange) {
          ret = CamEngineDisableAfm(hCamEngine);
          REPORT(ret);

          // hardcode WINID, use WINA
          ret = CamEngineAfmSetMeasureWindow(hCamEngine, 1, 
                      config.win.startX, config.win.startY, config.win.width, config.win.height);
          REPORT(ret);

          ret = CamEngineEnableAfm(hCamEngine);
          REPORT(ret);
        }

        af.config = config;
        ret = afEnableSet(af.isEnable);
        REPORT(ret);
      }
    }

    af.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::afEnableGet(bool &isEnable) {
  bool_t isRunning = BOOL_FALSE;
  CamEngineAfSearchAlgorithm_t searchAlgorithm;
  CamEngineAfWorkMode_t pMode;
  uint32_t pPos;
  uint16_t startX, startY, width, height;

  auto ret = CamEngineAfStatus(hCamEngine, &isRunning, &searchAlgorithm, &pMode, &pPos);
  REPORT(ret);

  clb::Af &af = pCalibration->module<clb::Af>();

  // hardcode WINID, use WINA
  ret = CamEngineAfmGetMeasureWindow(hCamEngine, 1, &startX, &startY, &width, &height);
  REPORT(ret);

  isEnable = af.config.isOneshot ? af.isEnable : isRunning == BOOL_TRUE;

  af.config.searchAlgorithm = searchAlgorithm;
  af.config.mode = pMode;
  af.config.pos = pPos;
  af.isEnable = isEnable;

  af.config.win.startX = startX;
  af.config.win.startY = startY;
  af.config.win.width = width;
  af.config.win.height = height;

  return RET_SUCCESS;
}

int32_t Engine::afEnableSet(bool isEnable) {
  clb::Af &af = pCalibration->module<clb::Af>();

  if (isEnable) {
    /*only auto mode and oneshot configuration can enable oneshot function*/
    if (af.config.isOneshot && af.config.mode == CAM_ENGINE_AF_MODE_AUTO) {
      auto ret = CamEngineAfOneShot(hCamEngine, af.config.searchAlgorithm, af.config.mode);
      REPORT(ret);
    } else {
      af.config.isOneshot = false;
      auto ret = CamEngineAfStart(hCamEngine, af.config.searchAlgorithm, af.config.mode);
      REPORT(ret);
    }

    if (af.config.mode == CAM_ENGINE_AF_MODE_MANUAL) {
      auto ret = CamEngineAfSetPos(hCamEngine, af.config.pos);
      REPORT(ret);
    }

  } else {
    auto ret = CamEngineAfStop(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    af.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

void Engine::afpsResChangeCb(uint32_t newRes, const void *pUserContext) {
  DCT_ASSERT(pUserContext);

  Engine *pEngine = (Engine *)pUserContext;

  int32_t ret = OSLAYER_OK;

  // do {
  //   uint32_t dummy;

  //   ret = osQueueTryRead(&pEngine->queueAfpsResChange, &dummy);
  // } while (ret == OSLAYER_OK);

  ret = osQueueWrite(&pEngine->queueAfpsResChange, &newRes);
  DCT_ASSERT(ret == OSLAYER_OK);
}

int32_t Engine::entryAfpsResChange(void *pParam) {
  DCT_ASSERT(pParam);

  Engine *pEngine = (Engine *)pParam;

  uint32_t newRes = 0;

  while (osQueueRead(&pEngine->queueAfpsResChange, &newRes) == OSLAYER_OK) {
    if (newRes == 0) {
      TRACE(ITF_INF, "AfpsResChangeThread (stopping)\n");
      break;
    }
  }

  TRACE(ITF_INF, "AfpsResChangeThread (stopped)\n");

  return RET_SUCCESS;
}

int32_t Engine::avsConfigGet(clb::Avs::Config &config) {
  clb::Avs &avs = pCalibration->module<clb::Avs>();

#ifdef ISP_AVS
  bool_t useParams = BOOL_FALSE;
  uint16_t numItpPoints = 0;
  float theta = 0;
  float baseGain = 0;
  float fallOff = 0;
  float acceleration = 0;
  int numDampData = 0;
  double *pDampXData = nullptr;
  double *pDampYData = nullptr;

  auto ret = CamEngineAvsGetConfig(hCamEngine, &useParams, &numItpPoints,
                                   &theta, &baseGain, &fallOff, &acceleration,
                                   &numDampData, &pDampXData, &pDampYData);
  REPORT(ret);

  avs.config.acceleration = acceleration;
  avs.config.baseGain = baseGain;
  avs.config.fallOff = fallOff;
  avs.config.isUseParams = useParams == BOOL_TRUE;
  avs.config.numItpPoints = numItpPoints;
  avs.config.theta = theta;

  avs.config.dampCount =
      numDampData < AVS_DAMP_DATA_MAX ? numDampData : AVS_DAMP_DATA_MAX;
  memcpy(avs.config.x, pDampXData, sizeof(double) * avs.config.dampCount);
  memcpy(avs.config.y, pDampYData, sizeof(double) * avs.config.dampCount);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/AVS not support");
#endif

  config = avs.config;

  return RET_SUCCESS;
}

int32_t Engine::avsConfigSet(clb::Avs::Config config) {
  clb::Avs &avs = pCalibration->module<clb::Avs>();

#ifdef ISP_AVS
  bool useParams = config.isUseParams ? BOOL_TRUE : BOOL_FALSE;
  uint16_t numItpPoints = config.numItpPoints;
  float theta = config.theta;
  float baseGain = config.baseGain;
  float fallOff = config.fallOff;
  float acceleration = config.acceleration;

  auto ret = CamEngineAvsConfigure(hCamEngine, useParams, numItpPoints, theta,
                                   baseGain, fallOff, acceleration);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/AVS not support");
#endif

  if (!pCalibration->isReadOnly) {
    avs.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::avsEnableGet(bool &isEnable) {
  clb::Avs &avs = pCalibration->module<clb::Avs>();

#ifdef ISP_AVS
  bool_t isRunning;
  CamEngineVector_t currDisplVec;
  CamEngineVector_t currOffsetVec;

  auto ret = CamEngineAvsGetStatus(hCamEngine, &isRunning, &currDisplVec,
                                   &currOffsetVec);
  REPORT(ret);

  avs.isEnable = isRunning == BOOL_TRUE;
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/AVS not support");
#endif

  isEnable = avs.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::avsEnableSet(bool isEnable) {
  clb::Avs &avs = pCalibration->module<clb::Avs>();

#ifdef ISP_AVS
  if (isEnable) {
    auto ret = CamEngineAvsStart(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineAvsStop(hCamEngine);
    REPORT(ret);
  }
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/AVS not support");
#endif

  if (!pCalibration->isReadOnly) {
    avs.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::avsStatusGet(clb::Avs::Status &status) {
#ifdef ISP_AVS
  bool_t isRunning;
  CamEngineVector_t currDisplVec;
  CamEngineVector_t currOffsetVec;

  auto ret = CamEngineAvsGetStatus(hCamEngine, &isRunning, &currDisplVec,
                                   &currOffsetVec);
  REPORT(ret);

  status.displacement.first = currDisplVec.x;
  status.displacement.second = currDisplVec.y;
  status.offset.first = currOffsetVec.x;
  status.offset.second = currOffsetVec.y;
#else
  status = status;
  throw exc::LogicError(RET_NOTSUPP, "Engine/AVS not support");
#endif

  return RET_SUCCESS;
}

int32_t Engine::awbSetupIsiHandle()
{
  return CamEngineAwbSetupIsiHandle(hCamEngine);;
}

int32_t Engine::awbConfigGet(clb::Awb::Config &config) {
  bool_t isRunning = BOOL_FALSE;

  CamEngineAwbMode_t mode = CAM_ENGINE_AWB_MODE_INVALID;
  uint32_t index = 0;
  bool_t isDamping = BOOL_FALSE;
  CamEngineAwbRgProj_t rgProj;

  auto ret = CamEngineAwbStatus(hCamEngine, &isRunning, &mode, &index, &rgProj,
                                &isDamping);
  REPORT(ret);

  clb::Awb &awb = pCalibration->module<clb::Awb>();

  awb.config.mode = mode;
  awb.config.index = index;
  awb.config.isDamping = isDamping == BOOL_TRUE;

  config = awb.config;

  return RET_SUCCESS;
}

int32_t Engine::awbConfigSet(clb::Awb::Config config) {
  if (!pCalibration->isReadOnly) {
    clb::Awb &awb = pCalibration->module<clb::Awb>();

    awb.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::awbEnableGet(bool &isEnable) {
  bool_t isRunning = BOOL_FALSE;
  CamEngineAwbMode_t mode;
  uint32_t index = 0;
  bool_t isDamping = BOOL_FALSE;
  CamEngineAwbRgProj_t rgProj;

  auto ret = CamEngineAwbStatus(hCamEngine, &isRunning, &mode, &index, &rgProj,
                                &isDamping);
  REPORT(ret);

  clb::Awb &awb = pCalibration->module<clb::Awb>();

  awb.isEnable = isRunning == BOOL_TRUE;

  awb.config.mode = mode;
  awb.config.index = index;
  awb.config.isDamping = isDamping == BOOL_TRUE;

  isEnable = awb.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::awbEnableSet(bool isEnable) {
  clb::Awb &awb = pCalibration->module<clb::Awb>();

  if (isEnable) {
    CamEngineAwbMode_t mode = awb.config.mode;
    uint32_t index = awb.config.index;
    bool_t isDamping = awb.config.isDamping ? BOOL_TRUE : BOOL_FALSE;

    auto ret = CamEngineAwbStart(hCamEngine, mode, index, isDamping);
    REPORT(ret);
  } else {
    auto ret = CamEngineAwbStop(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    awb.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::awbReset() { return CamEngineAwbReset(hCamEngine); }

int32_t Engine::awbStatusGet(clb::Awb::Status &status) {
  bool_t isRunning = BOOL_FALSE;
  CamEngineAwbMode_t mode;
  uint32_t index = 0;
  bool_t isDamping = BOOL_FALSE;
  CamEngineAwbRgProj_t rgProj;

  auto ret = CamEngineAwbStatus(hCamEngine, &isRunning, &mode, &index, &rgProj,
                                &isDamping);
  REPORT(ret);

  clb::Awb &awb = pCalibration->module<clb::Awb>();

  awb.isEnable = isRunning == BOOL_TRUE;

  awb.config.mode = mode;
  awb.config.index = index;
  awb.config.isDamping = isDamping == BOOL_TRUE;

  status.rgProj = rgProj;

  return RET_SUCCESS;
}

int32_t Engine::blsConfigGet(clb::Bls::Config &config) {
  uint16_t red = 0;
  uint16_t greenR = 0;
  uint16_t greenB = 0;
  uint16_t blue = 0;

  auto ret = CamEngineBlsGet(hCamEngine, &red, &greenR, &greenB, &blue);
  REPORT(ret);

  clb::Bls &bls = pCalibration->module<clb::Bls>();

  bls.config.blue = blue;
  bls.config.greenR = greenR;
  bls.config.greenB = greenB;
  bls.config.red = red;

  config = bls.config;

  return RET_SUCCESS;
}

int32_t Engine::blsConfigSet(clb::Bls::Config config) {
  uint16_t red = config.red;
  uint16_t greenR = config.greenR;
  uint16_t greenB = config.greenB;
  uint16_t blue = config.blue;

  auto ret = CamEngineBlsSet(hCamEngine, red, greenR, greenB, blue);
  REPORT(ret);

  if (!pCalibration->isReadOnly) {
    clb::Bls &bls = pCalibration->module<clb::Bls>();

    bls.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::bufferCbRegister(CamEngineBufferCb_t fpCallback,
                                 void *pBufferCbCtx) {
  auto ret = CamEngineRegisterBufferCb(hCamEngine, fpCallback, pBufferCbCtx);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Engine::bufferCbUnregister() {
  auto ret = CamEngineDeRegisterBufferCb(hCamEngine);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Engine::bufferCbGet(CamEngineBufferCb_t *pCallback,
                            void **ppBufferCbCtx) {
  return CamEngineGetBufferCb(hCamEngine, pCallback, ppBufferCbCtx);
}

int32_t Engine::cacEnableGet(bool &isEnable) {
  clb::Cac &cac = pCalibration->module<clb::Cac>();

  bool_t isRunning = BOOL_FALSE;
  CamEngineCacConfig_t config;

  auto ret = CamEngineCacStatus(hCamEngine, &isRunning, &config);
  REPORT(ret);

  cac.isEnable = isRunning == BOOL_TRUE;

  isEnable = cac.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::cacEnableSet(bool isEnable) {
  clb::Cac &cac = pCalibration->module<clb::Cac>();

  if (isEnable) {
    auto ret = CamEngineCacEnable(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineCacDisable(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    cac.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::cacStatusGet(clb::Cac::Status &status) {
  clb::Cac &cac = pCalibration->module<clb::Cac>();

  bool_t isRunning = BOOL_FALSE;
  CamEngineCacConfig_t config;

  auto ret = CamEngineCacStatus(hCamEngine, &isRunning, &config);
  REPORT(ret);

  cac.status.config = config;

  status = cac.status;

  return RET_SUCCESS;
}

void Engine::cbCompletion(CamEngineCmdId_t cmdId, int32_t,
                          const void *pUserContext) {
  DCT_ASSERT(pUserContext);

  Engine *pEngine = (Engine *)pUserContext;

  auto ret = RET_SUCCESS;

  switch (cmdId) {
  case CAM_ENGINE_CMD_START:
    pEngine->state = Idle;

    TRACE(ITF_INF, "CAM_ENGINE_CMD_START, ret = 0x%X \n", ret);
    ret = osEventSignal(&pEngine->eventStart);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case CAM_ENGINE_CMD_STOP:
    pEngine->state = Init;

    TRACE(ITF_INF, "CAM_ENGINE_CMD_STOP, ret = 0x%X \n", ret);
    ret = osEventSignal(&pEngine->eventStop);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case CAM_ENGINE_CMD_START_STREAMING:
    pEngine->state = Running;

    TRACE(ITF_INF, "CAM_ENGINE_CMD_START_STREAMING, ret = 0x%X \n", ret);
    ret = osEventSignal(&pEngine->eventStartStreaming);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case CAM_ENGINE_CMD_STOP_STREAMING:
    pEngine->state = Idle;

    TRACE(ITF_INF, "CAM_ENGINE_CMD_STOP_STREAMING, ret = 0x%X \n", ret);
    ret = osEventSignal(&pEngine->eventStopStreaming);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case CAM_ENGINE_CMD_ACQUIRE_LOCK:
    TRACE(ITF_INF, "CAM_ENGINE_CMD_ACQUIRE_LOCK, ret = 0x%X \n", ret);
    ret = osEventSignal(&pEngine->eventAcquireLock);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case CAM_ENGINE_CMD_RELEASE_LOCK:
    TRACE(ITF_INF, "CAM_ENGINE_CMD_RELEASE_LOCK, ret = 0x%X \n", ret);
    ret = osEventSignal(&pEngine->eventReleaseLock);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  default:
    break;
  }
}

int32_t Engine::cnrConfigGet(clb::Cnr::Config &config) {
  clb::Cnr &cnr = pCalibration->module<clb::Cnr>();

#ifdef ISP_CNR
  bool_t isRunning = BOOL_FALSE;
  uint32_t threshold1 = 0;
  uint32_t threshold2 = 0;

  auto ret =
      CamEngineCnrStatus(hCamEngine, &isRunning, &threshold1, &threshold2);
  REPORT(ret);

  cnr.config.tc1 = threshold1;
  cnr.config.tc2 = threshold2;
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/CNR not support");
#endif

  config = cnr.config;

  return RET_SUCCESS;
}

int32_t Engine::cnrConfigSet(clb::Cnr::Config config) {
  clb::Cnr &cnr = pCalibration->module<clb::Cnr>();

#ifdef ISP_CNR
  uint32_t threshold1 = config.tc1;
  uint32_t threshold2 = config.tc2;

  auto ret = CamEngineCnrSetThresholds(hCamEngine, threshold1, threshold2);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/CNR not support");
#endif

  if (!pCalibration->isReadOnly) {
    cnr.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::cnrEnableGet(bool &isEnable) {
  clb::Cnr &cnr = pCalibration->module<clb::Cnr>();

#ifdef ISP_CNR
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/CNR not support");
#endif

  isEnable = cnr.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::cnrEnableSet(bool isEnable) {
  clb::Cnr &cnr = pCalibration->module<clb::Cnr>();

#ifdef ISP_CNR
  if (isEnable) {
    auto ret = CamEngineCnrEnable(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineCnrDisable(hCamEngine);
    REPORT(ret);
  }
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/CNR not support");
#endif

  if (!pCalibration->isReadOnly) {
    cnr.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::cprocConfigGet(clb::Cproc::Config &config) {
  clb::Cproc &cproc = pCalibration->module<clb::Cproc>();

  bool_t isRunning = cproc.isEnable ? BOOL_TRUE : BOOL_FALSE;
  CamEngineCprocConfig_t cprocConfig = cproc.config.config;

  //auto ret = CamEngineCprocStatus(hCamEngine, &isRunning, &cprocConfig);
  //REPORT(ret);

  cproc.isEnable = isRunning == BOOL_TRUE;
  cproc.config.config = cprocConfig;

  config = cproc.config;

  return RET_SUCCESS;
}

int32_t Engine::cprocConfigSet(clb::Cproc::Config config) {
  auto ret = RET_SUCCESS;
  clb::Cproc &cproc = pCalibration->module<clb::Cproc>();
  CamEngineCsmConfig_t CsmConfig;

  if ((config.config.ChromaOut == CAMERIC_CPROC_CHROM_RANGE_OUT_BT601) &&
       (config.config.LumaOut == CAMERIC_CPROC_LUM_RANGE_OUT_BT601) &&
       (config.config.LumaIn == CAMERIC_CPROC_LUM_RANGE_IN_BT601)) {
    CsmConfig.colorRange = CAM_ENGINE_CSM_LIMIT_RANGE;
    CsmConfig.colorSpace = CAM_ENGINE_CSM_COLORSPACE_REC709;
    ret = CamEngineConfigCSM(hCamEngine, CsmConfig);
    REPORT(ret);
  } else if ((config.config.ChromaOut == CAMERIC_CPROC_CHROM_RANGE_OUT_FULL_RANGE) &&
       (config.config.LumaOut == CAMERIC_CPROC_LUM_RANGE_OUT_FULL_RANGE) &&
       (config.config.LumaIn == CAMERIC_CPROC_LUM_RANGE_IN_FULL_RANGE)) {
    CsmConfig.colorRange = CAM_ENGINE_CSM_FULL_RANGE;
    CsmConfig.colorSpace = CAM_ENGINE_CSM_COLORSPACE_REC709;
    ret = CamEngineConfigCSM(hCamEngine, CsmConfig);
    REPORT(ret);
  } else {
    config.config.ChromaOut = cproc.config.config.ChromaOut;
    config.config.LumaOut = cproc.config.config.LumaOut;
    config.config.LumaIn = cproc.config.config.LumaIn;
  }

  ret = CamEngineCprocSetConfig(hCamEngine, config.config);
  REPORT(ret);
  if (!pCalibration->isReadOnly) {
    cproc.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::cprocEnableGet(bool &isEnable) {
  clb::Cproc &cproc = pCalibration->module<clb::Cproc>();

  bool_t isRunning = cproc.isEnable ? BOOL_TRUE : BOOL_FALSE;
  CamEngineCprocConfig_t cprocConfig = cproc.config.config;

  auto ret = CamEngineCprocStatus(hCamEngine, &isRunning, &cprocConfig);
  REPORT(ret);

  cproc.isEnable = isRunning == BOOL_TRUE;
  cproc.config.config = cprocConfig;

  isEnable = cproc.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::cprocEnableSet(bool isEnable) {
  clb::Cproc &cproc = pCalibration->module<clb::Cproc>();

  if (isEnable) {
    auto ret = CamEngineEnableCproc(hCamEngine, &cproc.config.config);
    REPORT(ret);
  } else {
    auto ret = CamEngineDisableCproc(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    cproc.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::demosaicConfigGet(clb::Demosaic::Config &config) {
  clb::Demosaic &demosaic = pCalibration->module<clb::Demosaic>();

  bool_t isBypass = BOOL_FALSE;
  uint8_t threshold = 0;
#ifndef ISP_DEMOSAIC2

  auto ret = CamEngineDemosaicGet(hCamEngine, &isBypass, &threshold);
  REPORT(ret);
#else
  auto ret = CamEngineDemosaic2Get(hCamEngine, &isBypass, &threshold);
  REPORT(ret);
#endif
  demosaic.config.threshold = threshold;

  config = demosaic.config;

  return RET_SUCCESS;
}

int32_t Engine::demosaicConfigSet(clb::Demosaic::Config config) {
  clb::Demosaic &demosaic = pCalibration->module<clb::Demosaic>();

  bool_t isBypass = demosaic.isEnable ? BOOL_FALSE : BOOL_TRUE;
  uint8_t threshold = static_cast<uint8_t>(config.threshold);

#ifndef ISP_DEMOSAIC2
  auto ret = CamEngineDemosaicSet(hCamEngine, isBypass, threshold);
  if (ret == RET_WRONG_STATE) {
    throw exc::LogicError(ret,
                          "Demosaic wrong state: stop preview then try again");
  }
  REPORT(ret);
#else
  auto ret = CamEngineDemosaic2Set(hCamEngine, isBypass, threshold);
  if (ret == RET_WRONG_STATE) {
    throw exc::LogicError(ret,
  						"Demosaic wrong state: stop preview then try again");
  }
  REPORT(ret);
#endif

  if (!pCalibration->isReadOnly) {
    demosaic.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::demosaicEnableGet(bool &isEnable) {
  clb::Demosaic &demosaic = pCalibration->module<clb::Demosaic>();

  bool_t isBypass = BOOL_FALSE;
  uint8_t threshold = 0;

#ifndef ISP_DEMOSAIC2
  auto ret = CamEngineDemosaicGet(hCamEngine, &isBypass, &threshold);
  REPORT(ret);
#else
  auto ret = CamEngineDemosaic2Get(hCamEngine, &isBypass, &threshold);
  REPORT(ret);

#endif
  demosaic.isEnable = isBypass == BOOL_FALSE;

  isEnable = demosaic.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::demosaicEnableSet(bool isEnable) {
  clb::Demosaic &demosaic = pCalibration->module<clb::Demosaic>();

  bool_t isBypass = isEnable ? BOOL_FALSE : BOOL_TRUE;
  uint8_t threshold = static_cast<uint8_t>(demosaic.config.threshold);

#ifndef ISP_DEMOSAIC2
  auto ret = CamEngineDemosaicSet(hCamEngine, isBypass, threshold);
  REPORT(ret);
#else
  auto ret = CamEngineDemosaic2Set(hCamEngine, isBypass, threshold);
  REPORT(ret);
#endif
  if (!pCalibration->isReadOnly) {
    demosaic.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr2ConfigGet(clb::Dnr2::Config &config,
                              clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    bool_t isRunning = BOOL_FALSE;
    CamEngineA2dnrMode_t mode = CAM_ENGINE_A2DNR_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    float sigma = 0;
    uint8_t strength = 0;
    uint8_t pregamaStrength = 0;

    auto ret = CamEngineA2dnrStatus(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &sigma, &strength,
                                    &pregamaStrength);
    REPORT(ret);

    clb::Dnr2::Config::V1 &v1Config = dnr2.holders[generation].config.v1;

    v1Config.isAuto = mode == CAM_ENGINE_A2DNR_MODE_AUTO;
    v1Config.sigma = sigma;
    v1Config.denoiseStrength = strength;
    v1Config.denoisePregamaStrength = pregamaStrength;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)dnr2;
#endif
  }

  config = dnr2.holders[generation].config;

  return RET_SUCCESS;
}

int32_t Engine::dnr2ConfigSet(clb::Dnr2::Config config,
                              clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    uint8_t denoisePregamaStrength =
        static_cast<uint8_t>(config.v1.denoisePregamaStrength);
    uint8_t denoiseStrength = static_cast<uint8_t>(config.v1.denoiseStrength);
    float sigma = static_cast<float>(config.v1.sigma);

    auto ret = CamEngineA2dnrConfigure(hCamEngine, sigma, denoiseStrength,
                                       denoisePregamaStrength);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)dnr2;
#endif
  }

  if (!pCalibration->isReadOnly) {
    dnr2.holders[generation].config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr2EnableGet(bool &isEnable,
                              clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    bool_t isRunning = BOOL_FALSE;
    CamEngineA2dnrMode_t mode = CAM_ENGINE_A2DNR_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    float sigma = 0;
    uint8_t strength = 0;
    uint8_t pregamaStrength = 0;

    auto ret = CamEngineA2dnrStatus(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &sigma, &strength,
                                    &pregamaStrength);
    REPORT(ret);

    dnr2.holders[generation].isEnable = isRunning == BOOL_TRUE;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)dnr2;
#endif
  }

  isEnable = dnr2.holders[generation].isEnable;

  return RET_SUCCESS;
}

int32_t Engine::dnr2EnableSet(bool isEnable, clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    if (isEnable) {
      CamEngineA2dnrMode_t mode = dnr2.holders[clb::Dnr2::V1].config.v1.isAuto
                                      ? CAM_ENGINE_A2DNR_MODE_AUTO
                                      : CAM_ENGINE_A2DNR_MODE_MANUAL;
      auto ret = CamEngineA2dnrStart(hCamEngine, mode);
      REPORT(ret);
    } else {
      auto ret = CamEngineA2dnrStop(hCamEngine);
      REPORT(ret);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)dnr2;
#endif
  }

  if (!pCalibration->isReadOnly) {
    dnr2.holders[generation].isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr2Reset(clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    dnr2.holders[generation].config.v1.reset();

    auto ret = dnr2ConfigSet(dnr2.holders[generation].config, generation);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)dnr2;
#endif
  }

  clb::Dnr2 noUnusedValue = dnr2;

  return RET_SUCCESS;
}

int32_t Engine::dnr2StatusGet(clb::Dnr2::Status &status,
                              clb::Dnr2::Generation generation) {

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();
    bool_t isRunning = BOOL_FALSE;
    CamEngineA2dnrMode_t mode = CAM_ENGINE_A2DNR_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    float sigma = 0;
    uint8_t strength = 0;
    uint8_t pregamaStrength = 0;

    auto ret = CamEngineA2dnrStatus(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &sigma, &strength,
                                    &pregamaStrength);
    REPORT(ret);

    status.gain = gain;
    status.integrationTime = integrationTime;
    (void)dnr2;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)generation;
    (void)status;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr2TableGet(Json::Value &jTable,
                             clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();

  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    jTable = dnr2.holders[generation].table.jTable;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)jTable;
    (void)dnr2;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr2TableSet(Json::Value jTable,
                             clb::Dnr2::Generation generation) {
  clb::Dnr2 &dnr2 = pCalibration->module<clb::Dnr2>();
  if (generation == clb::Dnr2::V1) {
#if defined ISP_2DNR
    dnr2.holders[generation].table.jTable = jTable;

    Json::Value &jRows = jTable[KEY_ROWS];

    auto rowCount = jRows.size();

    auto *pNodes = static_cast<CamEngineA2dnrParamNode_t *>(
        calloc(rowCount, sizeof(CamEngineA2dnrParamNode_t)));

    auto rowCountMatchHdr = 0;

    clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

    for (uint8_t i = 0; i < rowCount; i++) {
      Json::Value &jRow = jRows[i];

      if ((hdr.isEnable && jRow[clb::Dnr2::Table::ColumnV1::Hdr].asInt()) ||
          (!hdr.isEnable && !jRow[clb::Dnr2::Table::ColumnV1::Hdr].asInt())) {
      } else {
        continue;
      }

      auto *pNode = &pNodes[rowCountMatchHdr++];

      pNode->gain = jRow[clb::Dnr2::Table::ColumnV1::Gain].asFloat();
      pNode->integrationTime =
          jRow[clb::Dnr2::Table::ColumnV1::IntegrationTime].asFloat();
      pNode->pregmaStrength =
          jRow[clb::Dnr2::Table::ColumnV1::PregammaStrength].asUInt();
      pNode->strength =
          jRow[clb::Dnr2::Table::ColumnV1::DenoiseStrength].asUInt();
      pNode->sigma = jRow[clb::Dnr2::Table::ColumnV1::Sigma].asFloat();
    }

    if (rowCountMatchHdr) {
      auto ret =
          CamEngineA2dnrSetAutoTable(hCamEngine, pNodes, rowCountMatchHdr);
      free(pNodes);

      REPORT(ret);
    } else {
      free(pNodes);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/2DNR not support");
    (void)jTable;
    (void)dnr2;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr3ConfigGet(clb::Dnr3::Config &config,
                              clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    bool_t isRunning = BOOL_FALSE;
    CamEngineA3dnrMode_t mode = CAM_ENGINE_A3DNR_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    uint8_t strength = 0;
    uint16_t motionFactor = 0;
    uint16_t deltaFactor = 0;

    auto ret = CamEngineA3dnrStatus(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &strength, &motionFactor,
                                    &deltaFactor);
    REPORT(ret);

    clb::Dnr3::Config &dnr3Config = dnr3.holders[generation].config;

    dnr3Config.v1.isAuto = mode == CAM_ENGINE_A3DNR_MODE_AUTO;
    dnr3Config.v1.strength = strength;
    dnr3Config.v1.motionFactor = motionFactor;
    dnr3Config.v1.deltaFactor = deltaFactor;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)dnr3;
#endif
  }

  config = dnr3.holders[generation].config;

  return RET_SUCCESS;
}

int32_t Engine::dnr3ConfigSet(clb::Dnr3::Config config,
                              clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    uint8_t strength = static_cast<uint8_t>(config.v1.strength);
    uint16_t motionFactor = static_cast<uint16_t>(config.v1.motionFactor);
    uint16_t deltaFactor = static_cast<uint16_t>(config.v1.deltaFactor);

    auto ret = CamEngineA3dnrConfigure(hCamEngine, strength, motionFactor,
                                       deltaFactor);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)dnr3;
#endif
  }

  if (!pCalibration->isReadOnly) {
    dnr3.holders[generation].config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr3EnableGet(bool &isEnable,
                              clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    bool_t isRunning = BOOL_FALSE;
    CamEngineA3dnrMode_t mode = CAM_ENGINE_A3DNR_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    uint8_t strength = 0;
    uint16_t motionFactor = 0;
    uint16_t deltaFactor = 0;

    auto ret = CamEngineA3dnrStatus(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &strength, &motionFactor,
                                    &deltaFactor);
    REPORT(ret);

    dnr3.holders[generation].isEnable = isRunning == BOOL_TRUE;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)dnr3;
#endif
  }

  isEnable = dnr3.holders[generation].isEnable;

  return RET_SUCCESS;
}

int32_t Engine::dnr3EnableSet(bool isEnable, clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    if (isEnable) {
      CamEngineA3dnrMode_t mode = dnr3.holders[generation].config.v1.isAuto
                                      ? CAM_ENGINE_A3DNR_MODE_AUTO
                                      : CAM_ENGINE_A3DNR_MODE_MANUAL;

      auto ret = CamEngineA3dnrStart(hCamEngine, mode);
      REPORT(ret);
    } else {
      auto ret = CamEngineA3dnrStop(hCamEngine);
      REPORT(ret);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)dnr3;
#endif
  }

  if (!pCalibration->isReadOnly) {
    dnr3.holders[generation].isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr3Reset(clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    dnr3.holders[generation].config.v1.reset();

    auto ret = dnr3ConfigSet(dnr3.holders[generation].config, generation);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)dnr3;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr3StatusGet(clb::Dnr3::Status &status,
                              clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    bool_t isRunning = BOOL_FALSE;
    CamEngineA3dnrMode_t mode = CAM_ENGINE_A3DNR_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    uint8_t strength = 0;
    uint16_t motionFactor = 0;
    uint16_t deltaFactor = 0;

    auto ret = CamEngineA3dnrStatus(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &strength, &motionFactor,
                                    &deltaFactor);
    REPORT(ret);

    status.gain = gain;
    status.integrationTime = integrationTime;
    (void)dnr3;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)dnr3;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr3TableGet(Json::Value &jTable,
                             clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    jTable = dnr3.holders[generation].table.jTable;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)jTable;
    (void)dnr3;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dnr3TableSet(Json::Value jTable,
                             clb::Dnr3::Generation generation) {
  clb::Dnr3 &dnr3 = pCalibration->module<clb::Dnr3>();

  if (generation == clb::Dnr3::V1) {
#if defined ISP_3DNR
    dnr3.holders[generation].table.jTable = jTable;

    Json::Value &jRows = jTable[KEY_ROWS];

    auto rowCount = jRows.size();

    auto *pNodes = static_cast<CamEngineA3dnrParamNode_t *>(
        calloc(rowCount, sizeof(CamEngineA3dnrParamNode_t)));

    auto rowCountMatchHdr = 0;

    clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

    for (uint8_t i = 0; i < rowCount; i++) {
      Json::Value &jRow = jRows[i];

      if ((hdr.isEnable && jRow[clb::Dnr3::Table::ColumnV1::Hdr].asInt()) ||
          (!hdr.isEnable && !jRow[clb::Dnr3::Table::ColumnV1::Hdr].asInt())) {
      } else {
        continue;
      }

      auto *pNode = &pNodes[rowCountMatchHdr++];

      pNode->gain = jRow[clb::Dnr3::Table::ColumnV1::Gain].asFloat();
      pNode->integrationTime =
          jRow[clb::Dnr3::Table::IntegrationTime].asFloat();
      pNode->strength = jRow[clb::Dnr3::Table::ColumnV1::Strength].asUInt();
      pNode->motionFactor =
          jRow[clb::Dnr3::Table::ColumnV1::MotionFactor].asUInt();
      pNode->deltaFactor =
          jRow[clb::Dnr3::Table::ColumnV1::DeltaFactor].asUInt();
    }

    if (rowCountMatchHdr) {
      auto ret =
          CamEngineA3dnrSetAutoTable(hCamEngine, pNodes, rowCountMatchHdr);
      free(pNodes);

      REPORT(ret);
    } else {
      free(pNodes);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/3DNR not support");
    (void)jTable;
    (void)dnr3;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::dpccEnableGet(bool &isEnable) {
  clb::Dpcc &dpcc = pCalibration->module<clb::Dpcc>();

  bool_t isRunning = BOOL_FALSE;

  auto ret = CamEngineAdpccStatus(hCamEngine, &isRunning);
  REPORT(ret);

  isEnable = isRunning == BOOL_TRUE;

  dpcc.isEnable = isEnable;

  return RET_SUCCESS;
}

int32_t Engine::dpccEnableSet(bool isEnable) {
  clb::Dpcc &dpcc = pCalibration->module<clb::Dpcc>();

  if (isEnable) {
    auto ret = CamEngineAdpccStart(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineAdpccStop(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    dpcc.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::dpfConfigGet(clb::Dpf::Config &config) {
  clb::Dpf &dpf = pCalibration->module<clb::Dpf>();

  bool_t isRunning = BOOL_FALSE;

  float gradient = 0;
  float offset = 0;
  float min = 0;
  float div = 0;

  uint8_t sigmaGreen = 0;
  uint8_t sigmaRedBlue = 0;

  auto ret = CamEngineAdpfStatus(hCamEngine, &isRunning, &gradient, &offset,
                                 &min, &div, &sigmaGreen, &sigmaRedBlue);
  REPORT(ret);

  dpf.isEnable = isRunning == BOOL_TRUE;

  dpf.config.divisionFactor = div;
  dpf.config.gradient = gradient;
  dpf.config.minimumBound = min;
  dpf.config.offset = offset;
  dpf.config.sigmaGreen = sigmaGreen;
  dpf.config.sigmaRedBlue = sigmaRedBlue;

  config = dpf.config;

  return RET_SUCCESS;
}

int32_t Engine::dpfConfigSet(clb::Dpf::Config config) {
  clb::Dpf &dpf = pCalibration->module<clb::Dpf>();

  float gradient = config.gradient;
  float offset = config.offset;
  float min = config.minimumBound;
  float div = config.divisionFactor;

  uint8_t sigmaGreen = config.sigmaGreen;
  uint8_t sigmaRedBlue = config.sigmaRedBlue;

  auto ret = CamEngineAdpfConfigure(hCamEngine, gradient, offset, min, div,
                                    sigmaGreen, sigmaRedBlue);
  REPORT(ret);

  if (!pCalibration->isReadOnly) {
    dpf.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::dpfEnableGet(bool &isEnable) {
  clb::Dpf &dpf = pCalibration->module<clb::Dpf>();

  bool_t isRunning = BOOL_FALSE;

  float gradient = 0;
  float offset = 0;
  float min = 0;
  float div = 0;

  uint8_t sigmaGreen = 0;
  uint8_t sigmaRedBlue = 0;

  auto ret = CamEngineAdpfStatus(hCamEngine, &isRunning, &gradient, &offset,
                                 &min, &div, &sigmaGreen, &sigmaRedBlue);
  REPORT(ret);

  dpf.isEnable = isRunning == BOOL_TRUE;

  dpf.config.divisionFactor = div;
  dpf.config.gradient = gradient;
  dpf.config.minimumBound = min;
  dpf.config.offset = offset;
  dpf.config.sigmaGreen = sigmaGreen;
  dpf.config.sigmaRedBlue = sigmaRedBlue;

  isEnable = dpf.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::dpfEnableSet(bool isEnable) {
  clb::Dpf &dpf = pCalibration->module<clb::Dpf>();

  if (isEnable) {
    auto ret = CamEngineAdpfStart(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineAdpfStop(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    dpf.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::eeConfigGet(clb::Ee::Config &config) {
  clb::Ee &ee = pCalibration->module<clb::Ee>();

#ifdef ISP_EE
  bool_t isRunning = BOOL_FALSE;
  CamEngineAeeMode_t mode = CAM_ENGINE_AEE_MODE_INVALID;
  float gain = 0;
  float integrationTime = 0;
  uint8_t strength = 0;
  uint16_t yUpGain = 0;
  uint16_t yDownGain = 0;
  uint16_t uvGain = 0;
  uint16_t edgeGain = 0;

  auto ret = CamEngineAeeStatus(hCamEngine, &isRunning, &mode, &gain,
                                &integrationTime, &strength, &yUpGain,
                                &yDownGain, &uvGain, &edgeGain);
  REPORT(ret);

  ee.config.isAuto = mode == CAM_ENGINE_AEE_MODE_AUTO;

  CamEngineEeConfig_t &eeConfig = ee.config.config;

  eeConfig.strength = strength;
  eeConfig.yUpGain = yUpGain;
  eeConfig.yDownGain = yDownGain;
  eeConfig.uvGain = uvGain;
  eeConfig.edgeGain = edgeGain;
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
#endif

  config = ee.config;

  return RET_SUCCESS;
}

int32_t Engine::eeConfigSet(clb::Ee::Config config) {
  clb::Ee &ee = pCalibration->module<clb::Ee>();

#ifdef ISP_EE
  CamEngineEeConfig_t &eeConfig = config.config;

  uint8_t strength = eeConfig.strength;
  uint16_t yUpGain = eeConfig.yUpGain;
  uint16_t yDownGain = eeConfig.yDownGain;
  uint16_t uvGain = eeConfig.uvGain;
  uint16_t edgeGain = eeConfig.edgeGain;

  auto ret = CamEngineAeeConfigure(hCamEngine, strength, yUpGain, yDownGain,
                                   uvGain, edgeGain);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
#endif

  if (!pCalibration->isReadOnly) {
    ee.config = config;
  }

  return RET_SUCCESS;
}

int32_t  Engine::eeEnableGet(bool &isEnable) {
  clb::Ee &ee = pCalibration->module<clb::Ee>();

#ifdef ISP_EE
  bool_t isRunning = BOOL_FALSE;
  CamEngineAeeMode_t mode = CAM_ENGINE_AEE_MODE_INVALID;
  float gain = 0;
  float integrationTime = 0;
  uint8_t strength = 0;
  uint16_t yUpGain = 0;
  uint16_t yDownGain = 0;
  uint16_t uvGain = 0;
  uint16_t edgeGain = 0;

  auto ret = CamEngineAeeStatus(hCamEngine, &isRunning, &mode, &gain,
                                &integrationTime, &strength, &yUpGain,
                                &yDownGain, &uvGain, &edgeGain);
  REPORT(ret);

  ee.isEnable = isRunning == BOOL_TRUE;
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
#endif

  isEnable = ee.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::eeEnableSet(bool isEnable) {
  clb::Ee &ee = pCalibration->module<clb::Ee>();

#ifdef ISP_EE
  if (isEnable) {
    CamEngineAeeMode_t mode = ee.config.isAuto ? CAM_ENGINE_AEE_MODE_AUTO
                                               : CAM_ENGINE_AEE_MODE_MANUAL;

    auto ret = CamEngineAeeStart(hCamEngine, mode);
    REPORT(ret);

    ret = eeConfigSet(ee.config);
    REPORT(ret);
  } else {
    auto ret = CamEngineAeeStop(hCamEngine);
    REPORT(ret);
  }
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
#endif

  if (!pCalibration->isReadOnly) {
    ee.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t  Engine::eeReset() {
#ifdef ISP_EE
  clb::Ee &ee = pCalibration->module<clb::Ee>();

  ee.config.reset();

  auto ret = eeConfigSet(ee.config);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
#endif

  return RET_SUCCESS;
}

int32_t Engine::eeStatusGet(clb::Ee::Status &status) {
#ifdef ISP_EE
  clb::Ee &ee = pCalibration->module<clb::Ee>();

  bool_t isRunning = BOOL_FALSE;
  CamEngineAeeMode_t mode = CAM_ENGINE_AEE_MODE_INVALID;
  float gain = 0;
  float integrationTime = 0;
  uint8_t strength = 0;
  uint16_t yUpGain = 0;
  uint16_t yDownGain = 0;
  uint16_t uvGain = 0;
  uint16_t edgeGain = 0;

  auto ret = CamEngineAeeStatus(hCamEngine, &isRunning, &mode, &gain,
                                &integrationTime, &strength, &yUpGain,
                                &yDownGain, &uvGain, &edgeGain);
  REPORT(ret);

  status.gain = gain;
  status.integrationTime = integrationTime;

  (void)ee;
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
#endif

  return RET_SUCCESS;
}

int32_t Engine::eeTableGet(Json::Value &jTable) {
  clb::Ee &ee = pCalibration->module<clb::Ee>();

#ifdef ISP_EE
  jTable = ee.table.jTable;
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
  (void)jTable;
  (void)ee;
#endif

  return RET_SUCCESS;
}

int32_t Engine::eeTableSet(Json::Value jTable) {
  clb::Ee &ee = pCalibration->module<clb::Ee>();

#ifdef ISP_EE
  ee.table.jTable = jTable;

  Json::Value &jRows = jTable[KEY_ROWS];

  auto rowCount = jRows.size();

  auto *pNodes = static_cast<CamEngineAeeParamNode_t *>(
      calloc(rowCount, sizeof(CamEngineAeeParamNode_t)));

  auto rowCountMatchHdr = 0;

  clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

  for (uint8_t i = 0; i < rowCount; i++) {
    Json::Value &jRow = jRows[i];

    if ((hdr.isEnable && jRow[clb::Ee::Table::Column::Hdr].asInt()) ||
        (!hdr.isEnable && !jRow[clb::Ee::Table::Column::Hdr].asInt())) {
    } else {
      continue;
    }

    auto *pNode = &pNodes[rowCountMatchHdr++];

    pNode->gain = jRow[clb::Ee::Table::Column::Gain].asFloat();
    pNode->integrationTime = jRow[clb::Ee::Table::IntegrationTime].asFloat();
    pNode->edgeGain = jRow[clb::Ee::Table::Column::EdgeGain].asUInt();
    pNode->strength = jRow[clb::Ee::Table::Column::Strength].asUInt();
    pNode->uvGain = jRow[clb::Ee::Table::Column::UvGain].asUInt();
    pNode->yUpGain = jRow[clb::Ee::Table::Column::YGainUp].asUInt();
    pNode->yDownGain = jRow[clb::Ee::Table::Column::YGainDown].asUInt();
  }

  if (rowCountMatchHdr) {
    auto ret = CamEngineAeeSetAutoTable(hCamEngine, pNodes, rowCountMatchHdr);
    free(pNodes);

    REPORT(ret);
  } else {
    free(pNodes);
  }
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/EE not support");
  (void)jTable;
  (void)ee;
#endif

  return RET_SUCCESS;
}

int32_t Engine::filterConfigGet(clb::Filter::Config &config) {
  clb::Filter &filter = pCalibration->module<clb::Filter>();

  bool_t isRunning = BOOL_FALSE;
  CamEngineAfltMode_t mode = CAM_ENGINE_AFLT_MODE_INVALID;
  float gain = 0;
  float integrationTime = 0;
  uint8_t denoise = 0;
  uint8_t sharpen = 0;
  uint8_t chrV = 0;
  uint8_t chrH = 0;

  auto ret = CamEngineAfltStatus(hCamEngine, &isRunning, &mode, &gain,
                                 &integrationTime, &denoise, &sharpen, &chrV, &chrH);
  REPORT(ret);

  filter.config.isAuto = mode == CAM_ENGINE_AFLT_MODE_AUTO;
  filter.config.denoise = denoise;
  filter.config.sharpen = sharpen;
  filter.config.chrV = chrV;
  filter.config.chrH = chrH;

  config = filter.config;

  return RET_SUCCESS;
}

int32_t Engine::filterConfigSet(clb::Filter::Config config) {
  clb::Filter &filter = pCalibration->module<clb::Filter>();
  uint8_t denoise = static_cast<uint8_t>(config.denoise);
  uint8_t sharpen = static_cast<uint8_t>(config.sharpen);
  uint8_t chrV = static_cast<uint8_t>(config.chrV);
  uint8_t chrH = static_cast<uint8_t>(config.chrH);
#ifdef ISP_FILTER
  auto ret = CamEngineAfltConfigure(hCamEngine, denoise, sharpen, chrV, chrH);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/FILTER not support");
#endif

  if (!pCalibration->isReadOnly) {
    filter.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::filterEnableGet(bool &isEnable) {
  clb::Filter &filter = pCalibration->module<clb::Filter>();

  bool_t isRunning = BOOL_FALSE;
  CamEngineAfltMode_t mode = CAM_ENGINE_AFLT_MODE_INVALID;
#ifdef ISP_FILTER
  float gain = 0;
  float integrationTime = 0;
  uint8_t denoise = 0;
  uint8_t sharpen = 0;
  uint8_t chrV = 0;
  uint8_t chrH = 0;

  auto ret = CamEngineAfltStatus(hCamEngine, &isRunning, &mode, &gain,
                                 &integrationTime, &denoise, &sharpen, &chrV, &chrH);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/FILTER not support");
#endif

  filter.isEnable = isRunning == BOOL_TRUE;

  isEnable = filter.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::filterEnableSet(bool isEnable) {
  clb::Filter &filter = pCalibration->module<clb::Filter>();

#ifdef ISP_FILTER
  if (isEnable) {
    CamEngineAfltMode_t mode = filter.config.isAuto
                                   ? CAM_ENGINE_AFLT_MODE_AUTO
                                   : CAM_ENGINE_AFLT_MODE_MANUAL;

    auto ret = CamEngineAfltStart(hCamEngine, mode);
    REPORT(ret);
  } else {
    auto ret = CamEngineAfltStop(hCamEngine);
    REPORT(ret);
  }
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/FILTER not support");
#endif
  if (!pCalibration->isReadOnly) {
    filter.isEnable = isEnable;
  }
  return RET_SUCCESS;
}

int32_t Engine::filterStatusGet(clb::Filter::Status &status) {

  bool_t isRunning = BOOL_FALSE;
  CamEngineAfltMode_t mode = CAM_ENGINE_AFLT_MODE_INVALID;
  float gain = 0;
  float integrationTime = 0;
  uint8_t denoise = 0;
  uint8_t sharpen = 0;
  uint8_t chrV = 0;
  uint8_t chrH = 0;

#ifdef ISP_FILTER
  auto ret = CamEngineAfltStatus(hCamEngine, &isRunning, &mode, &gain,
                                 &integrationTime, &denoise, &sharpen, &chrV, &chrH);
  REPORT(ret);
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/FILTER not support");
#endif

  status.gain = gain;
  status.integrationTime = integrationTime;

  return RET_SUCCESS;
}

int32_t Engine::filterTableGet(Json::Value &jTable) {
  clb::Filter &filter = pCalibration->module<clb::Filter>();

  jTable = filter.table.jTable;

  return RET_SUCCESS;
}

int32_t Engine::filterTableSet(Json::Value jTable) {
  clb::Filter &filter = pCalibration->module<clb::Filter>();

  filter.table.jTable = jTable;

  Json::Value &jRows = jTable[KEY_ROWS];

  auto rowCount = jRows.size();

  auto *pNodes = static_cast<CamEngineAfltParamNode_t *>(
      calloc(rowCount, sizeof(CamEngineAfltParamNode_t)));

  auto rowCountMatchHdr = 0;

  clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

  for (uint8_t i = 0; i < rowCount; i++) {
    Json::Value &jRow = jRows[i];

    if ((hdr.isEnable && jRow[clb::Filter::Table::Column::Hdr].asInt()) ||
        (!hdr.isEnable && !jRow[clb::Filter::Table::Column::Hdr].asInt())) {
    } else {
      continue;
    }

    auto *pNode = &pNodes[rowCountMatchHdr++];

    pNode->gain = jRow[clb::Filter::Table::Column::Gain].asFloat();
    pNode->integrationTime =
        jRow[clb::Filter::Table::IntegrationTime].asFloat();
    pNode->denoiseLevel = jRow[clb::Filter::Table::Column::Denoising].asUInt();
    pNode->sharpenLevel = jRow[clb::Filter::Table::Column::Sharpening].asUInt();
  }

  if (rowCountMatchHdr) {
    auto ret = CamEngineAfltSetAutoTable(hCamEngine, pNodes, rowCountMatchHdr);
    free(pNodes);

    REPORT(ret);
  } else {
    free(pNodes);
  }

  return RET_SUCCESS;
}

int32_t Engine::gcConfigGet(clb::Gc::Config &config) {
  clb::Gc &gc = pCalibration->module<clb::Gc>();

  config = gc.config;

  return RET_SUCCESS;
}

int32_t Engine::gcConfigSet(clb::Gc::Config config) {

  clb::Gc &gc = pCalibration->module<clb::Gc>();

  auto ret = CamEngineGammaSetCurve(hCamEngine, config.curve);
  REPORT(ret);
  if (!pCalibration->isReadOnly) {
    gc.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::gcEnableGet(bool &isEnable) {
  clb::Gc &gc = pCalibration->module<clb::Gc>();

  bool_t isRunning = BOOL_FALSE;

  auto ret = CamEngineGammaStatus(hCamEngine, &isRunning);
  REPORT(ret);

  gc.isEnable = isRunning == BOOL_TRUE;

  isEnable = gc.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::gcEnableSet(bool isEnable) {
  clb::Gc &gc = pCalibration->module<clb::Gc>();
#ifdef ISP_RGBGC
	CamEngineGammaOutCurve_t curve;

	unsigned int  gammaPx[CAMERIC_ISP_RGBGAMMA_PX_NUM] = {6,6, 6, 6, 6 ,6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6};
	unsigned  int dataY[64]  = {0x000,0x09a,0x0d3,0x0fe,0x122,0x141,0x15c,0x176,0x18d,0x1a3,0x1b8,0x1cb,0x1de,0x1ef,0x200,0x211,0x220,0x230,0x23e,0x24d,0x25a,0x268,0x275,0x282,0x28f,0x29b,0x2a7,0x2b3,0x2be,0x2c9,0x2d5,0x2df,0x2ea,0x2f5,0x2ff,0x309,0x313,0x31d,0x327,0x330,0x33a,0x343,0x34c,0x355,0x35e,0x367,0x370,0x379,0x381,0x38a,0x392,0x39a,0x3a2,0x3ab,0x3b3,0x3bb,0x3c2,0x3ca,0x3d2,0x3d9,0x3e1,0x3e9,0x3f0,0x3f7};
	unsigned int  dataX[63] = { 0x040,0x080,0x0c0,0x100,0x140,0x180,0x1c0,0x200,0x240,0x280,0x2c0,0x300,0x340,0x380,0x3c0,0x400,0x440,0x480,0x4c0,0x500,0x540,0x580,0x5c0,0x600,0x640,0x680,0x6c0,0x700,0x740,0x780,0x7c0,0x800,0x840,0x880,0x8c0,0x900,0x940,0x980,0x9c0,0xa00,0xa40,0xa80,0xac0,0xb00,0xb40,0xb80,0xbc0,0xc00,0xc40,0xc80,0xcc0,0xd00,0xd40,0xd80,0xdc0,0xe00,0xe40,0xe80,0xec0,0xf00,0xf40,0xf80,0xfc0};
	memcpy(curve.gammaRPx, gammaPx, sizeof(gammaPx));
//	memcpy(config.curve.gammaGPx, gammaPx, sizeof(gammaPx));
//	memcpy(config.curve.gammaBPx, gammaPx, sizeof(gammaPx));

	memcpy(curve.gammaRDataX, dataX, sizeof(dataX));
	memcpy(curve.gammaRDataY, dataY, sizeof(dataY));
  TRACE(ITF_INF, "%s: %d %d %d\n", __func__, dataY[0], dataY[1], dataY[2]);
//	memcpy(config.curve.gammaGDataX, dataX, sizeof(dataX));
//	memcpy(config.curve.gammaGDataY, dataY, sizeof(dataY));

//	memcpy(config.curve.gammaBDataX, dataX, sizeof(dataX));
//	memcpy(config.curve.gammaBDataY, dataY, sizeof(dataY));

	auto ret = CamEngineGammaSetCurve(hCamEngine, curve);
	REPORT(ret);
	
#endif
  if (isEnable) {
    auto ret = CamEngineGammaEnable(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineGammaDisable(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    gc.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::hdrConfigGet(clb::Hdr::Config &config) {
  clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

  if (camEngineConfig.type == CAM_ENGINE_CONFIG_SENSOR) {
    IsiSensorMode_t sensor_mode;
    IsiGetSensorModeIss(camEngineConfig.data.sensor.hSensor,&sensor_mode);
    if (sensor_mode.hdr_mode != SENSOR_MODE_LINEAR) {
      bool_t isRunning = BOOL_FALSE;
      uint8_t extBit = 0;
      float hdrRatio = 0;

      int32_t ret = CamEngineAhdrStatus(hCamEngine, &isRunning, &extBit, &hdrRatio);
      REPORT(ret);

      hdr.config.extensionBit = extBit;
      hdr.config.exposureRatio = hdrRatio;
    }
  } else {
    throw exc::LogicError(RET_NOTSUPP, "Engine/HDR not support");
  }

  config = hdr.config;

  return RET_SUCCESS;
}

int32_t Engine::hdrConfigSet(clb::Hdr::Config config) {
  clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

  if (camEngineConfig.type == CAM_ENGINE_CONFIG_SENSOR) {
    IsiSensorMode_t sensor_mode;
    IsiGetSensorModeIss(camEngineConfig.data.sensor.hSensor,&sensor_mode);
    if (sensor_mode.hdr_mode != SENSOR_MODE_LINEAR) {
      uint8_t extBit = config.extensionBit;
      float hdrRatio = config.exposureRatio;
      int32_t ret = CamEngineAhdrConfigure(hCamEngine, extBit, hdrRatio);
      REPORT(ret);
    }
  } else {
    throw exc::LogicError(RET_NOTSUPP, "Engine/HDR not support");
  }

  if (!pCalibration->isReadOnly) {
    hdr.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::hdrEnableGet(bool &isEnable) {
  clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

  if (camEngineConfig.type == CAM_ENGINE_CONFIG_SENSOR) {
    IsiSensorMode_t sensor_mode;
    IsiGetSensorModeIss(camEngineConfig.data.sensor.hSensor,&sensor_mode);
    if (sensor_mode.hdr_mode == SENSOR_MODE_HDR_STITCH) {
      bool_t isRunning = BOOL_FALSE;
      uint8_t extBit = 0;
      float hdrRatio = 0;

      int32_t ret = CamEngineAhdrStatus(hCamEngine, &isRunning, &extBit, &hdrRatio);
      REPORT(ret);

      hdr.isEnable = isRunning == BOOL_TRUE;
    }
  } else {
      throw exc::LogicError(RET_NOTSUPP, "Engine/HDR not support");
  }

  isEnable = hdr.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::hdrEnableSet(bool isEnable) {
  clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

  if (camEngineConfig.type == CAM_ENGINE_CONFIG_SENSOR) {
    IsiSensorMode_t sensor_mode;
    IsiGetSensorModeIss(camEngineConfig.data.sensor.hSensor,&sensor_mode);
    if (sensor_mode.hdr_mode == SENSOR_MODE_HDR_STITCH) {
      if (isEnable) {
        int32_t ret = CamEngineAhdrStart(hCamEngine);
        REPORT(ret);
      } else {
        int32_t ret = CamEngineAhdrStop(hCamEngine);
        REPORT(ret);
      }
    }
  } else {
      throw exc::LogicError(RET_NOTSUPP, "Engine/HDR not support");
  }

  if (!pCalibration->isReadOnly) {
    hdr.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::hdrReset() {
  if (camEngineConfig.type == CAM_ENGINE_CONFIG_SENSOR) {
    throw exc::LogicError(RET_NOTAVAILABLE, "Engine/HDR not available");
  } else {
    throw exc::LogicError(RET_NOTSUPP, "Engine/HDR not support");
  }

  return RET_SUCCESS;
}

int32_t Engine::ieConfigGet(clb::Ie::Config &config) {
  clb::Ie &ie = pCalibration->module<clb::Ie>();

#if defined ISP_IE
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/IE not support");
#endif

  config = ie.config;

  return RET_SUCCESS;
}

int32_t Engine::ieConfigSet(clb::Ie::Config config) {
  clb::Ie &ie = pCalibration->module<clb::Ie>();

#if defined ISP_IE
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/IE not support");
#endif

  if (!pCalibration->isReadOnly) {
    ie.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::ieEnableGet(bool &isEnable) {
  clb::Ie &ie = pCalibration->module<clb::Ie>();

  isEnable = ie.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::ieEnableSet(bool isEnable) {
  clb::Ie &ie = pCalibration->module<clb::Ie>();

#if defined ISP_IE
  if (isEnable) {
    auto ret = CamEngineEnableImageEffect(hCamEngine, &ie.config.config);
    REPORT(ret);
  } else {
    auto ret = CamEngineDisableImageEffect(hCamEngine);
    REPORT(ret);
  }
#else
  throw exc::LogicError(RET_NOTSUPP, "Engine/IE not support");
#endif

  if (!pCalibration->isReadOnly) {
    ie.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::jpeConfigGet(clb::Jpe::Config &config) {
  clb::Jpe &jpe = pCalibration->module<clb::Jpe>();

  config = jpe.config;

  return RET_SUCCESS;
}

int32_t Engine::jpeConfigSet(clb::Jpe::Config config) {
  if (!pCalibration->isReadOnly) {
    clb::Jpe &jpe = pCalibration->module<clb::Jpe>();

    jpe.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::jpeEnableGet(bool &isEnable) {
  clb::Jpe &jpe = pCalibration->module<clb::Jpe>();

  isEnable = jpe.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::jpeEnableSet(bool isEnable) {
  clb::Jpe &jpe = pCalibration->module<clb::Jpe>();

  if (isEnable) {
    CamerIcJpeConfig_t JpeConfig = {CAMERIC_JPE_MODE_LARGE_CONTINUOUS,
                                    CAMERIC_JPE_COMPRESSION_LEVEL_HIGH,
                                    CAMERIC_JPE_LUMINANCE_SCALE_DISABLE,
                                    CAMERIC_JPE_CHROMINANCE_SCALE_DISABLE,
                                    jpe.config.width,
                                    jpe.config.height};

    auto ret = CamEngineEnableJpe(hCamEngine, &JpeConfig);
    REPORT(ret);
  } else {
    auto ret = CamEngineDisableJpe(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    jpe.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::lscConfigGet(clb::Lsc::Config &config) {
  clb::Lsc &lsc = pCalibration->module<clb::Lsc>();

  throw exc::LogicError(RET_NOTAVAILABLE, "Engine/LSC not available");

  config = lsc.config;

  return RET_SUCCESS;
}

int32_t Engine::lscConfigSet(clb::Lsc::Config config) {
  clb::Lsc &lsc = pCalibration->module<clb::Lsc>();

  throw exc::LogicError(RET_NOTAVAILABLE, "Engine/LSC not available");

  if (!pCalibration->isReadOnly) {
    lsc.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::lscEnableGet(bool &isEnable) {
  clb::Lsc &lsc = pCalibration->module<clb::Lsc>();

  isEnable = lsc.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::lscEnableSet(bool isEnable) {
  clb::Lsc &lsc = pCalibration->module<clb::Lsc>();

  if (isEnable) {
    auto ret = CamEngineLscEnable(hCamEngine);
    REPORT(ret);
  } else {
    auto ret = CamEngineLscDisable(hCamEngine);
    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    lsc.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::lscStatusGet(clb::Lsc::Status &status) {
  bool_t isRunning = BOOL_FALSE;

  auto ret = CamEngineLscStatus(hCamEngine, &isRunning, &status.config);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Engine::pathConfigGet(clb::Paths::Config &config) {
  clb::Paths &paths = pCalibration->module<clb::Paths>();

  config = paths.config;

  return RET_SUCCESS;
}

int32_t Engine::pathConfigSet(clb::Paths::Config config) {
  clb::Paths &paths = pCalibration->module<clb::Paths>();

  if (state >= Idle) {

#if 1
    auto ret =
        CamEngineSetPathConfig(hCamEngine, &config.config[CAM_ENGINE_PATH_MAIN],
                               &config.config[CAM_ENGINE_PATH_SELF],
                               &config.config[CAM_ENGINE_PATH_SELF2_BP],
                               &config.config[CAM_ENGINE_PATH_RDI],
                               &config.config[CAM_ENGINE_PATH_META]);
#else
	auto ret = CamEngineSetPathConfig(hCamEngine, config.config);
#endif

    REPORT(ret);
  }

  if (!pCalibration->isReadOnly) {
    paths.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::pictureOrientationSet(CamerIcMiOrientation_t orientation) {
  switch (orientation) {
  case CAMERIC_MI_ORIENTATION_ORIGINAL:
    return CamEngineOriginal(hCamEngine);

  case CAMERIC_MI_ORIENTATION_HORIZONTAL_FLIP:
    return CamEngineHorizontalFlip(hCamEngine);

  case CAMERIC_MI_ORIENTATION_VERTICAL_FLIP:
    return CamEngineVerticalFlip(hCamEngine);

  case CAMERIC_MI_ORIENTATION_ROTATE90:
    return CamEngineRotateLeft(hCamEngine);

  case CAMERIC_MI_ORIENTATION_ROTATE270:
    return CamEngineRotateRight(hCamEngine);

  default:
    DCT_ASSERT(!"UNKOWN CamerIcMiOrientation_t");
  }

  return RET_SUCCESS;
}

int32_t Engine::reset() {
  clb::Paths &paths = pCalibration->module<clb::Paths>();

  paths.reset();

  MEMCPY(camEngineConfig.pathConfig, paths.config.config,
         sizeof(CamEnginePathConfig_t) * CAMERIC_MI_PATH_MAX);

#if 1
  auto ret = CamEngineSetPathConfig(
      hCamEngine, &camEngineConfig.pathConfig[CAM_ENGINE_PATH_MAIN],
      &camEngineConfig.pathConfig[CAM_ENGINE_PATH_SELF],
      &camEngineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP],
      &camEngineConfig.pathConfig[CAM_ENGINE_PATH_RDI],
      &camEngineConfig.pathConfig[CAM_ENGINE_PATH_META]);
#else
  auto ret = CamEngineSetPathConfig(
      hCamEngine, camEngineConfig.pathConfig);
#endif
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Engine::resolutionSet(CamEngineWindow_t acqWindow,
                              CamEngineWindow_t outWindow,
                              CamEngineWindow_t isWindow,
                              uint32_t numFramesToSkip) {
  auto ret = CamEngineSetAcqResolution(hCamEngine, acqWindow, outWindow,
                                       isWindow, numFramesToSkip);
  REPORT(ret);

  return RET_SUCCESS;
}

int32_t Engine::searchAndLock(CamEngineLockType_t locks) {
  auto ret = CamEngineSearchAndLock(hCamEngine, locks);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventAcquireLock) == OSLAYER_OK);

  return RET_SUCCESS;
}

int32_t Engine::simpConfigGet(clb::Simp::Config &config) {
  clb::Simp &simp = pCalibration->module<clb::Simp>();

  config = simp.config;

  return RET_SUCCESS;
}

int32_t Engine::simpConfigSet(clb::Simp::Config config) {
  clb::Simp &simp = pCalibration->module<clb::Simp>();

  if (!pCalibration->isReadOnly) {
    simp.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::simpEnableGet(bool &isEnable) {
  clb::Simp &simp = pCalibration->module<clb::Simp>();

  isEnable = simp.isEnable;

  return RET_SUCCESS;
}

int32_t Engine::simpEnableSet(bool isEnable) {
  clb::Simp &simp = pCalibration->module<clb::Simp>();

  if (isEnable) {
    if (pSimpImage) {
      delete pSimpImage;
      pSimpImage = nullptr;
    }

    pSimpImage = new Image();

    pSimpImage->load(simp.config.fileName);

    simp.config.config.pPicBuffer = &pSimpImage->config.picBufMetaData;

    auto ret = CamEngineEnableSimp(hCamEngine, &simp.config.config);
    REPORT(ret);
  } else {
    auto ret = CamEngineDisableSimp(hCamEngine);
    REPORT(ret);

    if (pSimpImage) {
      delete pSimpImage;
      pSimpImage = nullptr;
    }

    simp.config.config.pPicBuffer = nullptr;
  }

  if (!pCalibration->isReadOnly) {
    simp.isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::start() {
  if (state != Init) {
    REPORT(RET_WRONG_STATE);
  }

#ifndef REST_SIMULATE
  if(camEngineConfig.type == CAM_ENGINE_CONFIG_SENSOR)
  {
      IsiSensorMode_t sensor_mode;
      IsiGetSensorModeIss(camEngineConfig.data.sensor.hSensor, &sensor_mode);
      if(sensor_mode.hdr_mode == SENSOR_MODE_HDR_STITCH)
      {
          CamEngineInitHdrMode(hCamEngine, BOOL_TRUE);
      }
      else
      {
          CamEngineInitHdrMode(hCamEngine, BOOL_FALSE);
      }
  }

  auto ret = CamEngineStart(hCamEngine, &camEngineConfig);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventStart) == OSLAYER_OK);
#endif

  return RET_SUCCESS;
}

int32_t Engine::stop() {
  if (state != Idle) {
    REPORT(RET_WRONG_STATE);
  }

  auto ret = CamEngineStop(hCamEngine);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventStop) == OSLAYER_OK);

  return RET_SUCCESS;
}

int32_t Engine::streamingStart(uint32_t frames) {
  if (state != Idle) {
    REPORT(RET_WRONG_STATE);
  }

#ifndef REST_SIMULATE
  auto ret = CamEngineStartStreaming(hCamEngine, frames);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventStartStreaming) == OSLAYER_OK);
#endif

  return RET_SUCCESS;
}

int32_t Engine::streamingStop() {
  if (state != Running) {
    REPORT(RET_WRONG_STATE);
  }

  auto ret = CamEngineStopStreaming(hCamEngine);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventStopStreaming) == OSLAYER_OK);

  return RET_SUCCESS;
}

int32_t Engine::unlock(CamEngineLockType_t locks) {
  auto ret = CamEngineUnlock(hCamEngine, locks);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventReleaseLock) == OSLAYER_OK);

  return RET_SUCCESS;
}

int32_t Engine::wbConfigGet(clb::Wb::Config &config) {
  clb::Wb &wb = pCalibration->module<clb::Wb>();

  CamEngineCcMatrix_t matrix;

  auto ret = CamEngineWbGetCcMatrix(hCamEngine, &matrix);
  REPORT(ret);

  wb.config.ccMatrix = matrix;

  CamEngineCcOffset_t offset;

  ret = CamEngineWbGetCcOffset(hCamEngine, &offset);
  REPORT(ret);

  wb.config.ccOffset = offset;

  CamEngineWbGains_t gains;

  ret = CamEngineWbGetGains(hCamEngine, &gains);
  REPORT(ret);

  wb.config.wbGains = gains;

  config = wb.config;

  return RET_SUCCESS;
}

int32_t Engine::wbConfigSet(clb::Wb::Config config) {
  auto ret = CamEngineWbSetCcMatrix(hCamEngine, &config.ccMatrix);
  REPORT(ret);

  ret = CamEngineWbSetCcOffset(hCamEngine, &config.ccOffset);
  REPORT(ret);

  ret = CamEngineWbSetGains(hCamEngine, &config.wbGains);
  REPORT(ret);

  if (!pCalibration->isReadOnly) {
    clb::Wb &wb = pCalibration->module<clb::Wb>();

    wb.config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::wdrConfigGet(clb::Wdr::Config &config,
                             clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
#if defined ISP_WDR_V1 || defined ISP_GWDR
    config = wdr.holders[clb::Wdr::V1].config;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/GWDR not open");
#endif
  } else if (generation == clb::Wdr::V2) {
#if defined ISP_WDR_V2
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
#endif
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    bool_t isRunning = BOOL_FALSE;
    CamEngineAwdr3Mode_t mode = CAM_ENGINE_AWDR3_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    uint8_t strength = 0;
    uint8_t globalStrength = 0;
    uint8_t maxGain = 0;

    auto ret = CamEngineAwdr3Status(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &strength,
                                    &globalStrength, &maxGain);
    REPORT(ret);

    clb::Wdr::Config::V3 &wdr3Config = wdr.holders[generation].config.v3;

    wdr3Config.gainMax = maxGain;
    wdr3Config.strength = strength;
    wdr3Config.strengthGlobal = globalStrength;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
#endif
  } else {
    return RET_INVALID_PARM;
  }

  config = wdr.holders[generation].config;

  return RET_SUCCESS;
}

int32_t Engine::wdrConfigSet(clb::Wdr::Config config,
                             clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
#if defined ISP_WDR_V1 || defined ISP_GWDR
    auto ret = CamEngineWdrSetCurve(hCamEngine, config.v1.curve);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/GWDR not open");
#endif
  } else if (generation == clb::Wdr::V2) {
#if defined ISP_WDR_V2
    auto ret = CamEngineWdr2SetStrength(hCamEngine, config.v2.strength);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
#endif
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    auto ret =
        CamEngineAwdr3Configure(hCamEngine, config.v3.strength,
                                config.v3.strengthGlobal, config.v3.gainMax);
    REPORT(ret);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
#endif
  } else {
    REPORT(RET_INVALID_PARM);
  }

  if (!pCalibration->isReadOnly) {
    wdr.holders[generation].config = config;
  }

  return RET_SUCCESS;
}

int32_t Engine::wdrEnableGet(bool &isEnable, clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {

  } else if (generation == clb::Wdr::V2) {
#if defined ISP_WDR_V2

#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
#endif
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    bool_t isRunning = BOOL_FALSE;
    CamEngineAwdr3Mode_t mode = CAM_ENGINE_AWDR3_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    uint8_t strength = 0;
    uint8_t globalStrength = 0;
    uint8_t maxGain = 0;

    auto ret = CamEngineAwdr3Status(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &strength,
                                    &globalStrength, &maxGain);
    REPORT(ret);

    wdr.holders[generation].isEnable = isRunning == BOOL_TRUE;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
#endif
  } else {
    return RET_INVALID_PARM;
  }

  isEnable = wdr.holders[generation].isEnable;

  return RET_SUCCESS;
}

int32_t Engine::wdrEnableSet(bool isEnable, clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
#if defined ISP_WDR_V1 || defined ISP_GWDR
    if (isEnable) {
      auto ret = CamEngineEnableWdr(hCamEngine);
      REPORT(ret);
    } else {
      auto ret = CamEngineDisableWdr(hCamEngine);
      REPORT(ret);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/GWDR not open");
#endif
  } else if (generation == clb::Wdr::V2) {
#if defined ISP_WDR_V2
    if (isEnable) {
      auto ret = CamEngineEnableWdr2(hCamEngine);
      REPORT(ret);
    } else {
      auto ret = CamEngineDisableWdr2(hCamEngine);
      REPORT(ret);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
#endif
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    if (isEnable) {
      CamEngineAwdr3Mode_t mode = wdr.holders[generation].config.v3.isAuto
                                      ? CAM_ENGINE_AWDR3_MODE_AUTO
                                      : CAM_ENGINE_AWDR3_MODE_MANUAL;
      auto ret = CamEngineAwdr3Start(hCamEngine, mode);
      REPORT(ret);
    } else {
      auto ret = CamEngineAwdr3Stop(hCamEngine);
      REPORT(ret);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
#endif
  } else {
    return RET_INVALID_PARM;
  }

  if (!pCalibration->isReadOnly) {
    wdr.holders[generation].isEnable = isEnable;
  }

  return RET_SUCCESS;
}

int32_t Engine::wdrReset(clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
    throw exc::LogicError(RET_NOTAVAILABLE, "Engine/GWDR not available");
  } else if (generation == clb::Wdr::V2) {
    throw exc::LogicError(RET_NOTAVAILABLE, "Engine/WDR2 not available");
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    wdr.holders[generation].config.v3.reset();

    wdrConfigSet(wdr.holders[generation].config, generation);
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
#endif
  } else {
    return RET_INVALID_PARM;
  }

  clb::Wdr noUnusedValue = wdr;

  return RET_SUCCESS;
}

int32_t Engine::wdrStatusGet(clb::Wdr::Status &status,
                             clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
  } else if (generation == clb::Wdr::V2) {
#if defined ISP_WDR_V2
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
#endif
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    bool_t isRunning = BOOL_FALSE;
    CamEngineAwdr3Mode_t mode = CAM_ENGINE_AWDR3_MODE_INVALID;
    float gain = 0;
    float integrationTime = 0;
    uint8_t strength = 0;
    uint8_t globalStrength = 0;
    uint8_t maxGain = 0;

    auto ret = CamEngineAwdr3Status(hCamEngine, &isRunning, &mode, &gain,
                                    &integrationTime, &strength,
                                    &globalStrength, &maxGain);
    REPORT(ret);

    status.gain = gain;
    status.integrationTime = integrationTime;

    (void)wdr;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
#endif
  } else {
    return RET_INVALID_PARM;
  }

  return RET_SUCCESS;
}

int32_t Engine::wdrTableGet(Json::Value &jTable,
                            clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
    throw exc::LogicError(RET_NOTSUPP, "Engine/GWDR not support");
  } else if (generation == clb::Wdr::V2) {
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
  } else if (generation == clb::Wdr::V3) {
#if defined ISP_WDR_V3
    jTable = wdr.holders[clb::Wdr::V3].table.jTable;
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
    (void)jTable;
    (void)generation;
#endif
  }

  return RET_SUCCESS;
}

int32_t Engine::wdrTableSet(Json::Value jTable,
                            clb::Wdr::Generation generation) {
  clb::Wdr &wdr = pCalibration->module<clb::Wdr>();

  if (generation == clb::Wdr::V1) {
    throw exc::LogicError(RET_NOTSUPP, "Engine/GWDR not support");
  } else if (generation == clb::Wdr::V2) {
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR2 not support");
  } else if (generation == clb::Wdr::V3) {
#if  defined ISP_WDR_V3
    wdr.holders[clb::Wdr::V3].table.jTable = jTable;

    Json::Value &jRows = jTable[KEY_ROWS];

    auto rowCount = jRows.size();

    auto *pNodes = static_cast<CamEngineAwdr3ParamNode_t *>(
        calloc(rowCount, sizeof(CamEngineAwdr3ParamNode_t)));

    auto rowCountMatchHdr = 0;

    clb::Hdr &hdr = pCalibration->module<clb::Hdr>();

    for (uint8_t i = 0; i < rowCount; i++) {
      Json::Value &jRow = jRows[i];

      if ((hdr.isEnable && jRow[clb::Wdr::Table::ColumnV3::Hdr].asInt()) ||
          (!hdr.isEnable && !jRow[clb::Wdr::Table::ColumnV3::Hdr].asInt())) {
      } else {
        continue;
      }

      auto *pNode = &pNodes[rowCountMatchHdr++];

      pNode->gain = jRow[clb::Wdr::Table::ColumnV3::Gain].asFloat();
      pNode->integrationTime =
          jRow[clb::Wdr::Table::IntegrationTime].asFloat();
      pNode->strength = jRow[clb::Wdr::Table::ColumnV3::Strength].asUInt();
      pNode->maxGain = jRow[clb::Wdr::Table::ColumnV3::MaxGain].asUInt();
      pNode->globalStrength =
          jRow[clb::Wdr::Table::ColumnV3::GlobalCurve].asUInt();
    }

    if (rowCountMatchHdr) {
      auto ret =
          CamEngineAwdr3SetAutoTable(hCamEngine, pNodes, rowCountMatchHdr);
      free(pNodes);

      REPORT(ret);
    } else {
      free(pNodes);
    }
#else
    throw exc::LogicError(RET_NOTSUPP, "Engine/WDR3 not support");
    (void)jTable;
    (void)generation;
#endif
  }

  return RET_SUCCESS;
}
