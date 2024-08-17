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

#include "shell/lsc.hpp"
#ifdef APPMODE_NATIVE
#include "calibration/lsc.hpp"
#endif

using namespace sh;

Lsc &Lsc::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case EnableGet:
    return enableGet(jQuery, jResponse);

  case EnableSet:
    return enableSet(jQuery, jResponse);

  case StatusGet:
    return statusGet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Lsc &Lsc::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_LSC_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->lscEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Lsc &Lsc::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_LSC_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->lscEnableSet(isEnable);
#endif

  return *this;
}

Lsc &Lsc::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_LSC_G_STATUS, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Lsc::Status status;

  jResponse[REST_RET] = pCamera->pEngine->lscStatusGet(status);

  binEncode(status.config, jResponse[KEY_CONFIG]);
#endif

  return *this;
}
