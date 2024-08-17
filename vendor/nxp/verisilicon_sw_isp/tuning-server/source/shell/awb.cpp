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

#include "shell/awb.hpp"

using namespace sh;

Awb &Awb::process(Json::Value &jQuery, Json::Value &jResponse) {
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

  case IlluminationProfilesGet:
    return illuminanceProfilesGet(jQuery, jResponse);

  case Reset:
    return reset(jQuery, jResponse);

  case StatusGet:
    return statusGet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Awb &Awb::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Awb::Config config;

  jResponse[REST_RET] = pCamera->pEngine->awbConfigGet(config);

  jResponse[KEY_MODE] = config.mode;
  jResponse[KEY_INDEX] = config.index;
  jResponse[KEY_DAMPING] = config.isDamping;

  return *this;
}

Awb &Awb::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Awb::Config config;

  config.mode = static_cast<CamEngineAwbMode_t>(jQuery[KEY_MODE].asInt());
  config.index = jQuery[KEY_INDEX].asUInt();
  config.isDamping = jQuery[KEY_DAMPING].asBool();

  jResponse[REST_RET] = pCamera->pEngine->awbConfigSet(config);

  return *this;
}

Awb &Awb::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->awbEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;

  return *this;
}

Awb &Awb::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] =
      pCamera->pEngine->awbEnableSet(jQuery[KEY_ENABLE].asBool());

  return *this;
}

Awb &Awb::illuminanceProfilesGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  std::vector<CamIlluProfile_t *> profiles;

  jResponse[REST_RET] =
      pCamera->sensor().checkValid().illuminationProfilesGet(profiles);

  for (uint32_t i = 0; i < profiles.size(); i++) {
    CamIlluProfile_t *pProfile = profiles[i];

    Json::Value jProfile;

    binEncode(*pProfile, jProfile);

    jResponse[KEY_PROFILES].append(jProfile);
  }

  return *this;
}

Awb &Awb::reset(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = pCamera->pEngine->awbReset();

  return *this;
}

Awb &Awb::statusGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  clb::Awb::Status status;

  jResponse[REST_RET] = pCamera->pEngine->awbStatusGet(status);

  binEncode(status.rgProj, jResponse[KEY_STATUS]);

  return *this;
}
