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

#include "shell/filter.hpp"

using namespace sh;

Filter &Filter::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Filter &Filter::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Filter::Config config;

  jResponse[REST_RET] = pCamera->pEngine->filterConfigGet(config);

  jResponse[KEY_AUTO] = config.isAuto;

  jResponse[KEY_DENOISE] = config.denoise;
  jResponse[KEY_SHARPEN] = config.sharpen;
  jResponse[KEY_CHRV] = config.chrV;
  jResponse[KEY_CHRH] = config.chrH;

  return *this;
}

Filter &Filter::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Filter::Config config;

  config.isAuto = jQuery[KEY_AUTO].asBool();

  config.denoise = jQuery[KEY_DENOISE].asInt();
  config.sharpen = jQuery[KEY_SHARPEN].asInt();
  config.chrV = jQuery[KEY_CHRV].asInt();
  config.chrH = jQuery[KEY_CHRH].asInt();

  jResponse[REST_RET] = pCamera->pEngine->filterConfigSet(config);

  return *this;
}

Filter &Filter::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->filterEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Filter &Filter::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->filterEnableSet(isEnable);

  return *this;
}

Filter &Filter::statusGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Filter::Status status;

  jResponse[REST_RET] = pCamera->pEngine->filterStatusGet(status);

  jResponse[KEY_GAIN] = status.gain;
  jResponse[KEY_INTEGRATION_TIME] = status.integrationTime;

  return *this;
}

Filter &Filter::tableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  Json::Value jTable;

  jResponse[REST_RET] = pCamera->pEngine->filterTableGet(jTable);

  jResponse[KEY_TABLE] = jTable;

  return *this;
}

Filter &Filter::tableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  Json::Value jTable = jQuery[KEY_TABLE];

  jResponse[REST_RET] = pCamera->pEngine->filterTableSet(jTable);

  return *this;
}
