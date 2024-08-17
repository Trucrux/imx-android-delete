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

#include "camera/camera.hpp"
#include "common/bash-color.hpp"
#include "common/exception.hpp"
#include "common/macros.hpp"
#include "common/version.hpp"
#include <base64/base64.hpp>

#define TRACE_CMD                                                              \
  TRACE(SHELL_INFO, COLOR_STRING(COLOR_BLUE, " %s\n"), __PRETTY_FUNCTION__)

#define REST_FUNC "function"
#define REST_ID "id"
#define REST_RET "result"
#define REST_MSG "message"

#define RET_IRRELATIVE 0xFF

USE_TRACER(SHELL_INFO);
USE_TRACER(SHELL_ERROR);

namespace sh {

class Shell : Version {
public:
  enum {
    Begin,

    Abstract,
    Camera,
    Cli,
    Dewarp,
    Engine,
    FileSystem,
    Image,
    Sensor,

    End,

    Step = 10000,
  };

  Shell() {
    idBegin = Begin;
    idEnd = End;
  }

  template <typename T> Shell &binDecode(Json::Value &jValue, T &object) {
    uint32_t sizeT = sizeof(T);
    uint32_t size = jValue[KEY_SIZE].asUInt();

    if (sizeT != size) {
      std::string message("T size(%d) != size(%d)", sizeT, size);
      throw exc::LogicError(RET_INVALID_PARM, message.c_str());
    }

    std::string decodedString = base64_decode(jValue[KEY_BIN].asString());

    std::copy(decodedString.begin(), decodedString.end(), (char *)&object);

    return *this;
  }

  Shell &binDecode(Json::Value &jValue, void **ppBuffer, uint32_t &size);

  template <typename T> Shell &binEncode(T &object, Json::Value &jValue) {
    uint32_t size = sizeof(T);

    jValue[KEY_SIZE] = size;
    jValue[KEY_BIN] = base64_encode((u_char *)&object, size);

    return *this;
  }

  Shell &binEncode(void *pBuffer, uint32_t size, Json::Value &jValue);

  virtual Shell &process(Json::Value &jQuery, Json::Value &jResponse);

  camera::Sensor &sensor();

  void versionGet(std::string &name, std::string &number,
                  std::string &date) override;

public:
  int32_t idBegin;
  int32_t idEnd;
};

} // namespace sh
