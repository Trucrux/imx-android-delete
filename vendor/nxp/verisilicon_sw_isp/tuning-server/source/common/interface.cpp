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

#include "common/interface.hpp"

#define STATE_DESCRIPTION(_VALUE_)                                             \
  do {                                                                         \
    if (state == _VALUE_)                                                      \
      return #_VALUE_;                                                         \
  } while (0)

std::string Object::stateDescription() {
  STATE_DESCRIPTION(Invalid);
  STATE_DESCRIPTION(Idle);
  STATE_DESCRIPTION(Running);
  return "UNKNOWN STATE";
}

Ctrl::Ctrl() {
  TRACE_IN;

  int32_t ret = 0;

  ret = osEventInit(&eventStarted, 1, 0);
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osEventInit(&eventStopped, 1, 0);
  DCT_ASSERT(ret == OSLAYER_OK);

  TRACE_OUT;
}

Ctrl::~Ctrl() {
  TRACE_IN;

  int32_t ret = 0;

  ret = osEventDestroy(&eventStarted);
  DCT_ASSERT(ret == OSLAYER_OK);

  ret = osEventDestroy(&eventStopped);
  DCT_ASSERT(ret == OSLAYER_OK);

  TRACE_OUT;
}
