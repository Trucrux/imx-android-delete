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

#include "common/interface.hpp"
#include "common/macros.hpp"
#include <cam_engine/cam_engine_api.h>
#include <string>

namespace camera {

struct Image : Object {
  Image();
  ~Image();

  Image &checkValid();

  void clean();

  void load(std::string fileName);

  struct Config {
    Config() { REFSET(picBufMetaData, 0); }

    PicBufMetaData_t picBufMetaData;

    bool isLsb = false;
  } config;
};

} // namespace camera
