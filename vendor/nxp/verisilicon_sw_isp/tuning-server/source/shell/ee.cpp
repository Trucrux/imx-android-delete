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

#include "shell/ee.hpp"

using namespace sh;

Ee &Ee::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Ee &Ee::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Ee::Config config;

  jResponse[REST_RET] = pCamera->pEngine->eeConfigGet(config);

  jResponse[KEY_AUTO] = config.isAuto;

  binEncode(config.config, jResponse[KEY_CONFIG]);

  return *this;
}

Ee &Ee::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Ee::Config config;

  config.isAuto = jQuery[KEY_AUTO].asBool();

  binDecode(jQuery[KEY_CONFIG], config.config);

  jResponse[REST_RET] = pCamera->pEngine->eeConfigSet(config);

  return *this;
}

Ee &Ee::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->eeEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Ee &Ee::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] =
      pCamera->pEngine->eeEnableSet(jQuery[KEY_ENABLE].asBool());

  return *this;
}

Ee &Ee::reset(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = pCamera->pEngine->eeReset();

  return *this;
}

Ee &Ee::statusGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Ee::Status status;

  jResponse[REST_RET] = pCamera->pEngine->eeStatusGet(status);

  jResponse[KEY_GAIN] = status.gain;
  jResponse[KEY_INTEGRATION_TIME] = status.integrationTime;

  return *this;
}

Ee &Ee::tableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  Json::Value jTable;

  jResponse[REST_RET] = pCamera->pEngine->eeTableGet(jTable);

  jResponse[KEY_TABLE] = jTable;

  return *this;
}

Ee &Ee::tableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  Json::Value jTable = jQuery[KEY_TABLE];

  jResponse[REST_RET] = pCamera->pEngine->eeTableSet(jTable);

  return *this;
}
