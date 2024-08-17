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

#include "shell/simp.hpp"
#include "camera/readpgmraw.hpp"

using namespace sh;

Simp &Simp::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case ConfigGet:
    return configGet(jQuery, jResponse);

  case ConfigSet:
    return configSet(jQuery, jResponse);

  case EnableGet:
    return enableGet(jQuery, jResponse);

  case EnableSet:
    return enableSet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Simp &Simp::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->simpEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Simp &Simp::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->simpEnableSet(isEnable);

  return *this;
}

Simp &Simp::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Simp::Config config;

  jResponse[REST_RET] = pCamera->pEngine->simpConfigGet(config);

  jResponse[KEY_FILE_NAME] = config.fileName;

  binEncode(config.config, jResponse[KEY_CONFIG]);

  return *this;
}

Simp &Simp::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Simp::Config config;

  config.fileName = jQuery[KEY_FILE_NAME].asString();

  binDecode(jQuery[KEY_CONFIG], config.config);

  jResponse[REST_RET] = pCamera->pEngine->simpConfigSet(config);

  return *this;
}
