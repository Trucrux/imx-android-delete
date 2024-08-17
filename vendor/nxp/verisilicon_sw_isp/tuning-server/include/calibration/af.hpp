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
#include <cam_engine/cam_engine_api.h>

#include <cam_engine/cam_engine_aaa_api.h>

namespace clb {

struct Af : Abstract {
  Af(XMLDocument &);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = true;
  struct MeasureWindow {
        uint16_t startX;
        uint16_t startY;
        uint16_t width;
        uint16_t height;
  };

  struct Config {
    bool isOneshot = false;
    uint32_t pos = 0;
    CamEngineAfSearchAlgorithm_t searchAlgorithm =
        CAM_ENGINE_AUTOFOCUS_SEARCH_ALGORITHM_ADAPTIVE_RANGE;
    CamEngineAfWorkMode_t mode = CAM_ENGINE_AF_MODE_AUTO;
    struct MeasureWindow win;
  } config;

};

} // namespace clb
