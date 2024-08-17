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

class Avs : public Engine {
public:
  enum {
    Begin = Engine::Avs * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,
    StatusGet,

    End,
  };

  Avs() {
    idBegin = Begin;
    idEnd = End;
  }

  Avs &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Avs &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Avs &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Avs &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Avs &enableSet(Json::Value &jQuery, Json::Value &jResponse);
  Avs &statusGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
