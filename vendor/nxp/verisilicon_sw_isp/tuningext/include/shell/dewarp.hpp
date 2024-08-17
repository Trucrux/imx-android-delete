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

#ifdef APPMODE_V4L2
#include "ioctl/v4l2-ioctl.hpp"
#endif

namespace sh {

class Dewarp : public Shell {
public:
  enum {
    Begin = Shell::Dewarp * Shell::Step,

    ConfigGet,
    HFlipSet,
    VFlipSet,
    DweTypeSet,
    MatrixSet,
    BypassSet,

    End,
  };

  Dewarp() {
    idBegin = Begin;
    idEnd = End;
  }

  Dewarp &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Dewarp &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Dewarp &hFlipSet(Json::Value &jQuery, Json::Value &jResponse);
  Dewarp &vFlipSet(Json::Value &jQuery, Json::Value &jResponse);
  Dewarp &dweTypeSet(Json::Value &jQuery, Json::Value &jResponse);
  Dewarp &matrixSet(Json::Value &jQuery, Json::Value &jResponse);
  Dewarp &bypassSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
