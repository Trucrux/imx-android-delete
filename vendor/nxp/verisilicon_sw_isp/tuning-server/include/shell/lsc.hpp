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

struct Lsc : public Engine {
  enum {
    Begin = Engine::Lsc * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,
    StatusGet,

    End,
  };

  Lsc() {
    idBegin = Begin;
    idEnd = End;
  }

  Lsc &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Lsc &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Lsc &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Lsc &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Lsc &enableSet(Json::Value &jQuery, Json::Value &jResponse);
  Lsc &statusGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
