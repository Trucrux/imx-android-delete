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


#include "common/json-app.hpp"
#include "common/exception.hpp"

Json::Value JsonApp::fromString(std::string description) {
  if (description.empty()) {
    return Json::Value();
  }

  Json::CharReaderBuilder charReaderBuilder;
  Json::CharReader *pCharReader = charReaderBuilder.newCharReader();

  std::string errors;

  Json::Value jValue;

  bool isSuccess = pCharReader->parse(description.c_str(),
                                      description.c_str() + description.size(),
                                      &jValue, &errors);
  delete pCharReader;

  if (!isSuccess) {
    throw exc::LogicError(-1, "Parse JSON failed:" + errors);
  }

  return jValue;
}

std::string JsonApp::toString(Json::Value jValue, std::string indentation) {
  Json::StreamWriterBuilder streamWriterBuilder;

  streamWriterBuilder["indentation"] = indentation;

  return Json::writeString(streamWriterBuilder, jValue);
}
