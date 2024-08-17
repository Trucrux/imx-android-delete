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

#include "calibration/ae.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Ae::Ae(XMLDocument &document) : Abstract(document) { name = NAME_AE; }

void Ae::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
  element.SetAttribute(KEY_BYPASS, config.isBypass);
}

void Ae::composeSubElements(XMLElement &element) {
  subElementSet(element, KEY_AFPS, ecm.isAfps);
  subElementSet(element, KEY_FLICKER_PERIOD, (int32_t)ecm.flickerPeriod);
  subElementSet(element, KEY_DAMPING_OVER, config.dampingOver);
  subElementSet(element, KEY_DAMPING_UNDER, config.dampingUnder);
  subElementSet(element, KEY_SET_POINT, config.setPoint);
  subElementSet(element, KEY_TOLERANCE, config.tolerance);

  Json::Value jWeight;
  for (int32_t i = 0; i < 25; i++) {
    jWeight.append(config.Weight[i]);
  }
  subElementSet(element, KEY_WEIGHT, jWeight);
}

void Ae::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
  element.QueryBoolAttribute(KEY_BYPASS, &config.isBypass);
}

void Ae::parseSubElements(XMLElement &element) {
  int32_t flickerPeriod = ecm.flickerPeriod;
  subElementGet(element, KEY_AFPS, ecm.isAfps);
  subElementGet(element, KEY_FLICKER_PERIOD, flickerPeriod);
  subElementGet(element, KEY_DAMPING_OVER, config.dampingOver);
  subElementGet(element, KEY_DAMPING_UNDER, config.dampingUnder);
  subElementGet(element, KEY_SET_POINT, config.setPoint);
  subElementGet(element, KEY_TOLERANCE, config.tolerance);

  Json::Value jWeight;

  subElementGet(element, KEY_WEIGHT, jWeight);

  for (int32_t i = 0; i < 25; i++) {
    config.Weight[i] = jWeight[i].asUInt();
  }

}
