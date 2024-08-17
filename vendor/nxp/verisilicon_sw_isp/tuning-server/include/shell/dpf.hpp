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

class Dpf : public Engine {
public:
  enum {
    Begin = Engine::Dpf * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,

    End,
  };

  Dpf() {
    idBegin = Begin;
    idEnd = End;
  };

  Dpf &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Dpf &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Dpf &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Dpf &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Dpf &enableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
