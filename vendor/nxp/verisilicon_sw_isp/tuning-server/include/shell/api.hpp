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

#pragma GCC diagnostic ignored "-Wpedantic"

#include <json/json.h>
#include <list>

class Shell;

namespace sh {

class ShellApi {
public:
  ShellApi();

  ShellApi &process(Json::Value &jQuery, Json::Value &jResponse);

  std::list<Shell *> list;
};

} // namespace sh
