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

class Simp : public Engine {
public:
  enum {
    Begin = Engine::Simp * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,

    End,
  };

  Simp() {
    idBegin = Begin;
    idEnd = End;
  }

  Simp &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Simp &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Simp &enableSet(Json::Value &jQuery, Json::Value &jResponse);
  Simp &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Simp &configSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
