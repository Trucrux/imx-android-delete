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

Awb &Awb::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Awb::Config config;

  jResponse[REST_RET] = pCamera->pEngine->awbConfigGet(config);

  jResponse[KEY_MODE] = config.mode;
  jResponse[KEY_INDEX] = config.index;
  jResponse[KEY_DAMPING] = config.isDamping;
#endif

  return *this;
}

Awb &Awb::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Awb::Config config;

  config.mode = static_cast<CamEngineAwbMode_t>(jQuery[KEY_MODE].asInt());
  config.index = jQuery[KEY_INDEX].asUInt();
  config.isDamping = jQuery[KEY_DAMPING].asBool();

  jResponse[REST_RET] = pCamera->pEngine->awbConfigSet(config);
#endif

  return *this;
}

Awb &Awb::enableGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_G_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->pEngine->awbEnableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#endif

  return *this;
}

Awb &Awb::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_S_EN, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  jResponse[REST_RET] =
      pCamera->pEngine->awbEnableSet(jQuery[KEY_ENABLE].asBool());
#endif

  return *this;
}

Awb &Awb::illuminanceProfilesGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_G_ILLUMPRO, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  std::vector<CamIlluProfile_t *> profiles;

  jResponse[REST_RET] =
      pCamera->sensor().checkValid().illuminationProfilesGet(profiles);

  for (uint32_t i = 0; i < profiles.size(); i++) {
    CamIlluProfile_t *pProfile = profiles[i];

    Json::Value jProfile;

    binEncode(*pProfile, jProfile);

    jResponse[KEY_PROFILES].append(jProfile);
  }
#endif

  return *this;
}

Awb &Awb::reset(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_RESET, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  jResponse[REST_RET] = pCamera->pEngine->awbReset();
#endif

  return *this;
}

Awb &Awb::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_AWB_G_STATUS, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Awb::Status status;

  jResponse[REST_RET] = pCamera->pEngine->awbStatusGet(status);

  binEncode(status.rgProj, jResponse[KEY_STATUS]);
#endif

  return *this;
}
