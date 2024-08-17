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

class Ie : public Engine {
public:
  enum {
    Begin = Engine::Ie * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,

    End,
  };

  Ie() {
    idBegin = Begin;
    idEnd = End;
  }

  Ie &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Ie &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Ie &configSet(Json::Value &jQuery, Json::Value &jResponse);

  Ie &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Ie &enableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
