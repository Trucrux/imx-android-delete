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

#include "shell/gc.hpp"

using namespace sh;

Gc &Gc::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case CurveGet:
    return curveGet(jQuery, jResponse);

  case CurveSet:
    return curveSet(jQuery, jResponse);

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

Gc &Gc::curveGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Gc::Config config;

  jResponse[REST_RET] = pCamera->pEngine->gcConfigGet(config);

  binEncode(config.curve, jResponse[KEY_CURVE]);

  return *this;
}

Gc &Gc::curveSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Gc::Config config;

  binDecode(jQuery[KEY_CURVE], config.curve);

  jResponse[REST_RET] = pCamera->pEngine->gcConfigSet(config);

  return *this;
}

Gc &Gc::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->gcEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Gc &Gc::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] =
      pCamera->pEngine->gcEnableSet(jQuery[KEY_ENABLE].asBool());

  return *this;
}
