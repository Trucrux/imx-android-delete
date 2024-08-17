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

class Abstract : public Shell {
public:
  enum {
    Begin = Shell::Abstract * Shell::Step,

    DehazeEnableGet,
    DehazeEnableSet,

    End,
  };

  Abstract() {
    idBegin = Begin;
    idEnd = End;
  }

  Abstract &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Abstract &dehazeEnableGet(Json::Value &jQuery, Json::Value &jResponse);
  Abstract &dehazeEnableSet(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
