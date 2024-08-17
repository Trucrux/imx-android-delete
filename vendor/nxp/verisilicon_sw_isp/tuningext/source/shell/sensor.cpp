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

#include "shell/sensor.hpp"
#include "common/filesystem.hpp"
#include "display/display-event.hpp"
#include <isi/isi.h>
#include <list>
#ifdef APPMODE_NATIVE
#include <isi/isi_iss.h>
#include <cam_device/cam_device_module_ids.h>
#include "calibration/inputs.hpp"
#include "calibration/sensors.hpp"
#include "camera/mapcaps.hpp"
#include "common/interface.hpp"
#endif

using namespace sh;

Sensor &Sensor::process(Json::Value &jQuery, Json::Value &jResponse) {
  Shell::process(jQuery, jResponse);

  auto id = jQuery[REST_ID].asInt();

  switch (id) {
  case CalibrationList:
    return calibraionList(jQuery, jResponse);

  case Caps:
    return caps(jQuery, jResponse);

  case ConfigGet:
    return configGet(jQuery, jResponse);

  case ConfigSet:
    return configSet(jQuery, jResponse);

  case DriverChange:
    return driverChange(jQuery, jResponse);

  case DriverList:
    return driverList(jQuery, jResponse);

  case Info:
    return info(jQuery, jResponse);

  case RegisterDescription:
    return registerDescription(jQuery, jResponse);

  case RegisterDumpToFile:
    return registerDump2File(jQuery, jResponse);

  case RegisterGet:
    return registerGet(jQuery, jResponse);

  case RegisterSet:
    return registerSet(jQuery, jResponse);

  case RegisterTable:
    return registerTable(jQuery, jResponse);

  case ResolutionSet:
    return resolutionSet(jQuery, jResponse);

  case ResolutionGet:
    return resolutionGet(jQuery, jResponse);

  case TestPatternEnableSet:
    return testPatternEnableSet(jQuery, jResponse);

  case ResolutionSupportGet:
    return resolutionSupportListGet(jQuery, jResponse);

  case FrameRateGet:
    return frameRateGet(jQuery, jResponse);

  case FrameRateSet:
    return frameRateSet(jQuery, jResponse);

  case ModeGet:
    return modeGet(jQuery, jResponse);

  case ModeSet:
    return modeSet(jQuery, jResponse);

  case StreamingIsRuning:
    return streamingIsRuning(jQuery, jResponse);

  default:
    throw exc::LogicError(RET_NOTAVAILABLE,
                          "Command implementation is not available");
  }

  return *this;
}

