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

#include <cam_engine/cam_engine_cproc_api.h>

namespace clb {

struct Cproc : Abstract {
  Cproc(XMLDocument &);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = true;

  struct Config {
    Config() {
      config.ChromaOut = CAMERIC_CPROC_CHROM_RANGE_OUT_BT601;
      config.LumaOut = CAMERIC_CPROC_LUM_RANGE_OUT_BT601;
      config.LumaIn = CAMERIC_CPROC_LUM_RANGE_IN_BT601;

      config.contrast = 1.1;
      config.brightness = -15;
      config.saturation = 1;
      config.hue = 0;
    }

    CamEngineCprocConfig_t config;
  } config;
};

} // namespace clb
