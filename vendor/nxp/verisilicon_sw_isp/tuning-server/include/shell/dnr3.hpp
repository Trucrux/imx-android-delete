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

class Dnr3 : public Engine {
public:
  enum {
    Begin = Engine::Dnr3 * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,
    Reset,
    StatusGet,
    TableGet,
    TableSet,

    End,
  };

  Dnr3() {
    idBegin = Begin;
    idEnd = End;
  }

  Dnr3 &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Dnr3 &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Dnr3 &configSet(Json::Value &jQuery, Json::Value &jResponse);

  Dnr3 &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Dnr3 &enableSet(Json::Value &jQuery, Json::Value &jResponse);

  Dnr3 &reset(Json::Value &jQuery, Json::Value &jResponse);

  Dnr3 &statusGet(Json::Value &jQuery, Json::Value &jResponse);

  Dnr3 &tableGet(Json::Value &jQuery, Json::Value &jResponse);
  Dnr3 &tableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
