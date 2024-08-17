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

class Ae : public Engine {
public:
  enum {
    Begin = Engine::Ae * Engine::Step,

    ConfigGet,
    ConfigSet,
    EcmGet,
    EcmSet,
    EnableGet,
    EnableSet,
    Reset,
    StatusGet,

    End,
  };

  Ae() {
    idBegin = Begin;
    idEnd = End;
  }

  Ae &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Ae &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Ae &configSet(Json::Value &jQuery, Json::Value &jResponse);

  Ae &ecmGet(Json::Value &jQuery, Json::Value &jResponse);
  Ae &ecmSet(Json::Value &jQuery, Json::Value &jResponse);

  Ae &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Ae &enableSet(Json::Value &jQuery, Json::Value &jResponse);

  Ae &reset(Json::Value &jQuery, Json::Value &jResponse);

  Ae &statusGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
