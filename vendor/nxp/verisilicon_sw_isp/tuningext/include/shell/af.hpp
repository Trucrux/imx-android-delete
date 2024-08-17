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

#include "engine.hpp"

namespace sh {

class Af : public Engine {
public:
  enum {
    Begin = Engine::Af * Engine::Step,

    AvailableGet,
    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,

    End,
  };

  Af() {
    idBegin = Begin;
    idEnd = End;
  }

  Af &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Af &availableGet(Json::Value &jQuery, Json::Value &jResponse);
  Af &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Af &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Af &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Af &enableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
