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

#include "shell/shell.hpp"

CREATE_TRACER(SHELL_INFO, "[SHELL][INF]: ", INFO, 1);
CREATE_TRACER(SHELL_ERROR, "[SHELL][ERR]: ", ERROR, 1);

using namespace sh;

Shell &Shell::process(Json::Value &jQuery, Json::Value &) {
  int32_t id = jQuery[REST_ID].asInt();

  if (id < idBegin || id > idEnd) {
    throw exc::LogicError(RET_IRRELATIVE, "Can't handle command");
  }

  return *this;
}

Shell &Shell::binDecode(Json::Value &jValue, void **ppBuffer, uint32_t &size) {
  size = jValue[KEY_SIZE].asUInt();

  if (!size) {
    return *this;
  }

  DCT_ASSERT(!*ppBuffer);

  *ppBuffer = calloc(1, size);

  std::string decodedString = base64_decode(jValue[KEY_BIN].asString());

  std::copy(decodedString.begin(), decodedString.end(),
            static_cast<unsigned char *>(*ppBuffer));

  return *this;
}

Shell &Shell::binEncode(void *pBuffer, uint32_t size, Json::Value &jValue) {
  jValue[KEY_SIZE] = size;
  jValue[KEY_BIN] =
      base64_encode(static_cast<const unsigned char *>(pBuffer), size);

  return *this;
}

camera::Sensor &Shell::sensor() {
  if (!pCamera->sensors.size()) {
    throw exc::LogicError(RET_WRONG_STATE, "Sensor is never initialized");
  }

  return pCamera->sensor();
}

void Shell::versionGet(std::string &name, std::string &number,
                       std::string &date) {
  name = "Shell";
  number = VERSION_SHELL;
  date = BUILD_TIME;
}
