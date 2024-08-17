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

#include "shell/avs.hpp"

using namespace sh;

Avs &Avs::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Avs &Avs::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Avs::Config config;

  jResponse[REST_RET] = pCamera->pEngine->avsConfigGet(config);

  jResponse[KEY_USE_PARAMS] = config.isUseParams;
  jResponse[KEY_ACCELERATION] = config.acceleration;
  jResponse[KEY_GAIN_BASE] = config.baseGain;
  jResponse[KEY_FALL_OFF] = config.fallOff;
  jResponse[KEY_NUM_ITP_POINTS] = config.numItpPoints;
  jResponse[KEY_THETA] = config.theta;

  jResponse[KEY_COUNT] = config.dampCount;
  binEncode(config.x, jResponse[KEY_X]);
  binEncode(config.y, jResponse[KEY_Y]);

  return *this;
}

Avs &Avs::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Avs::Config config;

  config.isUseParams = jQuery[KEY_USE_PARAMS].asBool();
  config.acceleration = jQuery[KEY_ACCELERATION].asFloat();
  config.baseGain = jQuery[KEY_GAIN_BASE].asFloat();
  config.fallOff = jQuery[KEY_FALL_OFF].asFloat();
  config.numItpPoints = jQuery[KEY_NUM_ITP_POINTS].asUInt();
  config.theta = jQuery[KEY_THETA].asFloat();

  jResponse[REST_RET] = pCamera->pEngine->avsConfigSet(config);

  return *this;
}

Avs &Avs::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->avsEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Avs &Avs::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] =
      pCamera->pEngine->avsEnableSet(jQuery[KEY_ENABLE].asBool());

  return *this;
}

Avs &Avs::statusGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Avs::Status status;

  jResponse[REST_RET] = pCamera->pEngine->avsStatusGet(status);

  jResponse[KEY_DISPLACEMENT][KEY_X] = status.displacement.first;
  jResponse[KEY_DISPLACEMENT][KEY_Y] = status.displacement.second;
  jResponse[KEY_OFFSET][KEY_X] = status.offset.first;
  jResponse[KEY_OFFSET][KEY_Y] = status.offset.second;

  return *this;
}
