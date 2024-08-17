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

#include "exa/exa.hpp"
#include "camera/halholder.hpp"
#include <ebase/builtins.h>

Exa::Exa() {
  TRACE_IN;

  exaCtrlConfig_t config;
  MEMSET(&config, 0, sizeof(config));

  config.MaxPendingCommands = 10;
  config.MaxBuffers = 1;
  config.exaCbCompletion = cbCompletion;
  config.pUserContext = (void *)this;
  config.HalHandle = pHalHolder->hHal;

  int32_t result = exaCtrlInit(&config);
  DCT_ASSERT(result == RET_SUCCESS);

  hCtrl = config.exaCtrlHandle;

  state = Idle;

  TRACE_OUT;
}

Exa::~Exa() {
  TRACE_IN;

  stop();

  int32_t result = exaCtrlShutDown(hCtrl);
  DCT_ASSERT(result == RET_SUCCESS);

  hCtrl = NULL;

  TRACE_OUT;
}

void Exa::bufferCb(MediaBuffer_t *pBuffer) {
  TRACE_IN;

  exaCtrlShowBuffer(hCtrl, pBuffer);

  TRACE_OUT;
}

void Exa::cbCompletion(exaCtrlCmdID_t cmdId, int32_t result,
                              void *pUserContext) {
  TRACE_IN;

  if (result != RET_SUCCESS) {
    TRACE_OUT;

    return;
  }

  DCT_ASSERT(pUserContext);

  Exa *pCtrlItf = (Exa *)pUserContext;

  int32_t ret = 0;

  switch (cmdId) {
  case EXA_CTRL_CMD_START:
    ret = osEventSignal(&pCtrlItf->eventStarted);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case EXA_CTRL_CMD_STOP:
    ret = osEventSignal(&pCtrlItf->eventStopped);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  default:
    break;
  }

  TRACE_OUT;
}

int32_t Exa::start(void *pParam) {
  TRACE_IN;

  if (state == Running) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  DCT_ASSERT(pParam);

  StartParam *pStartParam = (StartParam *)pParam;

  int32_t result =
      exaCtrlStart(hCtrl, pStartParam->sampleCb, pStartParam->pSampleCbContext,
                   pStartParam->sampleSkip);
  DCT_ASSERT(result == RET_PENDING);

  int32_t ret = osEventWait(&eventStarted);
  DCT_ASSERT(ret == OSLAYER_OK);

  state = Running;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Exa::stop() {
  TRACE_IN;

  if (state == Idle) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  int32_t result = exaCtrlStop(hCtrl);
  DCT_ASSERT(result == RET_PENDING);

  int32_t ret = osEventWait(&eventStopped);
  DCT_ASSERT(ret == OSLAYER_OK);

  state = Idle;

  TRACE_OUT;

  return RET_SUCCESS;
}
