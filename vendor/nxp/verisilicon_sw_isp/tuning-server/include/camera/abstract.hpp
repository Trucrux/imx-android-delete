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

#include "calibration/inputs.hpp"
#include "common/interface.hpp"
#include "engine.hpp"
#include "image.hpp"
#include "sensor.hpp"
#include <vector>

namespace camera {

typedef void(AfpsResChangeCb_t)(const void *);

struct Abstract : Object {
  Abstract(AfpsResChangeCb_t *, const void *);
  ~Abstract();

  enum SnapshotType { RAW8, RAW10, RAW12, RAW16, RGB, YUV, JPEG };

  static int32_t afpsResChangeRequestCb(uint16_t, uint16_t, const void *);

  uint bitstreamId() const;

  int32_t bufferMap(const MediaBuffer_t *, PicBufMetaData_t *);
  int32_t bufferUnmap(PicBufMetaData_t *);

  uint camerIcId() const;

  int32_t dehazeEnableGet(bool &);
  int32_t dehazeEnableSet(bool);

  int32_t ecmSet(bool = false);

  Image &image() {
    return *images[pCalibration->module<clb::Inputs>().config.index];
  }

  int32_t inputConnect();
  int32_t inputDisconnect();
  int32_t inputSwitch(int32_t);

  int32_t pipelineEnableSet(bool);

  int32_t reset();

  int32_t resolutionGet(uint16_t &, uint16_t &);
  int32_t resolutionSet(uint16_t,uint16_t);

  Sensor &sensor() {
    return *sensors[pCalibration->module<clb::Inputs>().config.index];
  };

  std::string softwareVersion() const;

  int32_t streamingStart(uint frames = 0);
  int32_t streamingStop();

  std::vector<Image *> images;
  std::vector<Sensor *> sensors;

  AfpsResChangeCb_t *pAfpsResChangeCb = nullptr;

  Engine *pEngine = nullptr;

  const void *pUserCbCtx = nullptr;
};

} // namespace camera
