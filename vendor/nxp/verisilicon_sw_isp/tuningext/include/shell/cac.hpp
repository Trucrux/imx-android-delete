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

class Cac : public Engine {
public:
  enum {
    Begin = Engine::Cac * Engine::Step,

    EnableGet,
    EnableSet,
    StatusGet,

    End,
  };

  Cac() {
    idBegin = Begin;
    idEnd = End;
  }

  Cac &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Cac &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Cac &enableSet(Json::Value &jQuery, Json::Value &jResponse);
  Cac &statusGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
