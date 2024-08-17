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

class Ec : public Engine {
public:
  enum {
    Begin = Engine::Ec * Engine::Step,

    ConfigGet,
    ConfigSet,
    StatusGet,

    End,
  };

  Ec() {
    idBegin = Begin;
    idEnd = End;
  }

  Ec &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Ec &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Ec &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Ec &statusGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
