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

#include "shell/wb.hpp"

using namespace sh;

Wb &Wb::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Wb &Wb::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wb::Config config;

  jResponse[REST_RET] = pCamera->pEngine->wbConfigGet(config);

  binEncode(config.ccMatrix, jResponse[KEY_CC_MATRIX]);
  binEncode(config.ccOffset, jResponse[KEY_CC_OFFSET]);
  binEncode(config.wbGains, jResponse[KEY_WB_GAINS]);

  return *this;
}

Wb &Wb::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Wb::Config config;

  binDecode(jQuery[KEY_CC_MATRIX], config.ccMatrix);
  binDecode(jQuery[KEY_CC_OFFSET], config.ccOffset);
  binDecode(jQuery[KEY_WB_GAINS], config.wbGains);

  jResponse[REST_RET] = pCamera->pEngine->wbConfigSet(config);

  return *this;
}
