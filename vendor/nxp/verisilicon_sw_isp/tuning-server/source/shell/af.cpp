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

#include "shell/af.hpp"

using namespace sh;

Af &Af::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case AvailableGet:
    return availableGet(jQuery, jResponse);

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

Af &Af::availableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isAvailable = true;

  jResponse[REST_RET] = pCamera->pEngine->afAvailableGet(isAvailable);

  jResponse[KEY_AVAILABLE] = isAvailable;

  return *this;
}

Af &Af::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Af::Config config;

  jResponse[REST_RET] = pCamera->pEngine->afConfigGet(config);

  jResponse[KEY_ALGORITHM] = config.searchAlgorithm;
  jResponse[KEY_ONESHOT] = config.isOneshot;
  jResponse[KEY_MODE] = config.mode;
  jResponse[KEY_LENGTH] = config.pos;

  jResponse[KEY_WIN_START_X] = config.win.startX;
  jResponse[KEY_WIN_START_Y] = config.win.startY;
  jResponse[KEY_WIN_START_WIDTH] = config.win.width;
  jResponse[KEY_WIN_START_HEIGHT] = config.win.height;

  return *this;
}

Af &Af::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Af::Config config;

  config.searchAlgorithm =
      static_cast<CamEngineAfSearchAlgorithm_t>(jQuery[KEY_ALGORITHM].asInt());
  config.isOneshot = jQuery[KEY_ONESHOT].asBool();

  config.mode = static_cast<CamEngineAfWorkMode_t>(jQuery[KEY_MODE].asInt());
  config.pos = jQuery[KEY_LENGTH].asUInt();

  config.win.startX = jQuery[KEY_WIN_START_X].asUInt();
  config.win.startY = jQuery[KEY_WIN_START_Y].asUInt();
  config.win.width = jQuery[KEY_WIN_START_WIDTH].asUInt();
  config.win.height = jQuery[KEY_WIN_START_HEIGHT].asUInt();

  jResponse[REST_RET] = pCamera->pEngine->afConfigSet(config);

  return *this;
}

Af &Af::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->afEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Af &Af::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->afEnableSet(isEnable);

  return *this;
}

