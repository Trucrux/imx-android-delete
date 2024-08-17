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

#include "shell/cli.hpp"
#include "versions.hpp"

using namespace sh;

Cli &Cli::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case ControlListGet:
    return controlListGet(jQuery, jResponse);

  case VersionGet:
    return versionGet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Cli &Cli::controlListGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = RET_SUCCESS;

#if defined ISP_2DNR
  jResponse[KEY_CONTROL_LIST].append(KEY_2DNR);
#endif

#if defined ISP_3DNR
  jResponse[KEY_CONTROL_LIST].append(KEY_3DNR);
#endif

#if defined ISP_AVS
  jResponse[KEY_CONTROL_LIST].append(KEY_AVS);
#endif

#if defined ISP_CNR
  jResponse[KEY_CONTROL_LIST].append(KEY_CNR);
#endif

#if defined ISP_EE
  jResponse[KEY_CONTROL_LIST].append(KEY_EE);
#endif

#if defined ISP_HDR
  jResponse[KEY_CONTROL_LIST].append(KEY_HDR);
#endif

#if defined ISP_IE
  jResponse[KEY_CONTROL_LIST].append(KEY_IE);
#endif

#if defined ISP_WDR2
  jResponse[KEY_CONTROL_LIST].append(KEY_WDR2);
#endif

#if defined ISP_WDR3
  jResponse[KEY_CONTROL_LIST].append(KEY_WDR3);
#endif

  return *this;
}

Cli &Cli::versionGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = RET_SUCCESS;

  Versions versions;

  std::for_each(versions.collection.begin(), versions.collection.end(),
                [&](Versions::Version *pVersion) {
                  Json::Value jVersion;

                  jVersion[KEY_NAME] = pVersion->name;
                  jVersion[KEY_NUMBER] = pVersion->number;
                  jVersion[KEY_DATE] = pVersion->date;

                  jResponse[KEY_VERSIONS].append(jVersion);
                });

  return *this;
}
