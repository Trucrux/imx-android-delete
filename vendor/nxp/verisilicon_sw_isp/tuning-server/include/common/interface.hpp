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

#include "common/tracer.hpp"
#include <bufferpool/media_buffer.h>
#include <common/return_codes.h>
#include <oslayer/oslayer.h>
#include <string>

struct Object {
  std::string stateDescription();

  enum State { Invalid, Init, Idle, Running };

  State state = Invalid;
};

struct Ctrl : Object {
  Ctrl();
  virtual ~Ctrl();

  virtual int32_t start(void *) = 0;
  virtual int32_t stop() = 0;

  osEvent eventStarted;
  osEvent eventStopped;
};

struct ItfBufferCb {
  virtual void bufferCb(MediaBuffer_t *) = 0;
};
