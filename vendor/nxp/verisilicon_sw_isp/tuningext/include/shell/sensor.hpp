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

#pragma once

#include "shell.hpp"

namespace sh {

class Sensor : public Shell {
public:
  enum {
    Begin = Shell::Sensor * Shell::Step,

    CalibrationList,
    Caps,
    ConfigGet,
    ConfigSet,
    DriverChange,
    DriverList,
    Info,
    RegisterDescription,
    RegisterDumpToFile,
    RegisterGet,
    RegisterSet,
    RegisterTable,
    ResolutionSet,
    ResolutionGet,
    TestPatternEnableSet,
    ResolutionSupportGet,
    FrameRateGet,
    FrameRateSet,
    ModeGet,
    ModeSet,
    StreamingIsRuning,

    End,
  };

  Sensor() {
    idBegin = Begin;
    idEnd = End;
  }

  Sensor &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Sensor &calibraionList(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &caps(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &configGet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &configSet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &driverChange(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &driverList(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &info(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &registerDescription(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &registerDump2File(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &registerGet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &registerSet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &registerTable(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &resolutionSet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &resolutionGet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &testPatternEnableSet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &resolutionSupportListGet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &frameRateGet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &frameRateSet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &modeGet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &modeSet(Json::Value &jQuery, Json::Value &jResponse);
  Sensor &streamingIsRuning(Json::Value &jQuery, Json::Value &jResponse);
};

} // namespace sh
