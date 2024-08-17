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
#include <vector>

#include <cam_engine/cam_engine_aaa_api.h>
#include <cam_engine/cam_engine_cproc_api.h>
#include <cam_engine/cam_engine_imgeffects_api.h>
#include <cam_engine/cam_engine_isp_api.h>
#include <cam_engine/cam_engine_jpe_api.h>
#include <cam_engine/cam_engine_mi_api.h>
#include <cam_engine/cam_engine_simp_api.h>

namespace clb {

struct Ae : Abstract {
  Ae(XMLDocument &);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = true;

  struct Config {
    bool isBypass = true;
    CamEngineAecSemMode_t mode = CAM_ENGINE_AEC_SCENE_EVALUATION_DISABLED;
    float dampingOver = 0;
    float dampingUnder = 0;
    float setPoint = 0;
    float tolerance = 0;
    uint8_t Weight[CAM_ENGINE_AEC_EXP_GRID_ITEMS] = {
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
    };
  } config;

  struct Ecm {
    CamEngineFlickerPeriod_t flickerPeriod = CAM_ENGINE_FLICKER_100HZ;

    bool isAfps = false;
  } ecm;

  struct Status {
    CamEngineAecHistBins_t histogram;
    CamEngineAecMeanLuma_t luminance;
    CamEngineAecMeanLuma_t objectRegion;
  } status;
};

} // namespace clb
