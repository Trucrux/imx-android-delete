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

#include "calibration/dnr3.hpp"
#include "calibration/section-name.hpp"
#include "common/json-app.hpp"

using namespace clb;

Dnr3::Dnr3(XMLDocument &document) : Abstract(document) {
  name = NAME_DNR3;

  for (int32_t i = 0; i < VMax; i++) {
    holders.emplace_back();
  }

  holders[V1].config.v1.reset();
  holders[V1].table.reset(V1);

  holders[V2].config.v2.reset();
  holders[V2].table.reset(V2);

  holders[V3].config.v3.reset();
  holders[V3].table.reset(V3);
}

void Dnr3::composeSubElements(XMLElement &element) {
#if defined ISP_3DNR
  subElementProc(element, KEY_V1, [&](XMLElement &subElement) {
    subElement.SetAttribute(KEY_ENABLE, holders[V1].isEnable);

    auto &v1 = holders[V1].config.v1;

    subElement.SetAttribute(KEY_AUTO, v1.isAuto);
    subElementSet(subElement, KEY_AUTO_LEVEL, v1.autoLevel);

    subElementSet(subElement, KEY_DELTA_FACTOR, v1.deltaFactor);
    subElementSet(subElement, KEY_MOTION_FACTOR, v1.motionFactor);
    subElementSet(subElement, KEY_STRENGTH, v1.strength);

    subElementSet(subElement, KEY_TABLE, holders[V1].table.jTable);
  });
#endif

  (void)element;
}

void Dnr3::parseSubElements(XMLElement &element) {
#if defined ISP_3DNR
  subElementProc(element, KEY_V1, [&](XMLElement &subElement) {
    subElement.QueryBoolAttribute(KEY_ENABLE, &holders[V1].isEnable);

    auto &v1 = holders[V1].config.v1;

    subElement.QueryBoolAttribute(KEY_AUTO, &v1.isAuto);
    subElementGet(subElement, KEY_AUTO_LEVEL, v1.autoLevel);

    subElementGet(subElement, KEY_DELTA_FACTOR, v1.deltaFactor);
    subElementGet(subElement, KEY_MOTION_FACTOR, v1.motionFactor);
    subElementGet(subElement, KEY_STRENGTH, v1.strength);

    subElementGet(subElement, KEY_TABLE, holders[V1].table.jTable);
  });
#endif

  (void)element;
}

void Dnr3::Config::V1::reset() {
  isAuto = true;
  autoLevel = 0;

  deltaFactor = 32;
  motionFactor = 1024;
  strength = 100;
}

void Dnr3::Config::V2::reset() {}

void Dnr3::Config::V3::reset() {}

void Dnr3::Table::reset(Generation generation) {
  std::string data;

  if (generation == V1) {
    data = "{ \"columns\": [\"HDR\", \"Gain\", \"Integration Time\", "
           "\"Strength\", \"Motion Factor\", "
           "\"Delta Factor\"], "
           "\"rows\": [] }";
  } else if (generation == V2) {

  } else if (generation == V3) {
  }

  jTable = JsonApp::fromString(data);
}
