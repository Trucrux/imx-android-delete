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

#include "som/som.hpp"
#include "camera/halholder.hpp"
#include "common/macros.hpp"

Som::Som() {
  TRACE_IN;

  somCtrlConfig_t somCtrlConfig;
  REFSET(somCtrlConfig, 0);

  somCtrlConfig.MaxPendingCommands = 10;
  somCtrlConfig.MaxBuffers = 1;
  somCtrlConfig.somCbCompletion = cbCompletion;
  somCtrlConfig.pUserContext = (void *)this;
  somCtrlConfig.HalHandle = pHalHolder->hHal;

  int32_t ret = somCtrlInit(&somCtrlConfig);
  DCT_ASSERT(ret == RET_SUCCESS);

  hCtrl = somCtrlConfig.somCtrlHandle;

  DCT_ASSERT(osEventInit(&eventDone, 1, 0) == OSLAYER_OK);

  state = Idle;

  TRACE_OUT;
}

Som::~Som() {
  TRACE_IN;

  stop();

  DCT_ASSERT(osEventDestroy(&eventDone) == OSLAYER_OK);

  int32_t ret = somCtrlShutDown(hCtrl);
  DCT_ASSERT(ret == RET_SUCCESS);

  hCtrl = NULL;

  TRACE_OUT;
}

void Som::bufferCb(MediaBuffer_t *pBuffer) {
  TRACE_IN;

  somCtrlStoreBuffer(hCtrl, pBuffer);

  TRACE_OUT;
}

void Som::cbCompletion(somCtrlCmdID_t cmdId, int32_t result,
                       somCtrlCmdParams_t *, somCtrlCompletionInfo_t *,
                       void *pUserContext) {
  TRACE_IN;

  DCT_ASSERT(pUserContext);

  Som *pSom = (Som *)pUserContext;

  int32_t ret = 0;

  switch (cmdId) {
  case SOM_CTRL_CMD_START:
    if (result == RET_PENDING) {
      TRACE(ITF_INF, "SOM_CTRL_CMD_START, result = RET_PENDING\n");
      DCT_ASSERT(osEventSignal(&pSom->eventStarted) == OSLAYER_OK);
    } else {
      TRACE(ITF_INF, "SOM_CTRL_CpSom, result = 0x%X \n", result);
      DCT_ASSERT(osEventSignal(&pSom->eventDone) == OSLAYER_OK);
    }
    break;

  case SOM_CTRL_CMD_STOP:
    ret = osEventSignal(&pSom->eventStopped);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  default:
    break;
  }

  TRACE_OUT;
}

int32_t Som::start(void *pParam) {
  TRACE_IN;

  if (state == Running) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  DCT_ASSERT(pParam);

  StartParam *pStartParam = (StartParam *)pParam;

  somCtrlCmdParams_t params;

  params.Start.szBaseFileName = pStartParam->pFilenameBase;
  params.Start.NumOfFrames = pStartParam->frameCount;
  params.Start.NumSkipFrames = pStartParam->skipCount;
  params.Start.AverageFrames = pStartParam->averageFrameCount;
  params.Start.ForceRGBOut = pStartParam->isForceRgbOut;
  params.Start.ExtendName = pStartParam->isExtendName;

  int32_t ret = somCtrlStart(hCtrl, &params);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventStarted) == OSLAYER_OK);

  state = Running;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Som::stop() {
  TRACE_IN;

  if (state == Idle) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  int32_t ret = somCtrlStop(hCtrl);
  REPORT(ret);

  DCT_ASSERT(osEventWait(&eventStopped) == OSLAYER_OK);

  state = Idle;

  TRACE_OUT;

  return RET_SUCCESS;
}
