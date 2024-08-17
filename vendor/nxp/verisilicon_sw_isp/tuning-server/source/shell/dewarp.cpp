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

#include "shell/dewarp.hpp"
#include "dewarp/dewarp.hpp"

using namespace sh;

Dewarp &Dewarp::process(Json::Value &jQuery, Json::Value &jResponse) {
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

Dewarp &Dewarp::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

#if defined CTRL_DEWARP
  clb::Dewarp::Config config;

  jResponse[REST_RET] = pCamera->dewarp().configGet(config);

  binEncode(config, jResponse[KEY_CONFIG]);

  binEncode(config.distortionMap[0].pUserMap,
            config.distortionMap[0].userMapSize, jResponse[KEY_MAP]);
#else
  jResponse = jResponse;
#endif

  return *this;
}

Dewarp &Dewarp::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#if defined CTRL_DEWARP
  clb::Dewarp::Config config;

  binDecode(jQuery[KEY_CONFIG], config);

  void *pUserMapBuffer = nullptr;
  uint32_t userMapSize = 0;

  binDecode(jQuery[KEY_MAP], &pUserMapBuffer, userMapSize);

  for (int32_t i = 0; i < DEWARP_CORE_MAX; i++) {
    dewarp_distortion_map *pDistortion = config.distortionMap + i;

    pDistortion->pUserMap = static_cast<uint32_t *>(pUserMapBuffer);
    pDistortion->userMapSize = userMapSize;
  }

  jResponse[REST_RET] = pCamera->dewarp().configSet(config);
#else
  jQuery = jQuery;
  jResponse = jResponse;
#endif

  return *this;
}

Dewarp &Dewarp::enableGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

#if defined CTRL_DEWARP
  bool isEnable = false;

  jResponse[REST_RET] = pCamera->dewarp().enableGet(isEnable);

  jResponse[KEY_ENABLE] = isEnable;
#else
  jResponse = jResponse;
#endif

  return *this;
}

Dewarp &Dewarp::enableSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

#if defined CTRL_DEWARP
  bool isEnable = jQuery[KEY_ENABLE].asBool();

  jResponse[REST_RET] = pCamera->dewarp().enableSet(isEnable);
#else
  jQuery = jQuery;
  jResponse = jResponse;
#endif

  return *this;
}
