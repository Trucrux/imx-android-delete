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

#include "shell/camera.hpp"
#include "calibration/images.hpp"
#include "calibration/inputs.hpp"

using namespace sh;

Camera &Camera::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  auto id = jQuery[REST_ID].asInt();

  switch (id) {
  case CalibrationGet:
    return calibrationGet(jQuery, jResponse);

  case CalibrationSet:
    return calibrationSet(jQuery, jResponse);

  case CaptureDma:
    return captureDma(jQuery, jResponse);

  case CaptureSensor:
    return captureSensor(jQuery, jResponse);

  case InputInfo:
    return inputInfo(jQuery, jResponse);

  case InputSwitch:
    return inputSwitch(jQuery, jResponse);

  case Preview:
    return preview(jQuery, jResponse);

  case GetPicBufLayout:
    return getPicBufLayout(jQuery, jResponse);

  case SetPicBufLayout:
    return setPicBufLayout(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

Camera &Camera::calibrationGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto filename = "isp-" + jQuery[KEY_TIME].asString() + ".xml";

  pCalibration->store(filename);

  jResponse[REST_RET] = RET_SUCCESS;
  jResponse[KEY_CALIB_FILE] = filename;

  return *this;
}

Camera &Camera::calibrationSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  auto deviceStateBackup = pCamera->state;

  if (deviceStateBackup >= Object::Running) {
    ret = pCamera->previewStop();
    if (ret != RET_SUCCESS) {
      jResponse[REST_RET] = ret;
      return *this;
    }
  }

  if (deviceStateBackup >= Object::State::Idle) {
    ret = pCamera->pipelineDisconnect();
    if (ret != RET_SUCCESS) {
      jResponse[REST_RET] = ret;
      return *this;
    }
  }

  auto fileName = jQuery[KEY_CALIB_FILE].asString();
  pCalibration->restore(fileName);

  if (deviceStateBackup >= Object::State::Idle) {
    ret = pCamera->pipelineConnect(true);
    if (ret != RET_SUCCESS) {
      jResponse[REST_RET] = ret;
      return *this;
    }
  }

  if (deviceStateBackup >= Object::Running) {
    ret = pCamera->previewStart();
    if (ret != RET_SUCCESS) {
      jResponse[REST_RET] = ret;
      return *this;
    }
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Camera &Camera::captureDma(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto fileName = jQuery[KEY_FILE_NAME].asString();

  // if (pCamera
  //   delete pCamera // }

  // pDevicenew Camera;

  pCamera->pipelineDisconnect();

  auto &images = pCalibration->module<clb::Images>();

  images.images[pCalibration->module<clb::Inputs>().config.index]
      .config.fileName = fileName;

  pCamera->image().load(fileName);

  auto &inputs = pCalibration->module<clb::Inputs>();

  // clb::Input::Config configBackup = input.config;

  inputs.input().config.type = clb::Input::Image;

  jResponse[REST_RET] = pCamera->pipelineConnect(false);

  auto snapshotType =
      (camera::Abstract::SnapshotType)jQuery[KEY_SNAPSHOT_TYPE].asInt();

  jResponse[REST_RET] = pCamera->captureDma(fileName, snapshotType);

  jResponse[KEY_FILE_NAME] = fileName;

  // input.config = configBackup;

  return *this;
}

Camera &Camera::captureSensor(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto fileName = jQuery[KEY_FILE_NAME].asString();
  auto snapshotType =
      (camera::Abstract::SnapshotType)jQuery[KEY_SNAPSHOT_TYPE].asInt();
  auto resolution = jQuery[KEY_RESOLUTION].asUInt();
  auto lockType = (CamEngineLockType_t)jQuery[KEY_LOCK_TYPE].asInt();

  if (snapshotType == camera::Abstract::YUV) {
    jResponse[REST_RET] = pCamera->captureSensorYUV(fileName);
  } else {
    jResponse[REST_RET] =
      pCamera->captureSensor(fileName, snapshotType, resolution, lockType);
  }

  if (snapshotType == camera::Abstract::RGB) {
    fileName += ".ppm";
  } else if (snapshotType == camera::Abstract::RAW8  ||
             snapshotType == camera::Abstract::RAW10 || 
             snapshotType == camera::Abstract::RAW12) {
    fileName += ".raw";
  } else if (snapshotType == camera::Abstract::YUV) {
    fileName += ".yuv";
  }

  jResponse[KEY_FILE_NAME] = fileName;

  return *this;
}

Camera &Camera::inputInfo(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = RET_SUCCESS;

  jResponse[KEY_COUNT] = static_cast<int32_t>(pCamera->sensors.size());
  jResponse[KEY_INDEX] = pCalibration->module<clb::Inputs>().config.index;

  return *this;
}

Camera &Camera::inputSwitch(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto index = jQuery[KEY_INDEX].asInt();

  jResponse[REST_RET] = pCamera->inputSwitch(index);

  auto &type = pCalibration->module<clb::Inputs>().input().config.type;

  jResponse[KEY_INPUT_TYPE] = type;

  if (type == clb::Input::Type::Sensor) {
    jResponse[KEY_DRIVER_FILE] = pCalibration->module<clb::Sensors>()
                                     .sensors[index]
                                     .config.driverFileName;
  } else if (type == clb::Input::Type::Image) {
    jResponse[KEY_IMAGE] =
        pCalibration->module<clb::Images>().images[index].config.fileName;
  }

  return *this;
}

Camera &Camera::preview(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  auto type = pCalibration->module<clb::Inputs>().input().config.type;

  if (type == clb::Input::Sensor) {
    pCamera->sensor().checkValid();
  } else if (type == clb::Input::Image) {
    pCamera->image().checkValid();
  }

  if (jQuery[KEY_PREVIEW].asBool()) {
    ret = pCamera->previewStart();
  } else {
    ret = pCamera->previewStop();
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Camera &Camera::getPicBufLayout(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;
  uint32_t mode, layout;

  ret = pCamera->getPicBufLayout(mode, layout);

  jResponse[KEY_LAYOUT] = layout;
  jResponse[KEY_MODE] = mode;
  jResponse[REST_RET] = ret;

  return *this;
}

Camera &Camera::setPicBufLayout(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;
  uint32_t mode, layout;

  layout = jQuery[KEY_LAYOUT].asUInt();
  mode = jQuery[KEY_MODE].asUInt();

  ret = pCamera->setPicBufLayout(mode, layout);

  jResponse[REST_RET] = ret;

  return *this;
}
