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

struct Filter : Abstract {
  Filter(XMLDocument &);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = true;

  struct Config {
    void reset();

    bool isAuto;

    int32_t denoise;
    int32_t sharpen;
    int32_t chrV;
    int32_t chrH;
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
      Denoising,
      Sharpening,
    };

    void reset();

    Json::Value jTable;
  } table;
};

} // namespace clb
