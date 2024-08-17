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

#include "shell/reg.hpp"

using namespace sh;

Reg &Reg::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  int32_t id = jQuery[REST_ID].asInt();

  switch (id) {
  case Description:
    return description(jQuery, jResponse);

  case Get:
    return get(jQuery, jResponse);

  case Set:
    return set(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Reg &Reg::description(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  int32_t ret = RET_SUCCESS;

  CamerIcModuleId_t moduleId = (CamerIcModuleId_t)jQuery[KEY_MODULE_ID].asInt();

  uint32_t numRegisters = 0;
  RegDescription_t *pRegDescriptions = NULL;

  ret =
      CamerIcGetRegisterDescription(moduleId, &numRegisters, &pRegDescriptions);

  jResponse[REST_RET] = ret;

  jResponse[KEY_COUNT] = numRegisters;

  for (uint32_t i = 0; i < numRegisters; i++) {
    Json::Value jRegister;

    RegDescription_t *pRegDescription = pRegDescriptions + i;

    binEncode(*pRegDescription, jRegister);

    jResponse[KEY_REGISTERS].append(jRegister);
  }

  return *this;
}

Reg &Reg::get(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  int32_t ret = RET_SUCCESS;

  //uint32_t address = jQuery[KEY_ADDRESS].asUInt();
  uint32_t value = 0;

  //ret = CamerIcGetRegister(address, &value);

  jResponse[REST_RET] = ret;

  jResponse[KEY_VALUE] = value;

  return *this;
}

Reg &Reg::set(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  int32_t ret = RET_SUCCESS;

  //uint32_t address = jQuery[KEY_ADDRESS].asUInt();
  //uint32_t value = jQuery[KEY_VALUE].asUInt();

  //ret = CamerIcSetRegister(address, value);

  jResponse[REST_RET] = ret;

  return *this;
}
