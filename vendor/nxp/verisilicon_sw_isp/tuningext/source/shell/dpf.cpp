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

#include "shell/dpf.hpp"

using namespace sh;

Dpf &Dpf::process(Json::Value &jQuery, Json::Value &jResponse) {
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

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Dpf &Dpf::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DPF_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Dpf::Config config;

  jResponse[REST_RET] = pCamera->pEngine->dpfConfigGet(config);

  jResponse[KEY_GRADIENT] = config.gradient;
  jResponse[KEY_OFFSET] = config.offset;
  jResponse[KEY_MIN] = config.minimumBound;
  jResponse[KEY_DIV] = config.divisionFactor;
  jResponse[KEY_SIGMA_GREEN] = config.sigmaGreen;
  jResponse[KEY_SIGMA_RED_BLUE] = config.sigmaRedBlue;
#endif

  return *this;
}

Dpf &Dpf::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DPF_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Dpf::Config config;

  config.gradient = jQuery[KEY_GRADIENT].asFloat();
  config.offset = jQuery[KEY_OFFSET].asFloat();
  config.minimumBound = jQuery[KEY_MIN].asFloat();
  config.divisionFactor = jQuery[KEY_DIV].asFloat();
  config.sigmaGreen = static_cast<uint8_t>(jQuery[KEY_SIGMA_GREEN].asUInt());
  config.sigmaRedBlue =
      static_cast<uint8_t>(jQuery[KEY_SIGMA_RED_BLUE].asUInt());

  jResponse[REST_RET] = pCamera->pEngine->dpfConfigSet(config);
#endif

  return *this;
}

Dpf &Dpf::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DPF_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->dpfEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Dpf &Dpf::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DPF_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->dpfEnableSet(isEnable);
#endif

  return *this;
}
