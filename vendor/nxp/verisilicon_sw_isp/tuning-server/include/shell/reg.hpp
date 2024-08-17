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

class Reg : public Engine {
public:
  enum {
    Begin = Engine::Reg * Engine::Step,

    Description,
    Get,
    Set,

    End,
  };

  Reg() {
    idBegin = Begin;
    idEnd = End;
  }

  Reg &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Reg &description(Json::Value &jQuery, Json::Value &jResponse);
  Reg &get(Json::Value &jQuery, Json::Value &jResponse);
  Reg &set(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
