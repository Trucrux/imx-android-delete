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
#include "shell/dnr2.hpp"

using namespace sh;

Dnr2 &Dnr2::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Dnr2 &Dnr2::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr2::Config config;
  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr2ConfigGet(config, generation);

  if (generation == clb::Dnr2::V1) {
    auto &v1 = config.v1;

    jResponse[KEY_AUTO] = v1.isAuto;
    jResponse[KEY_AUTO_LEVEL] = v1.autoLevel;

    jResponse[KEY_DENOISE_PREGAMA_STRENGTH] = v1.denoisePregamaStrength;
    jResponse[KEY_DENOISE_STRENGTH] = v1.denoiseStrength;
    jResponse[KEY_SIGMA] = v1.sigma;
  }

  return *this;
}

Dnr2 &Dnr2::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr2::Config config;
  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  if (generation == clb::Dnr2::V1) {
    auto &v1 = config.v1;

    v1.isAuto = jQuery[KEY_AUTO].asBool();
    v1.autoLevel = jQuery[KEY_AUTO_LEVEL].asInt();

    v1.denoisePregamaStrength = jQuery[KEY_DENOISE_PREGAMA_STRENGTH].asInt();
    v1.denoiseStrength = jQuery[KEY_DENOISE_STRENGTH].asInt();
    v1.sigma = jQuery[KEY_SIGMA].asDouble();
  }

  jResponse[REST_RET] = pCamera->pEngine->dnr2ConfigSet(config, generation);

  return *this;
}

Dnr2 &Dnr2::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;
  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr2EnableGet(isEnable, generation);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Dnr2 &Dnr2::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();
  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr2EnableSet(isEnable, generation);

  return *this;
}

Dnr2 &Dnr2::reset(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  jResponse[REST_RET] = pCamera->pEngine->dnr2Reset(generation);

  return *this;
}

Dnr2 &Dnr2::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  clb::Dnr2::Status status;

  jResponse[REST_RET] = pCamera->pEngine->dnr2StatusGet(status, generation);

  jResponse[KEY_GAIN] = status.gain;
  jResponse[KEY_INTEGRATION_TIME] = status.integrationTime;

  return *this;
}

Dnr2 &Dnr2::tableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  Json::Value jTable;

  jResponse[REST_RET] = pCamera->pEngine->dnr2TableGet(jTable, generation);

  jResponse[KEY_TABLE] = jTable;

  return *this;
}

Dnr2 &Dnr2::tableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Dnr2::Generation generation =
      static_cast<clb::Dnr2::Generation>(jQuery[KEY_GENERATION].asInt());

  Json::Value jTable = jQuery[KEY_TABLE];

  jResponse[REST_RET] = pCamera->pEngine->dnr2TableSet(jTable, generation);

  return *this;
}
