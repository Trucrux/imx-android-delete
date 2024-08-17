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
//#include "dewarp/dewarp.hpp"
#ifdef APPMODE_V4L2
#include "ioctl/v4l2-ioctl.hpp"
#endif


using namespace sh;

Dewarp &Dewarp::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case ConfigGet:
    return configGet(jQuery, jResponse);

  case HFlipSet:
    return hFlipSet(jQuery, jResponse);

  case VFlipSet:
    return vFlipSet(jQuery, jResponse);

  case DweTypeSet:
    return dweTypeSet(jQuery, jResponse);

  case MatrixSet:
    return matrixSet(jQuery, jResponse);

  case BypassSet:
    return bypassSet(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Dewarp &Dewarp::configGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_DWE_G_PARAMS, jQuery, jResponse);

  return *this;
}

Dewarp &Dewarp::hFlipSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_DWE_S_HFLIP, jQuery, jResponse);

  return *this;
}

Dewarp &Dewarp::vFlipSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_DWE_S_VFLIP, jQuery, jResponse);

  return *this;
}

Dewarp &Dewarp::dweTypeSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_DWE_S_TYPE, jQuery, jResponse);

  return *this;
}

Dewarp &Dewarp::matrixSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_DWE_S_MAT, jQuery, jResponse);

  return *this;
}

Dewarp &Dewarp::bypassSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_DWE_S_BYPASS, jQuery, jResponse);

  return *this;
}

