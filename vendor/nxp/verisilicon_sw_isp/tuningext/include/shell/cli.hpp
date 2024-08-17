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

#include "shell.hpp"

namespace sh {

class Cli : public Shell {
public:
  enum {
    Begin = Shell::Cli * Shell::Step,

    ControlListGet,
    VersionGet,

    End,
  };

  Cli() {
    idBegin = Begin;
    idEnd = End;
  }

  Cli &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Cli &controlListGet(Json::Value &jQuery, Json::Value &jResponse);
  Cli &versionGet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
