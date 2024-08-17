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

class Awb : public Engine {
public:
  enum {
    Begin = Engine::Awb * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,
    IlluminationProfilesGet,
    Reset,
    StatusGet,

    End,
  };

  Awb() {
    idBegin = Begin;
    idEnd = End;
  };

  Awb &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Awb &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Awb &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Awb &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Awb &enableSet(Json::Value &jQuery, Json::Value &jResponse);
  Awb &illuminanceProfilesGet(Json::Value &jQuery, Json::Value &jResponse);
  Awb &reset(Json::Value &jQuery, Json::Value &jResponse);
  Awb &statusGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
