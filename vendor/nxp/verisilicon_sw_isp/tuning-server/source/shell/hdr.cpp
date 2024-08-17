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

#include "shell/hdr.hpp"

using namespace sh;

Hdr &Hdr::process(Json::Value &jQuery, Json::Value &jResponse) {
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

  case Reset:
    return reset(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Hdr &Hdr::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Hdr::Config config;

  jResponse[REST_RET] = pCamera->pEngine->hdrConfigGet(config);

  jResponse[KEY_EXPOSURE_RATIO] = config.exposureRatio;
  jResponse[KEY_EXTENSION_BIT] = config.extensionBit;

  return *this;
}

Hdr &Hdr::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Hdr::Config config;

  config.exposureRatio =
      static_cast<uint8_t>(jQuery[KEY_EXPOSURE_RATIO].asUInt());
  config.extensionBit =
      static_cast<uint8_t>(jQuery[KEY_EXTENSION_BIT].asUInt());

  jResponse[REST_RET] = pCamera->pEngine->hdrConfigSet(config);

  return *this;
}

Hdr &Hdr::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->hdrEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Hdr &Hdr::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] =
      pCamera->pEngine->hdrEnableSet(jQuery[KEY_ENABLE].asBool());

  return *this;
}

Hdr &Hdr::reset(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = pCamera->pEngine->hdrReset();

  return *this;
}
