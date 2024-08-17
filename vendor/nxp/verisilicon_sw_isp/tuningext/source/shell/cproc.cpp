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

#include "shell/cproc.hpp"

using namespace sh;

Cproc &Cproc::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Cproc &Cproc::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_CPROC_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Cproc::Config config;

  jResponse[REST_RET] = pCamera->pEngine->cprocConfigGet(config);

  binEncode(config.config, jResponse[KEY_CONFIG]);
#endif

  return *this;
}

Cproc &Cproc::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_CPROC_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Cproc::Config config;

  binDecode(jQuery[KEY_CONFIG], config.config);

  jResponse[REST_RET] = pCamera->pEngine->cprocConfigSet(config);
#endif

  return *this;
}

Cproc &Cproc::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_CPROC_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->cprocEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Cproc &Cproc::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_CPROC_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->cprocEnableSet(isEnable);
#endif

  return *this;
}
