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
#include "shell/dnr3.hpp"

using namespace sh;

Dnr3 &Dnr3::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Dnr3 &Dnr3::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr3::Config config;
  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr3ConfigGet(config, generation);

  if (generation == clb::Dnr3::V1) {
    auto &v1 = config.v1;

    jResponse[KEY_AUTO] = v1.isAuto;
    jResponse[KEY_AUTO_LEVEL] = v1.autoLevel;

    jResponse[KEY_DELTA_FACTOR] = v1.deltaFactor;
    jResponse[KEY_MOTION_FACTOR] = v1.motionFactor;
    jResponse[KEY_STRENGTH] = v1.strength;
  }

  return *this;
}

Dnr3 &Dnr3::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr3::Config config;
  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  if (generation == clb::Dnr3::V1) {
    auto &v1 = config.v1;

    v1.isAuto = jQuery[KEY_AUTO].asBool();
    v1.autoLevel = jQuery[KEY_AUTO_LEVEL].asInt();

    v1.deltaFactor = jQuery[KEY_DELTA_FACTOR].asInt();
    v1.motionFactor = jQuery[KEY_MOTION_FACTOR].asInt();
    v1.strength = jQuery[KEY_STRENGTH].asInt();
  }

  jResponse[REST_RET] = pCamera->pEngine->dnr3ConfigSet(config, generation);

  return *this;
}

Dnr3 &Dnr3::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;
  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr3EnableGet(isEnable, generation);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Dnr3 &Dnr3::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();
  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr3EnableSet(isEnable, generation);

  return *this;
}

Dnr3 &Dnr3::reset(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr3Reset(generation);

  return *this;
}

Dnr3 &Dnr3::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  clb::Dnr3::Status status;

  jResponse[REST_RET] = pCamera->pEngine->dnr3StatusGet(status, generation);

  jResponse[KEY_GAIN] = status.gain;
  jResponse[KEY_INTEGRATION_TIME] = status.integrationTime;

  return *this;
}

Dnr3 &Dnr3::tableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  Json::Value jTable;

  jResponse[REST_RET] = pCamera->pEngine->dnr3TableGet(jTable, generation);

  jResponse[KEY_TABLE] = jTable;

  return *this;
}

Dnr3 &Dnr3::tableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr3::Generation generation =
      static_cast<clb::Dnr3::Generation>(jQuery[KEY_GENERATION].asInt());

  Json::Value jTable = jQuery[KEY_TABLE];

  jResponse[REST_RET] = pCamera->pEngine->dnr3TableSet(jTable, generation);

  return *this;
}
