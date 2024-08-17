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
#include "common/macros.hpp"
#include <cam_engine/cam_engine_api.h>

#include <cam_engine/cam_engine_isp_api.h>

namespace clb {

struct Wb : Abstract {
  Wb(XMLDocument &document);

  void composeSubElements(XMLElement &) override;

  void parseSubElements(XMLElement &) override;

  struct Config {
    Config() {
      int32_t i = 0;

      ccMatrix.Coeff[i++] = 1.805;
      ccMatrix.Coeff[i++] = -0.539;
      ccMatrix.Coeff[i++] = -0.250;
      ccMatrix.Coeff[i++] = -0.477;
      ccMatrix.Coeff[i++] = 1.789;
      ccMatrix.Coeff[i++] = -0.234;
      ccMatrix.Coeff[i++] = 0.016;
      ccMatrix.Coeff[i++] = -0.633;
      ccMatrix.Coeff[i++] = 1.734;

      REFSET(ccOffset, 0);

      wbGains.Red = 1.887;
      wbGains.GreenR = 1.016;
      wbGains.GreenB = 1.016;
      wbGains.Blue = 2.199;
    }

    CamEngineCcMatrix_t ccMatrix;
    CamEngineCcOffset_t ccOffset;
    CamEngineWbGains_t wbGains;
  } config;
};

} // namespace clb
