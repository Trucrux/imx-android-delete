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

#ifdef APPMODE_V4L2
#include "ioctl/v4l2-ioctl.hpp"
#endif

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

Filter &Filter::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Filter::Config config;

  jResponse[REST_RET] = pCamera->pEngine->filterConfigGet(config);

  jResponse[KEY_AUTO] = config.isAuto;

  jResponse[KEY_DENOISE] = config.denoise;
  jResponse[KEY_SHARPEN] = config.sharpen;
  jResponse[KEY_CHRV] = config.chrV;
  jResponse[KEY_CHRH] = config.chrH;
#endif

  return *this;
}

Filter &Filter::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Filter::Config config;

  config.isAuto = jQuery[KEY_AUTO].asBool();

  config.denoise = jQuery[KEY_DENOISE].asInt();
  config.sharpen = jQuery[KEY_SHARPEN].asInt();
  config.chrV = jQuery[KEY_CHRV].asInt();
  config.chrH = jQuery[KEY_CHRH].asInt();

  jResponse[REST_RET] = pCamera->pEngine->filterConfigSet(config);
#endif

  return *this;
}

Filter &Filter::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->filterEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Filter &Filter::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->filterEnableSet(isEnable);
#endif

  return *this;
}

Filter &Filter::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_G_STATUS, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Filter::Status status;

  jResponse[REST_RET] = pCamera->pEngine->filterStatusGet(status);

  jResponse[KEY_GAIN] = status.gain;
  jResponse[KEY_INTEGRATION_TIME] = status.integrationTime;
#endif

  return *this;
}

Filter &Filter::tableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_G_TBL, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  Json::Value jTable;

  jResponse[REST_RET] = pCamera->pEngine->filterTableGet(jTable);

  jResponse[KEY_TABLE] = jTable;
#endif

  return *this;
}

Filter &Filter::tableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_FILTER_S_TBL, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  Json::Value jTable = jQuery[KEY_TABLE];

  jResponse[REST_RET] = pCamera->pEngine->filterTableSet(jTable);
#endif

  return *this;
}
