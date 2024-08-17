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

#include "shell/cnr.hpp"

using namespace sh;

Cnr &Cnr::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Cnr &Cnr::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Cnr::Config config;

  jResponse[REST_RET] = pCamera->pEngine->cnrConfigGet(config);

  jResponse[KEY_TC1] = config.tc1;
  jResponse[KEY_TC2] = config.tc2;

  return *this;
}

Cnr &Cnr::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Cnr::Config config;

  config.tc1 = jQuery[KEY_TC1].asUInt();
  config.tc2 = jQuery[KEY_TC2].asUInt();

  jResponse[REST_RET] = pCamera->pEngine->cnrConfigSet(config);

  return *this;
}

Cnr &Cnr::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->cnrEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Cnr &Cnr::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] =
      pCamera->pEngine->cnrEnableSet(jQuery[KEY_ENABLE].asBool());

  return *this;
}
