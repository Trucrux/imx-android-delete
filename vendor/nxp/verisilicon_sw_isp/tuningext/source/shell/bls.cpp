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

#include "shell/bls.hpp"

using namespace sh;

Bls &Bls::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case ConfigGet:
    return configGet(jQuery, jResponse);

  case ConfigSet:
    return configSet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Bls &Bls::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_BLS_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Bls::Config config;

  jResponse[REST_RET] = pCamera->pEngine->blsConfigGet(config);

  jResponse[KEY_RED] = config.red;
  jResponse[KEY_GREEN_R] = config.greenR;
  jResponse[KEY_GREEN_B] = config.greenB;
  jResponse[KEY_BLUE] = config.blue;
#endif

  return *this;
}

Bls &Bls::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_BLS_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Bls::Config config;

  config.isBypass = false;
  config.red = jQuery[KEY_RED].asInt();
  config.greenR = jQuery[KEY_GREEN_R].asInt();
  config.greenB = jQuery[KEY_GREEN_B].asInt();
  config.blue = jQuery[KEY_BLUE].asInt();

  jResponse[REST_RET] = pCamera->pEngine->blsConfigSet(config);
#endif

  return *this;
}
