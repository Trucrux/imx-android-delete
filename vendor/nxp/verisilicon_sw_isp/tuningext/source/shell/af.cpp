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

Af &Af::availableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AF_G_AVI, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isAvailable = true;

  jResponse[REST_RET] = pCamera->pEngine->afAvailableGet(isAvailable);

  jResponse[KEY_AVAILABLE] = isAvailable;
#endif

  return *this;
}

Af &Af::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AF_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Af::Config config;

  jResponse[REST_RET] = pCamera->pEngine->afConfigGet(config);

  jResponse[KEY_ALGORITHM] = config.searchAlgorithm;
  jResponse[KEY_ONESHOT] = config.isOneshot;
  jResponse[KEY_MODE] = config.mode;
  jResponse[KEY_LENGTH] = config.pos;
#endif


  return *this;
}

Af &Af::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AF_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Af::Config config;

  config.searchAlgorithm =
      static_cast<CamEngineAfSearchAlgorithm_t>(jQuery[KEY_ALGORITHM].asInt());
  config.isOneshot = jQuery[KEY_ONESHOT].asBool();

  config.mode = static_cast<CamEngineAfWorkMode_t>(jQuery[KEY_MODE].asInt());
  config.pos = jQuery[KEY_LENGTH].asUInt();

  jResponse[REST_RET] = pCamera->pEngine->afConfigSet(config);
#endif

  return *this;
}

Af &Af::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AF_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->afEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Af &Af::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AF_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->afEnableSet(isEnable);
#endif

  return *this;
}
