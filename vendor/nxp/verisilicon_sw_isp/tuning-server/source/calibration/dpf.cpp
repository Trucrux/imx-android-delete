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

#include "calibration/dpf.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Dpf::Dpf(XMLDocument &document) : Abstract(document) { name = NAME_DPF; }

void Dpf::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Dpf::composeSubElements(XMLElement &element) {
  element.SetAttribute(KEY_ADAPTIVE, config.isAdaptive);

  subElementSet(element, KEY_DIVISION_FACTOR, config.divisionFactor);
  subElementSet(element, KEY_GRADIENT, config.gradient);
  subElementSet(element, KEY_MINIMUM_BOUND, config.minimumBound);
  subElementSet(element, KEY_OFFSET, config.offset);

  subElementSet(element, KEY_SIGMA_GREEN, config.sigmaGreen);
  subElementSet(element, KEY_SIGMA_RED_BLUE, config.sigmaRedBlue);
}

void Dpf::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Dpf::parseSubElements(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ADAPTIVE, &config.isAdaptive);

  subElementGet(element, KEY_DIVISION_FACTOR, config.divisionFactor);
  subElementGet(element, KEY_GRADIENT, config.gradient);
  subElementGet(element, KEY_MINIMUM_BOUND, config.minimumBound);
  subElementGet(element, KEY_OFFSET, config.offset);

  subElementGet(element, KEY_SIGMA_GREEN, config.sigmaGreen);
  subElementGet(element, KEY_SIGMA_RED_BLUE, config.sigmaRedBlue);
}
