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

#include "camera/image.hpp"
#include "camera/readpgmraw.hpp"
#include "common/exception.hpp"
#include "common/filesystem.hpp"
#include "common/interface.hpp"
#include "common/macros.hpp"

using namespace camera;

Image::Image() {
  TRACE_IN;

  state = Init;

  TRACE_OUT;
}

Image::~Image() {
  TRACE_IN;

  clean();

  TRACE_OUT;
}

Image &Image::checkValid() {
  if (state <= Init) {
    throw exc::LogicError(RET_WRONG_STATE, "Load image firstly");
  }

  return *this;
}

void Image::clean() {
  if (config.picBufMetaData.Data.raw.pData) {
    free(config.picBufMetaData.Data.raw.pData);

    config.picBufMetaData.Data.raw.pData = nullptr;
  }

  REFSET(config.picBufMetaData, 0);

  config.isLsb = false;

  state = Init;
}

void Image::load(std::string fileName) {
  if (!fs::isExists(fileName)) {
    throw exc::LogicError(RET_INVALID_PARM, "No such file" + fileName);
  }

  clean();

  if (!PGM_ReadRaw(fileName.c_str(), &config.picBufMetaData)) {
    throw exc::LogicError(RET_FAILURE, "Load file failed: " + fileName);
  }

  state = Idle;
}
