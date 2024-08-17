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

class Gc : public Engine {
public:
  enum {
    Begin = Engine::Gc * Engine::Step,

    CurveGet,
    CurveSet,
    EnableGet,
    EnableSet,

    End,
  };

  Gc() {
    idBegin = Begin;
    idEnd = End;
  }

  Gc &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Gc &curveGet(Json::Value &jQuery, Json::Value &jResponse);
  Gc &curveSet(Json::Value &jQuery, Json::Value &jResponse);
  Gc &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Gc &enableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