#ifdef APPMODE_V4L2
Sensor &Sensor::calibraionList(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  std::list<std::string> files;

  ret = fs::ls(".", "xml", files);
  if (ret) {
    jResponse[REST_RET] = ret;

    return *this;
  }

  for (auto fileName : files) {
    jResponse[KEY_FILE_LIST].append(fileName);
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::caps(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::driverChange(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::driverList(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;
  std::list<std::string> files;

  ret = fs::ls(".", "drv", files);
  if (ret == RET_SUCCESS) {
    for (auto fileName : files) {
      jResponse[KEY_FILE_LIST].append(fileName);
    }
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::info(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  viv_private_ioctl(IF_SENSOR_INFO, jQuery, jResponse);

  return *this;
}

Sensor &Sensor::resolutionSupportListGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::frameRateGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::frameRateSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::registerDescription(Json::Value &jQuery,
                                    Json::Value &jResponse) {
  TRACE_CMD;


  return *this;
}

Sensor &Sensor::registerDump2File(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::registerGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::registerSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::registerTable(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::resolutionSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::resolutionGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  jResponse[REST_RET] = viv_private_ioctl(IF_G_RESOLUTION, jQuery, jResponse);

  return *this;
}


Sensor &Sensor::testPatternEnableSet(Json::Value &jQuery,
                                     Json::Value &jResponse) {
  TRACE_CMD;

  if (pDisplayEvent->running ==false) {
    ALOGE("wrong state, need preview start first");
    jResponse[REST_RET] = RET_WRONG_STATE;
    return *this;
  }

  Json::Value jRequest;
  auto isEnable = jQuery[KEY_ENABLE].asBool();

  if (isEnable) {
    jRequest[KEY_TEST_PATTERN] = (uint32_t)ISI_TPG_MODE_0 +
                                 jQuery[KEY_CURPATTERN].asUInt();
  } else {
    jRequest[KEY_TEST_PATTERN] = (uint32_t)ISI_TPG_DISABLE;
  }

  jResponse[REST_RET] = viv_private_ioctl(IF_SENSOR_S_TESTPAT,
                        jRequest, jResponse);

  return *this;
}

Sensor &Sensor::modeGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;
  jResponse[KEY_CURSENSORMODE] = pDisplayEvent->sensorMode;
  jResponse[KEY_MAXSENSORMODE] = pDisplayEvent->sensorModeCount;
  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::modeSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  pDisplayEvent->sensorMode = jQuery[KEY_CURSENSORMODE].asUInt();
  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::streamingIsRuning(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  jResponse[KEY_STATE] = pDisplayEvent->running;
  jResponse[REST_RET] = ret;

  return *this;
}

#else
Sensor &Sensor::calibraionList(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  std::list<std::string> files;

  ret = fs::ls(".", "xml", files);
  if (ret) {
    jResponse[REST_RET] = ret;

    return *this;
  }

  for (auto fileName : files) {
    jResponse[KEY_FILE_LIST].append(fileName);
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::caps(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  IsiSensorCaps_t caps;
  REFSET(caps, 0);

  jResponse[REST_RET] = sensor().capsGet(caps);

  binEncode(caps, jResponse[KEY_CAPS]);

  return *this;
}

Sensor &Sensor::configGet(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  IsiSensorConfig_t sensorConfig;
  REFSET(sensorConfig, 0);

  jResponse[REST_RET] = sensor().configGet(sensorConfig);

  binEncode(sensorConfig, jResponse[KEY_CONFIG]);

  return *this;
}

Sensor &Sensor::configSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  IsiSensorConfig_t sensorConfig;
  REFSET(sensorConfig, 0);

  binDecode(jQuery[KEY_CONFIG], sensorConfig);

  jResponse[REST_RET] = sensor().configSet(sensorConfig);

  return *this;
}

Sensor &Sensor::driverChange(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  Object::State deviceStateBackup = pCamera->state;

  auto ret = RET_SUCCESS;

  if (deviceStateBackup >= Object::Running) {
    ret = pCamera->previewStop();
    if (ret != RET_SUCCESS) {
      jResponse[REST_RET] = ret;

      return *this;
    }
  }

  if (deviceStateBackup >= Object::Idle) {
    ret = pCamera->pipelineDisconnect();
    if (ret != RET_SUCCESS) {
      jResponse[REST_RET] = ret;

      return *this;
    }
  }

  ret = sensor().driverChange(jQuery[KEY_DRIVER_FILE].asString());

  if (ret != RET_SUCCESS) {
    jResponse[REST_RET] = ret;

    return *this;
  }

  pCalibration->module<clb::Inputs>().input().config.type = clb::Input::Sensor;

  ret = pCamera->pipelineConnect(true);
  if (ret != RET_SUCCESS) {
    jResponse[REST_RET] = ret;

    return *this;
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

Sensor &Sensor::driverList(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  auto ret = RET_SUCCESS;

  std::list<std::string> files;

  ret = fs::ls(".", "drv", files);
  if (ret) {
    jResponse[REST_RET] = ret;

    return *this;
  }

  for (auto fileName : files) {
    jResponse[KEY_FILE_LIST].append(fileName);
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::info(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  auto ret = RET_SUCCESS;

  std::list<camera::Sensor::Resolution> resolutionList;

  ret = sensor().resolutionDescriptionListGet(resolutionList);
  if (ret == RET_SUCCESS) {
    Json::Value jResolutionList = Json::Value(Json::arrayValue);
    for (camera::Sensor::Resolution resolution : resolutionList) {
      Json::Value jResolution;

      jResolution[KEY_VALUE] = resolution.value;
      jResolution[KEY_DESCRIPTION] = resolution.description;

      jResolutionList.append(jResolution);
    }

    jResponse[KEY_RESOLUTION_LIST] = jResolutionList;
  }

  jResponse[REST_RET] = ret;

  auto &config = pCalibration->module<clb::Sensors>().sensors[sensor().index].config;
  jResponse[KEY_DRIVER_FILE] = config.driverFileName;
  //jResponse[KEY_CALIB_FILE] = sensor().clbSensor.config.calibFileName;
  jResponse[KEY_NAME] = sensor().pSensor->pszName;
  jResponse[KEY_STATE] = sensor().stateDescription();

  uint32_t revId = 0;

  sensor().revisionGet(revId);
  jResponse[KEY_ID] = revId;

  jResponse[KEY_CONNECTION] = sensor().isConnected();

  jResponse[KEY_TEST_PATTERN] = sensor().isTestPattern();
  jResponse[KEY_MAXPATTERN] = sensor().maxTestPatternGet();

  jResponse[KEY_BAYER_PATTERN] =
      isiCapDescription<CamerIcIspBayerPattern_t>(sensor().SensorMode.bayer_pattern);
  jResponse[KEY_BUS_WIDTH] = isiCapDescription<CamerIcIspInputSelection_t>(sensor().SensorMode.bit_width);

  if (sensor().SensorMode.bit_width == 8){
    jResponse[KEY_MIPI_MODE] = isiCapDescription<MipiDataType_t>(ISI_MIPI_MODE_RAW_8);
    } else if (sensor().SensorMode.bit_width == 10) {
      jResponse[KEY_MIPI_MODE] = isiCapDescription<MipiDataType_t>(ISI_MIPI_MODE_RAW_10);
    } else {
      jResponse[KEY_MIPI_MODE] = isiCapDescription<MipiDataType_t>(ISI_MIPI_MODE_RAW_12);
    }

  return *this;
}

Sensor &Sensor::resolutionSupportListGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  int32_t ret = RET_SUCCESS;

  sensor().checkValid();

  std::list<camera::Sensor::Resolution> resolutionList;

  ret = sensor().resolutionSupportListGet(resolutionList);
  if (ret == RET_SUCCESS) {
    Json::Value jResolutionList = Json::Value(Json::arrayValue);
    for (camera::Sensor::Resolution resolution : resolutionList) {
      Json::Value jResolution;

      jResolution[KEY_VALUE] = resolution.value;
      jResolution[KEY_DESCRIPTION] = resolution.description;

      jResolutionList.append(jResolution);
    }

    jResponse[KEY_RESOLUTION_SUPPORT_LIST] = jResolutionList;
  }

  jResponse[REST_RET] = ret;

  return *this;
}

Sensor &Sensor::frameRateGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  uint32_t fps = 0;

  jResponse[REST_RET] = sensor().frameRateGet(fps);
  jResponse[KEY_FPS] = fps;

  return *this;
}

Sensor &Sensor::frameRateSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  uint32_t fps = jQuery[KEY_FPS].asUInt();

  jResponse[REST_RET] = sensor().frameRateSet(fps);

  return *this;
}

Sensor &Sensor::registerDescription(Json::Value &jQuery,
                                    Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  auto address = jQuery[KEY_ADDRESS].asUInt();

  IsiRegDescription_t regDescription;
  REFSET(regDescription, 0);

  auto ret = sensor().registerDescriptionGet(address, regDescription);
  jResponse[REST_RET] = ret;

  if (ret != RET_SUCCESS) {
    return *this;
  }

  Json::Value jDescription;

  jDescription[KEY_ADDRESS] = regDescription.Addr;
  jDescription[KEY_DEFAULT_VALUE] = regDescription.DefaultValue;
  jDescription[KEY_NAME] = regDescription.pName;
  jDescription[KEY_FLAGS] = regDescription.Flags;

  jResponse[KEY_DESCRIPTION] = jDescription;

  return *this;
}

Sensor &Sensor::registerDump2File(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  auto filename = jQuery[KEY_FILE_NAME].asString();
  jResponse[REST_RET] = sensor().registerDump2File(filename);

  return *this;
}

Sensor &Sensor::registerGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  auto address = jQuery[KEY_ADDRESS].asUInt();
  uint32_t value = 0;

  jResponse[REST_RET] = sensor().registerRead(address, value);

  jResponse[KEY_VALUE] = value;

  return *this;
}

Sensor &Sensor::registerSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  auto address = jQuery[KEY_ADDRESS].asUInt();
  auto value = jQuery[KEY_VALUE].asUInt();

  jResponse[REST_RET] = sensor().registerWrite(address, value);

  return *this;
}

Sensor &Sensor::registerTable(Json::Value &, Json::Value &jResponse) {
  TRACE_CMD;

  return *this;
}

Sensor &Sensor::resolutionSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  /*sensor().checkValid();

  auto oldState = pCamera->state;

  if (oldState == Object::Running) {
    pCamera->streamingStop();
  }

  uint16_t width = jQuery[KEY_IMAGE_WIDTH].asUInt();
  uint16_t height = jQuery[KEY_IMAGE_HEIGHT].asUInt();
  jResponse[REST_RET] = pCamera->resolutionSet(width,height);

  if (oldState == Object::Running) {
    pCamera->streamingStart();
  }*/

  return *this;
}

Sensor &Sensor::resolutionGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  sensor().checkValid();

  jResponse[KEY_IMAGE_WIDTH] = sensor().SensorMode.size.width;
  jResponse[KEY_IMAGE_HEIGHT] = sensor().SensorMode.size.height;

  return *this;
}


Sensor &Sensor::testPatternEnableSet(Json::Value &jQuery,
                                     Json::Value &jResponse) {
  TRACE_CMD;

  auto isEnable = jQuery[KEY_ENABLE].asBool();
  auto curPattern = jQuery[KEY_CURPATTERN].asUInt();
  jResponse[REST_RET] = sensor().checkValid().testPatternEnableSet(isEnable, curPattern);

  return *this;
}

Sensor &Sensor::modeGet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  uint32_t modeIndex=0, modeCount=3;

  jResponse[REST_RET] = sensor().checkValid().modeGet(modeIndex, modeCount);

  jResponse[KEY_CURSENSORMODE] = modeIndex;
  jResponse[KEY_MAXSENSORMODE] = modeCount;

  return *this;
}

Sensor &Sensor::modeSet(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  auto modeIndex = jQuery[KEY_CURSENSORMODE].asUInt();

  jResponse[REST_RET] = sensor().modeSet(modeIndex);

  return *this;
}

Sensor &Sensor::streamingIsRuning(Json::Value &jQuery, Json::Value &jResponse) {
  TRACE_CMD;

  if (pCamera->state == Object::Running) {
    jResponse[KEY_STATE] = true;
  } else {
    jResponse[KEY_STATE] = false;
  }

  return *this;
}

#endif /* APPMODE_V4L2 */
