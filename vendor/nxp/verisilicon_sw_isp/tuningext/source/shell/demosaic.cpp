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

#include "shell/demosaic.hpp"

#ifdef APPMODE_V4L2
#include "ioctl/v4l2-ioctl.hpp"
#endif

using namespace sh;

Demosaic &Demosaic::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Demosaic &Demosaic::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DEMOSAIC_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Demosaic::Config config;

  jResponse[REST_RET] = pCamera->pEngine->demosaicConfigGet(config);

  jResponse[KEY_THRESHOLD] = config.threshold;
#endif

  return *this;
}

Demosaic &Demosaic::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DEMOSAIC_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Demosaic::Config config;

  config.threshold = jQuery[KEY_THRESHOLD].asInt();

  jResponse[REST_RET] = pCamera->pEngine->demosaicConfigSet(config);
#endif

  return *this;
}

Demosaic &Demosaic::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DEMOSAIC_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->demosaicEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Demosaic &Demosaic::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_DEMOSAIC_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->demosaicEnableSet(isEnable);
#endif

  return *this;
}
