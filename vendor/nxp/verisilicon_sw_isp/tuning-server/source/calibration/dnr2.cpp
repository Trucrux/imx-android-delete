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

#include "calibration/dnr2.hpp"
#include "calibration/section-name.hpp"
#include "common/json-app.hpp"

using namespace clb;

Dnr2::Dnr2(XMLDocument &document) : Abstract(document) {
  name = NAME_DNR2;

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

void Dnr2::composeSubElements(XMLElement &element) {
#if defined ISP_2DNR
  subElementProc(element, KEY_V1, [&](XMLElement &subElement) {
    subElement.SetAttribute(KEY_ENABLE, holders[V1].isEnable);

    auto &v1 = holders[V1].config.v1;

    subElement.SetAttribute(KEY_AUTO, v1.isAuto);
    subElementSet(subElement, KEY_AUTO_LEVEL, v1.autoLevel);

    subElementSet(subElement, KEY_DENOISE_PREGAMA_STRENGTH,
                  v1.denoisePregamaStrength);
    subElementSet(subElement, KEY_DENOISE_STRENGTH, v1.denoiseStrength);
    subElementSet(subElement, KEY_SIGMA, v1.sigma);

    subElementSet(subElement, KEY_TABLE, holders[V1].table.jTable);
  });
#endif
}

void Dnr2::parseSubElements(XMLElement &element) {
#if defined ISP_2DNR
  subElementProc(element, KEY_V1, [&](XMLElement &subElement) {
    subElement.QueryBoolAttribute(KEY_ENABLE, &holders[V1].isEnable);

    auto &v1 = holders[V1].config.v1;

    subElement.QueryBoolAttribute(KEY_AUTO, &v1.isAuto);
    subElementGet(subElement, KEY_AUTO_LEVEL, v1.autoLevel);

    subElementGet(subElement, KEY_DENOISE_PREGAMA_STRENGTH,
                  v1.denoisePregamaStrength);
    subElementGet(subElement, KEY_DENOISE_STRENGTH, v1.denoiseStrength);
    subElementGet(subElement, KEY_SIGMA, v1.sigma);

    subElementGet(subElement, KEY_TABLE, holders[V1].table.jTable);
  });
#endif
}

void Dnr2::Config::V1::reset() {
  isAuto = true;
  autoLevel = 0;

  denoisePregamaStrength = 1;
  denoiseStrength = 80;
  sigma = 2.0;
}

void Dnr2::Config::V2::reset() {}

void Dnr2::Config::V3::reset() {}

void Dnr2::Table::reset(Generation generation) {
  std::string data;

  if (generation == V1) {
    data = "{ \"columns\": [ \"HDR\", \"Gain\", \"Integration Time\", "
           "\"Pre-gamma Strength\", \"Denoise Strength\", \"Sigma\"], "
           "\"rows\": [] }";
  } else if (generation == V2) {

  } else if (generation == V3) {
  }

  jTable = JsonApp::fromString(data);
}
