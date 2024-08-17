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

#include "vom/vom.hpp"
#include "camera/halholder.hpp"
#include <ebase/builtins.h>

Vom::Vom(bool is3d, Cea861VideoFormat_t format) {
  TRACE_IN;

  vomCtrlConfig_t config;
  MEMSET(&config, 0, sizeof(vomCtrlConfig_t));

  config.MaxPendingCommands = 10;
  config.MaxBuffers = 1;
  config.vomCbCompletion = cbCompletion;
  config.pUserContext = (void *)this;
  config.HalHandle = pHalHolder->hHal;
  config.Enable3D = is3d ? BOOL_TRUE : BOOL_FALSE;
  config.VideoFormat = format;
  config.VideoFormat3D = HDMI_3D_VIDEOFORMAT_FRAME_PACKING;

  int32_t ret = vomCtrlInit(&config);
  DCT_ASSERT(ret == RET_SUCCESS);

  hCtrl = config.VomCtrlHandle;

  state = Idle;

  TRACE_OUT;
}

Vom::~Vom() {
  TRACE_IN;

  stop();

  int32_t ret = vomCtrlShutDown(hCtrl);
  DCT_ASSERT(ret == RET_SUCCESS);

  TRACE_OUT;
}

void Vom::bufferCb(MediaBuffer_t *pBuffer) {
  TRACE_IN;

  vomCtrlShowBuffer(hCtrl, pBuffer);

  TRACE_OUT;
}

void Vom::cbCompletion(vomCtrlCmdId_t cmdId, int32_t ret,
                       const void *pUserContext) {
  TRACE_IN;

  if (ret != RET_SUCCESS) {
    TRACE_OUT;

    return;
  }

  DCT_ASSERT(pUserContext);

  Vom *pCtrlItf = (Vom *)pUserContext;

  switch (cmdId) {
  case VOM_CTRL_CMD_START:
    ret = osEventSignal(&pCtrlItf->eventStarted);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  case VOM_CTRL_CMD_STOP:
    ret = osEventSignal(&pCtrlItf->eventStopped);
    DCT_ASSERT(ret == OSLAYER_OK);
    break;

  default:
    break;
  }

  TRACE_OUT;
}

int32_t Vom::start(void *) {
  TRACE_IN;

  if (state == Running) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  DCT_ASSERT(hCtrl);

  int32_t ret = vomCtrlStart(hCtrl);
  DCT_ASSERT(ret == RET_PENDING);

  ret = osEventWait(&eventStarted);
  DCT_ASSERT(ret == OSLAYER_OK);

  state = Running;

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Vom::stop() {
  TRACE_IN;

  if (state == Idle) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

  DCT_ASSERT(hCtrl);

  int32_t ret = vomCtrlStop(hCtrl);
  DCT_ASSERT(ret == RET_PENDING);

  ret = osEventWait(&eventStopped);
  DCT_ASSERT(ret == OSLAYER_OK);

  state = Idle;

  TRACE_OUT;

  return RET_SUCCESS;
}
