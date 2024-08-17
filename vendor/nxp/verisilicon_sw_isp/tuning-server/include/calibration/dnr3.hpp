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
#include <algorithm>

namespace clb {

struct Dnr3 : Abstract {
  Dnr3(XMLDocument &);

  void composeSubElements(XMLElement &) override;

  void parseSubElements(XMLElement &) override;

  enum Generation { V1, V2, V3, VMax };

  union Config {
    struct V1 {
      void reset();

      bool isAuto;
      int32_t autoLevel;

      int32_t deltaFactor;
      int32_t motionFactor;
      int32_t strength;
    } v1;

    struct V2 {
      void reset();

      int32_t dummy;
    } v2;

    struct V3 {
      void reset();

      int32_t dummy;
    } v3;
  };

  struct Status {
    double gain;
    double integrationTime;
  };

  struct Table {
    enum ColumnV1 {
      Hdr,
      Gain,
      IntegrationTime,
      Strength,
      MotionFactor,
      DeltaFactor,
    };

    void reset(Generation Generation);

    Json::Value jTable;
  };

  struct Holder {
    bool isEnable = false;

    Config config;
    Table table;
  };

  std::vector<Holder> holders;
};

} // namespace clb
