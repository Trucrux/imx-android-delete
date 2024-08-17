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

#include "abstract.hpp"

namespace clb {

#define CLB_HDR_SENSOR_CURVE_POINT 64

struct Hdr : Abstract {
  Hdr(XMLDocument &document);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = false;

  struct Config {
    uint8_t exposureRatio = 16;
    uint8_t extensionBit = 3;
  } config;

  struct Sensor {
    float curve[CLB_HDR_SENSOR_CURVE_POINT * 2];
  } sensor;
};

} // namespace clb
