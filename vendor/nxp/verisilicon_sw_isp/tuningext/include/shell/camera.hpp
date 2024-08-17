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

class Camera : public Shell {
public:
  enum {
    Begin = Shell::Camera * Shell::Step,

    CalibrationGet,
    CalibrationSet,
    CaptureDma,
    CaptureSensor,
    InputInfo,
    InputSwitch,
    Preview,
    GetPicBufLayout,
    SetPicBufLayout,

    End,
  };

  enum SnapshotType { RAW8, RAW10, RAW12, RAW16, RGB, YUV, JPEG };

  Camera() {
    idBegin = Begin;
    idEnd = End;
  }

  Camera &process(Json::Value &jQuery, Json::Value &jResponse) override;

  Camera &calibrationGet(Json::Value &jQuery, Json::Value &jResponse);
  Camera &calibrationSet(Json::Value &jQuery, Json::Value &jResponse);

  Camera &captureDma(Json::Value &jQuery, Json::Value &jResponse);
  Camera &captureSensor(Json::Value &jQuery, Json::Value &jResponse);

  Camera &inputInfo(Json::Value &jQuery, Json::Value &jResponse);
  Camera &inputSwitch(Json::Value &jQuery, Json::Value &jResponse);

  Camera &preview(Json::Value &jQuery, Json::Value &jResponse);

  Camera &getPicBufLayout(Json::Value &jQuery, Json::Value &jResponse);
  Camera &setPicBufLayout(Json::Value &jQuery, Json::Value &jResponse);

private:
  int32_t captureSensorRAW(std::string fileName, SnapshotType snapshotType);
};

} // namespace sh
