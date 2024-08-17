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

#include "shell/lsc.hpp"
#include "calibration/lsc.hpp"

using namespace sh;

Lsc &Lsc::process(Json::Value &jQuery, Json::Value &jResponse) {
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

  case StatusGet:
    return statusGet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Lsc &Lsc::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Lsc::Config config;

  jResponse[REST_RET] = pCamera->pEngine->lscConfigGet(config);

  jResponse[KEY_ADAPTIVE] = config.isAdaptive;

  return *this;
}

Lsc &Lsc::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Lsc::Config config;

  config.isAdaptive = jQuery[KEY_ADAPTIVE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->lscConfigSet(config);

  return *this;
}

Lsc &Lsc::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->lscEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Lsc &Lsc::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->lscEnableSet(isEnable);

  return *this;
}

Lsc &Lsc::statusGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Lsc::Status status;

  jResponse[REST_RET] = pCamera->pEngine->lscStatusGet(status);

  binEncode(status.config, jResponse[KEY_CONFIG]);

  return *this;
}
