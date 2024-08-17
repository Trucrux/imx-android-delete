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

#include "shell/wdr.hpp"
#include "base64/base64.hpp"

using namespace sh;

Wdr &Wdr::process(Json::Value &jQuery, Json::Value &jResponse) {
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

  case StatusGet:
    return statusGet(jQuery, jResponse);

  case TableGet:
    return tableGet(jQuery, jResponse);

  case TableSet:
    return tableSet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Wdr &Wdr::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  clb::Wdr::Config config;

  jResponse[REST_RET] = pCamera->pEngine->wdrConfigGet(config, generation);

  if (generation == clb::Wdr::V1) {
    auto &v1 = config.v1;

    binEncode(v1.curve, jResponse[KEY_CURVE]);
  } else if (generation == clb::Wdr::V2) {
    auto &v2 = config.v2;

    jResponse[KEY_STRENGTH] = v2.strength;
  } else if (generation == clb::Wdr::V3) {
    auto &v3 = config.v3;

    jResponse[KEY_AUTO] = v3.isAuto;
    jResponse[KEY_AUTO_LEVEL] = v3.autoLevel;

    jResponse[KEY_GAIN_MAX] = v3.gainMax;
    jResponse[KEY_STRENGTH] = v3.strength;
    jResponse[KEY_STRENGTH_GLOBAL] = v3.strengthGlobal;
  }

  return *this;
}

Wdr &Wdr::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  clb::Wdr::Config config;

  if (generation == clb::Wdr::V1) {
    binDecode(jQuery[KEY_CURVE], config.v1.curve);
  } else if (generation == clb::Wdr::V2) {
    config.v2.strength = jQuery[KEY_STRENGTH].asFloat();
  } else if (generation == clb::Wdr::V3) {
    config.v3.isAuto = jQuery[KEY_AUTO].asBool();

    config.v3.gainMax = jQuery[KEY_GAIN_MAX].asInt();
    config.v3.strength = jQuery[KEY_STRENGTH].asInt();
    config.v3.strengthGlobal = jQuery[KEY_STRENGTH_GLOBAL].asInt();
  }

  jResponse[REST_RET] = pCamera->pEngine->wdrConfigSet(config, generation);

  return *this;
}

Wdr &Wdr::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->wdrEnableGet(isEnable, generation);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Wdr &Wdr::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->wdrEnableSet(isEnable, generation);

  return *this;
}

Wdr &Wdr::reset(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->wdrReset(generation);

  return *this;
}

Wdr &Wdr::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  clb::Wdr::Status status;

  jResponse[REST_RET] = pCamera->pEngine->wdrStatusGet(status, generation);

  jResponse[KEY_GAIN] = status.gain;
  jResponse[KEY_INTEGRATION_TIME] = status.integrationTime;

  return *this;
}

Wdr &Wdr::tableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  Json::Value jTable;

  jResponse[REST_RET] = pCamera->pEngine->wdrTableGet(jTable, generation);

  jResponse[KEY_TABLE] = jTable;

  return *this;
}

Wdr &Wdr::tableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wdr::Generation generation =
      static_cast<clb::Wdr::Generation>(jQuery[KEY_GENERATION].asInt());

  Json::Value jTable = jQuery[KEY_TABLE];

  jResponse[REST_RET] = pCamera->pEngine->wdrTableSet(jTable, generation);

  return *this;
}
