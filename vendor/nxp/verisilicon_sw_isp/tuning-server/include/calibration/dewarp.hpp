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

#if defined CTRL_DEWARP
#include "DW200Driver.h"
#endif
#include "abstract.hpp"
#include "common/macros.hpp"

#define DEWARP_CORE_MAX 2

namespace clb {

struct Dewarp : Abstract {
  Dewarp(XMLDocument &);
  ~Dewarp();

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = false;

  struct Config {
#if defined CTRL_DEWARP
    Config() {
      REFSET(params, 0);

      params.input_res[0].enable = true;
      params.input_res[0].format = MEDIA_PIX_FMT_YUV422SP;
      params.input_res[0].width = 1920;
      params.input_res[0].height = 1080;
      params.output_res[0].enable = true;
      params.output_res[0].format = MEDIA_PIX_FMT_YUV422SP;
      params.output_res[0].width = 1920;
      params.output_res[0].height = 1080;

      params.scale_factor = 4096;
      params.boundary_pixel.Y = 0;
      params.boundary_pixel.U = 128;
      params.boundary_pixel.V = 128;
      params.split_horizon_line = 716;
      params.split_vertical_line_up = 716;
      params.split_vertical_line_down = 716;
      params.dewarp_type = DEWARP_MODEL_FISHEYE_DEWARP;

      params.hflip = false;
      params.vflip = false;
      params.bypass = false;

      distortionMap[0] = {
          0,
          0,
          {6.5516074404594690e+002, 0.0, 9.6420599053623062e+002, 0.0,
           6.5552406676868952e+002, 5.3203601317192908e+002, 0.0, 0.0, 1.0},
          {1.0, 0, 0, 0, 1, 0, 0, 0, 1},
          {-2.2095698671518085e-002, 3.8543889520066955e-003,
           -5.9060355970132873e-003, 1.9007362178503509e-003, 0.0, 0.0, 0.0,
           0.0},
          nullptr};

      distortionMap[1] = {
          1,
          0,
          {6.5516074404594690e+002, 0.0, 9.6420599053623062e+002, 0.0,
           6.5552406676868952e+002, 5.3203601317192908e+002, 0.0, 0.0, 1.0},
          {1.0, 0, 0, 0, 1, 0, 0, 0, 1},
          {-2.2095698671518085e-002, 3.8543889520066955e-003,
           -5.9060355970132873e-003, 1.9007362178503509e-003, 0.0, 0.0, 0.0,
           0.0},
          nullptr};
    }

    Config &operator=(const Config &);

    void freeUserMap();

    dw200_parameters params;
    dewarp_distortion_map distortionMap[DEWARP_CORE_MAX];
#endif
  } config;
};

} // namespace clb
