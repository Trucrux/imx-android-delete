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

#include "cam_engine/cam_engine_isp_api.h"

namespace clb {

struct Ee : Abstract {
  Ee(XMLDocument &document);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = true;

  struct Config {
    bool isAuto = true;

    CamEngineEeConfig_t config;

    void reset();
  } config;

  struct Status {
    double gain;
    double integrationTime;
  };

  struct Table {
    enum Column {
      Hdr,
      Gain,
      IntegrationTime,
      EdgeGain,
      Strength,
      UvGain,
      YGainDown,
      YGainUp,
    };

    void reset();

    Json::Value jTable;
  } table;
};

} // namespace clb
