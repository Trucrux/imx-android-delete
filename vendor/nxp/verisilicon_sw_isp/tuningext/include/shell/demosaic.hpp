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

class Demosaic : public Engine {
public:
  enum {
    Begin = Engine::Demosaic * Engine::Step,

    ConfigGet,
    ConfigSet,
    EnableGet,
    EnableSet,

    End,
  };

  Demosaic() {
    idBegin = Begin;
    idEnd = End;
  }

  Demosaic &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Demosaic &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Demosaic &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Demosaic &enableGet(Json::Value &jQuery, Json::Value &jResponse);
  Demosaic &enableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
