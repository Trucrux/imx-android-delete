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

#include "shell/dpcc.hpp"

using namespace sh;

Dpcc &Dpcc::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
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

Dpcc &Dpcc::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->dpccEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Dpcc &Dpcc::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->pEngine->dpccEnableSet(isEnable);

  return *this;
}
