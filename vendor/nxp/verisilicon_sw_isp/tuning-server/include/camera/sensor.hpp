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

#include "calibdb/calibdb.hpp"
#include "calibration/calibration.hpp"
#include "calibration/hdr.hpp"
#include "calibration/sensors.hpp"
#include "common/interface.hpp"
#include <algorithm>
#include <iostream>
#include <isi/isi_iss.h>
#include <list>
#include <string>
#include <vector>

namespace camera {

struct Sensor : Object {
  Sensor(clb::Sensor &, int32_t);
  ~Sensor();

  struct Resolution {
    uint32_t  value;
    std::string description;
  };

  struct cam_compand_curve{
        bool     enable; 
        uint8_t  in_bit;
        uint8_t  out_bit;
        uint8_t  px[64];
        uint32_t x_data[65];
        uint32_t y_data[65]; 
    };

  int32_t capsGet(IsiSensorCaps_t &);
  int32_t configGet(IsiSensorConfig_t &);
  int32_t configSet(IsiSensorConfig_t &);

  Sensor &checkValid();

  int32_t close();

  int32_t driverChange(std::string driverFileName);

  int32_t ecConfigGet(clb::Sensor::Config::Ec &);
  int32_t ecConfigSet(clb::Sensor::Config::Ec);
  int32_t ecStatusGet(clb::Sensor::Status::Ec &);

  int32_t focusGet(uint32_t &);
  int32_t focusSet(uint32_t);

  int32_t illuminationProfilesGet(std::vector<CamIlluProfile_t *> &);

  bool isConnected();
  bool isTestPattern();
  int32_t maxTestPatternGet();

  int32_t nameGet(std::string &);

  //int32_t open(std::string);
  int32_t open();

  int32_t registerDescriptionGet(uint32_t, IsiRegDescription_t &);
  int32_t registerDump2File(std::string &);
  int32_t registerRead(uint32_t, uint32_t &);
  int32_t registerTableGet(IsiRegDescription_t *pRegisterTable);
  int32_t registerWrite(uint32_t, uint32_t);

  int32_t reset();

  int32_t resolutionDescriptionListGet(std::list<Resolution> &);
  int32_t resolutionSet(uint16_t, uint16_t);
  int32_t resolutionSupportListGet(std::list<Resolution> &);

  int32_t frameRateGet(uint32_t &);
  int32_t frameRateSet(uint32_t fps);

  int32_t modeGet(uint32_t &, uint32_t &);
  int32_t modeSet(uint32_t);

  int32_t revisionGet(uint32_t &);

  int32_t setup();

  int32_t streamEnableSet(bool);

  int32_t testPatternEnableSet(bool, int32_t);

  //CalibDb calibDb;


  IsiSensorConfig_t config;
  IsiSensorMode_t SensorMode;
  IsiSensorHandle_t hSensor = 0;
  int32_t index = 0;

  IsiCamDrvConfig_t *pCamDrvConfig = nullptr;
  void *pLib = nullptr;
  const IsiRegDescription_t *pRegisterTable = nullptr;
  const IsiSensor_t *pSensor = nullptr;

  clb::Sensor &calibrationSensor;

  int64_t csiFormat;
  uint32_t SensorHdrMode;
  uint32_t SensorHdrStichMode;
  struct cam_compand_curve expand_curve;
  struct cam_compand_curve compress_curve;
  //clb::Sensor &clbSensor;
};

} // namespace camera
