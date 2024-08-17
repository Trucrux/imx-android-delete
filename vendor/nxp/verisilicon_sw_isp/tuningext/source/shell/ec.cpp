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

#include "shell/ec.hpp"
#ifdef APPMODE_NATIVE
#include "calibration/sensors.hpp"
#endif

#ifdef APPMODE_V4L2
#include "ioctl/v4l2-ioctl.hpp"
#endif

using namespace sh;

Ec &Ec::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case ConfigGet:
    return configGet(jQuery, jResponse);

  case ConfigSet:
    return configSet(jQuery, jResponse);

  case StatusGet:
    return statusGet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Ec &Ec::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_EC_G_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Sensor::Config::Ec ec;

  jResponse[REST_RET] = pCamera->sensor().checkValid().ecConfigGet(ec);

  jResponse[KEY_GAIN] = static_cast<double>(ec.gain);
  jResponse[KEY_INTEGRATION_TIME] = static_cast<double>(ec.integrationTime);
#endif

  return *this;
}

Ec &Ec::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_EC_S_CFG, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Sensor::Config::Ec ec;

  ec.gain = jQuery[KEY_GAIN].asFloat();
  ec.integrationTime = jQuery[KEY_INTEGRATION_TIME].asFloat();

  jResponse[REST_RET] = pCamera->sensor().checkValid().ecConfigSet(ec);
#endif

  return *this;
}

Ec &Ec::statusGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#ifdef APPMODE_V4L2
    viv_private_ioctl(IF_EC_G_STATUS, jQuery, jResponse);
#endif

#ifdef APPMODE_NATIVE
  clb::Sensor::Status::Ec ec;

  jResponse[REST_RET] = pCamera->sensor().checkValid().ecStatusGet(ec);

  Json::Value &jGain = jResponse[KEY_GAIN];

  jGain[KEY_MAX] = ec.gain.max;
  jGain[KEY_MIN] = ec.gain.min;
  jGain[KEY_STEP] = ec.gain.step;

  Json::Value &jIntegrationTime = jResponse[KEY_INTEGRATION_TIME];

  jIntegrationTime[KEY_MAX] = ec.integrationTime.max;
  jIntegrationTime[KEY_MIN] = ec.integrationTime.min;
  jIntegrationTime[KEY_STEP] = ec.integrationTime.step;
#endif

  return *this;
}
