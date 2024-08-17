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
#include "display/display-event.hpp"
#include <cameric_drv/cameric_drv_api.h>
#include <cameric_drv/cameric_mi_drv_api.h>
#include "calib_features.hpp"
#ifdef APPMODE_NATIVE
#include "calibration/images.hpp"
#include "calibration/inputs.hpp"
#endif

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

#ifdef APPMODE_V4L2
Camera &Camera::calibrationGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_CALIBRATION_GET, jQuery, jResponse);

  return *this;
}

Camera &Camera::calibrationSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  pDisplayEvent->jLoadXml = jQuery;
  jResponse[REST_RET] = RET_SUCCESS;

  return *this;
}

Camera &Camera::captureDma(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

int32_t Camera::captureSensorRAW(std::string fileName, SnapshotType snapshotType) {
  TRACE_CMD;

  int32_t ret = RET_SUCCESS;

  if (pDisplayEvent->running == false) {
    ALOGE("wrong state, need preview start first");
    ret = RET_WRONG_STATE;
    return ret;
  }

  uint32_t curMiMode = pDisplayEvent->miMode;
  Json::Value jQuery, jResponse;

  // 1. get curConfig
  ret = viv_private_ioctl(IF_MODULE_DATA_G, jQuery, jResponse);
  ret |= jResponse[REST_RET].asInt();
  if (ret) {
      ALOGE("get module data failed");
      return ret;
  }

  pDisplayEvent->jModuledata = jResponse;

  jQuery[KEY_PREVIEW] = false;
  preview(jQuery, jResponse);

  // 2. update raw fmt params for display-event
  if (snapshotType == Camera::RAW10) {
    pDisplayEvent->miMode = CAMERIC_MI_DATAMODE_RAW10;
  } else if (snapshotType == Camera::RAW12) {
    pDisplayEvent->miMode = CAMERIC_MI_DATAMODE_RAW12;
  } else {
    ALOGE("unsupport raw type %d", snapshotType);
    ret = RET_NOTSUPP;
    return ret;
  }
  pDisplayEvent->filePre    = fileName;
  pDisplayEvent->ispCfg     = true;
  pDisplayEvent->captureRAW = true;
  // usleep(100000);

  // 3. start raw preview/capture
  jQuery[KEY_PREVIEW] = true;
  preview(jQuery, jResponse);
  if (osEventWait(&pDisplayEvent->captureDone) != OSLAYER_OK) {
    ALOGE("wait capture RAW done failed");
    ret = RET_FAILURE;
  }

  // 4. restore YUV preview
  jQuery[KEY_PREVIEW] = false;
  preview(jQuery, jResponse);
  pDisplayEvent->miMode  = curMiMode;
  pDisplayEvent->ispCfg  = true;
  // usleep(100000);
  jQuery[KEY_PREVIEW] = true;
  preview(jQuery, jResponse);
  ret = jResponse[REST_RET].asInt();

  return ret;
}

Camera &Camera::captureSensor(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  if (pDisplayEvent->running == false) {
    ALOGE("wrong state, need preview start first");
    jResponse[REST_RET] = RET_WRONG_STATE;
    return *this;
  }

  auto fileName     = jQuery[KEY_FILE_NAME].asString();
  auto snapshotType = (Camera::SnapshotType)jQuery[KEY_SNAPSHOT_TYPE].asInt();

  if (snapshotType == Camera::YUV) {
    viv_private_ioctl(IF_CAPTURE, jQuery, jResponse);
    fileName += ".yuv";
  } else if (snapshotType == Camera::RAW10 ||
             snapshotType == Camera::RAW12) {
    jResponse[REST_RET] = captureSensorRAW(fileName, snapshotType);
    fileName += ".raw";
  } else {
    jResponse[REST_RET] = RET_NOTSUPP;
    return *this;
  }

  jResponse[KEY_FILE_NAME] = fileName;
  // remove unnecessary attribute generated by nativeSensor jsonRequest()
  if (jResponse.isMember("CaptureImage")) {
    jResponse.removeMember("CaptureImage");
  }

  return *this;
}

Camera &Camera::inputInfo(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = RET_SUCCESS;
  jResponse[KEY_COUNT] = CAM_ISPCORE_ID_MAX;
  switch (pDisplayEvent->videoId) {
  case 2:
    jResponse[KEY_INDEX] = 0;
    break;
  case 3:
    jResponse[KEY_INDEX] = 1;
    break;
  default:
    jResponse[REST_RET] = RET_FAILURE;
    break;
  }

  return *this;
}

Camera &Camera::inputSwitch(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = RET_SUCCESS;
  if (!pDisplayEvent->running) {
    int sensorId = jQuery[KEY_INDEX].asInt();
    switch (sensorId) {
    case 0:
      pDisplayEvent->videoId = 2;
      break;
    case 1:
      pDisplayEvent->videoId = 3;
      break;
    default:
      ALOGE("unsupported sensor id %d", sensorId);
      jResponse[REST_RET] = RET_FAILURE;
      break;
    }
  }

  return *this;
}

Camera &Camera::preview(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  pDisplayEvent->running = jQuery[KEY_PREVIEW].asBool();
  if (osEventWait(&pDisplayEvent->previewDone) != OSLAYER_OK) {
    ALOGE("wait preview event control done failed");
    ret = RET_FAILURE;
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Camera &Camera::getPicBufLayout(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  jResponse[KEY_LAYOUT] = pDisplayEvent->layout;
  jResponse[KEY_MODE]   = pDisplayEvent->miMode;
  jResponse[REST_RET]   = ret;

  return *this;
}

Camera &Camera::setPicBufLayout(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  pDisplayEvent->layout = jQuery[KEY_LAYOUT].asUInt();
  pDisplayEvent->miMode = jQuery[KEY_MODE].asUInt();
  jResponse[REST_RET] = ret;

  return *this;
}

#else
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
#endif /* APPMODE_V4L2 */
