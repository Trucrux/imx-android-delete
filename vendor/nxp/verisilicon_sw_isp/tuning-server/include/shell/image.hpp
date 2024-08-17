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

#include "common/picture_buffer.h"
#include "shell.hpp"

namespace sh {

class Image : public Shell {
public:
  enum {
    Begin = Shell::Image * Shell::Step,

    ImageList,
    Load,

    End,
  };

  Image() {
    idBegin = Begin;
    idEnd = End;
  }

  Image &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Image &imageList(Json::Value &jQuery, Json::Value &jResponse);
  Image &load(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace rest
