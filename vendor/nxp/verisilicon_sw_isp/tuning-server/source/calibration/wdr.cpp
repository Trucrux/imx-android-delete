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

#include "calibration/wdr.hpp"
#include "calibration/section-name.hpp"
#include "common/json-app.hpp"
#include "common/macros.hpp"

using namespace clb;

Wdr::Wdr(XMLDocument &document) : Abstract(document) {
  name = NAME_WDR;

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

void Wdr::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_V1, [&](XMLElement &subElement) {
    subElement.SetAttribute(KEY_ENABLE, holders[V1].isEnable);

    CamEngineWdrCurve_t &curve = holders[V1].config.v1.curve;

    Json::Value jDy;
    Json::Value jYm;

    for (int32_t i = 0; i < CURVE_ARRAY_SIZE; i++) {
      jDy.append(curve.dY[i]);
    }

    subElementSet(subElement, KEY_D_Y, jDy);

    for (int32_t i = 0; i < CURVE_ARRAY_SIZE; i++) {
      jYm.append(curve.Ym[i]);
    }

    subElementSet(subElement, KEY_Y_M, jYm);
  });

#if defined ISP_WDR2
  subElementProc(element, KEY_V2, [&](XMLElement &subElement) {
    subElement.SetAttribute(KEY_ENABLE, holders[V2].isEnable);

    auto &v2 = holders[V2].config.v2;

    subElementSet(subElement, KEY_STRENGTH, v2.strength);
  });
#endif

#if defined ISP_WDR_V3
  subElementProc(element, KEY_V3, [&](XMLElement &subElement) {
    subElement.SetAttribute(KEY_ENABLE, holders[V3].isEnable);

    auto &v3 = holders[V3].config.v3;

    subElement.SetAttribute(KEY_AUTO, v3.isAuto);
    subElementSet(subElement, KEY_AUTO_LEVEL, v3.autoLevel);

    subElementSet(subElement, KEY_GAIN_MAX, v3.gainMax);
    subElementSet(subElement, KEY_STRENGTH, v3.strength);
    subElementSet(subElement, KEY_STRENGTH_GLOBAL, v3.strengthGlobal);

    subElementSet(subElement, KEY_TABLE, holders[V3].table.jTable);
  });
#endif
}

void Wdr::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_V1, [&](XMLElement &subElement) {
    subElement.QueryBoolAttribute(KEY_ENABLE, &holders[V1].isEnable);

    Json::Value jCurve;

    CamEngineWdrCurve_t &curve = holders[V1].config.v1.curve;

    Json::Value jDy;

    subElementGet(subElement, KEY_D_Y, jDy);

    for (int32_t i = 0; i < CURVE_ARRAY_SIZE; i++) {
      curve.dY[i] = static_cast<uint8_t>(jDy[i].asUInt());
    }

    Json::Value jYm;

    subElementGet(subElement, KEY_Y_M, jYm);

    for (int32_t i = 0; i < CURVE_ARRAY_SIZE; i++) {
      curve.Ym[i] = static_cast<uint16_t>(jYm[i].asUInt());
    }

  });

#if defined ISP_WDR2
  subElementProc(element, KEY_V2, [&](XMLElement &subElement) {
    subElement.QueryBoolAttribute(KEY_ENABLE, &holders[V2].isEnable);

    subElementGet(subElement, KEY_STRENGTH, holders[V2].config.v2.strength);
  });
#endif

#if defined ISP_WDR_V3
  subElementProc(element, KEY_V3, [&](XMLElement &subElement) {
    subElement.QueryBoolAttribute(KEY_ENABLE, &holders[V3].isEnable);

    auto &v3 = holders[V3].config.v3;

    subElement.QueryBoolAttribute(KEY_AUTO, &v3.isAuto);
    subElementGet(subElement, KEY_AUTO_LEVEL, v3.autoLevel);

    subElementGet(subElement, KEY_GAIN_MAX, v3.gainMax);
    subElementGet(subElement, KEY_STRENGTH, v3.strength);
    subElementGet(subElement, KEY_STRENGTH_GLOBAL, v3.strengthGlobal);

    subElementGet(subElement, KEY_TABLE, holders[V3].table.jTable);
  });
#endif
}

void Wdr::Config::V1::reset() {
  const CamEngineWdrCurve_t curveX0 = {
      // .Ym =
      {0x0,   0x7c,  0xf8,  0x174, 0x1f0, 0x26c, 0x2e8, 0x364, 0x3e0,
       0x45d, 0x4d9, 0x555, 0x5d1, 0x64d, 0x6c9, 0x745, 0x7c1, 0x83e,
       0x8ba, 0x936, 0x9b2, 0xa2e, 0xaaa, 0xb26, 0xba2, 0xc1f, 0xc9b,
       0xd17, 0xd93, 0xe0f, 0xe8b, 0xf07, 0xf83},
      // .dY =
      {0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
       4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}};

  curve = curveX0;
}

void Wdr::Config::V2::reset() { strength = 0; }

void Wdr::Config::V3::reset() {
  isAuto = true;
  autoLevel = 0;

  gainMax = 16;
  strength = 100;
  strengthGlobal = 0;
}

void Wdr::Table::reset(Generation generation) {
  std::string data;

  if (generation == V1) {

  } else if (generation == V2) {

  } else if (generation == V3) {
    data = "{ \"columns\": [\"HDR\", \"Gain\", \"Integration Time\", "
           "\"Strength\", \"Max Gain\", \"Global Curve\"], "
           "\"rows\": [] }";
  }

  jTable = JsonApp::fromString(data);
}
