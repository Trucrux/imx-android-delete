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

#pragma once

#include "common/interface.hpp"
#if defined ISP_DOM
#include <dom_ctrl/dom_ctrl_api.h>
#else
typedef int32_t domCtrlCmdId_t;
typedef void *domCtrlHandle_t;
#endif

struct Dom : ItfBufferCb, Ctrl {
  Dom();
  ~Dom();

  void bufferCb(MediaBuffer_t *) override;

  static void cbCompletion(domCtrlCmdId_t, int32_t, const void *);

  int32_t start(void * = NULL) override;
  int32_t stop() override;

  domCtrlHandle_t hCtrl = NULL;
};

extern Dom *pDom;
