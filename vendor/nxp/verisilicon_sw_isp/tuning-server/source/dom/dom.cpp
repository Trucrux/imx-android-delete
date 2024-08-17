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

#include "dom/dom.hpp"
#include "camera/halholder.hpp"
#include <ebase/builtins.h>

Dom::Dom() {
  TRACE_IN;

  domCtrlConfig_t config;
  MEMSET(&config, 0, sizeof(config));

  config.MaxPendingCommands = 10;
  config.MaxBuffers = 1;
  config.domCbCompletion = cbCompletion;
  config.pUserContext = (void *)this;
  config.HalHandle = pHalHolder->hHal;
  config.ImgPresent = DOMCTRL_IMAGE_PRESENTATION_3D_VERTICAL;

  DCT_ASSERT(!domCtrlInit(&config));
  hCtrl = config.domCtrlHandle;

  state = Idle;

  TRACE_OUT;
}

Dom::~Dom() {
  TRACE_IN;

  stop();

  DCT_ASSERT(!domCtrlShutDown(hCtrl));
  hCtrl = NULL;

  TRACE_OUT;
}

void Dom::bufferCb(MediaBuffer_t *pBuffer) {
  TRACE_IN;

  domCtrlShowBuffer(hCtrl, pBuffer);

  TRACE_OUT;
}

void Dom::cbCompletion(domCtrlCmdId_t cmdId, int32_t result,
                       const void *pUserContext) {
  TRACE_IN;

  if (result != RET_SUCCESS) {
    return;
  }

  DCT_ASSERT(pUserContext);

  Dom *pDom = (Dom *)pUserContext;

  switch (cmdId) {
  case DOM_CTRL_CMD_START:
    DCT_ASSERT(!osEventSignal(&pDom->eventStarted));
    break;

  case DOM_CTRL_CMD_STOP:
    DCT_ASSERT(!osEventSignal(&pDom->eventStopped));
    break;

  default:
    break;
  }

  TRACE_OUT;
}

int32_t Dom::start(void *) {
  TRACE_IN;

  if (state == Running) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  DCT_ASSERT(domCtrlStart(hCtrl) == RET_PENDING);

  DCT_ASSERT(!osEventWait(&eventStarted));

  state = Running;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Dom::stop() {
  TRACE_IN;

  if (state == Idle) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  int32_t result = domCtrlStop(hCtrl);
  DCT_ASSERT(result == RET_PENDING);

  int32_t ret = osEventWait(&eventStopped);
  DCT_ASSERT(ret == OSLAYER_OK);

  state = Idle;

  TRACE_OUT;

  return RET_SUCCESS;
}

Dom *pDom = nullptr;
