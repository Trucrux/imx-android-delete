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

#include "calibration/avs.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Avs::Avs(XMLDocument &document) : Abstract(document) {
  name = NAME_AVS;

  config.reset();
}

void Avs::composeAttributes(XMLElement &element) {
#if defined ISP_AVS
  element.SetAttribute(KEY_ENABLE, isEnable);
  element.SetAttribute(KEY_USE_PARAMS, config.isUseParams);
#endif
}

void Avs::composeSubElements(XMLElement &element) {
#if defined ISP_AVS
  subElementSet(element, KEY_DAMP_NUMBER_OF_INTERPOLATION_POINTS,
                config.numItpPoints);

  subElementSet(element, KEY_DAMP_ACCELERATION, config.acceleration);
  subElementSet(element, KEY_DAMP_BASE_GAIN, config.baseGain);
  subElementSet(element, KEY_DAMP_FALL_OFF, config.fallOff);
  subElementSet(element, KEY_DAMP_THETA, config.theta);
#endif
}

void Avs::parseAttributes(XMLElement &element) {
#if defined ISP_AVS
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
  element.QueryBoolAttribute(KEY_USE_PARAMS, &config.isUseParams);
#endif
}

void Avs::parseSubElements(XMLElement &element) {
#if defined ISP_AVS
  subElementGet(element, KEY_DAMP_NUMBER_OF_INTERPOLATION_POINTS,
                config.numItpPoints);
  subElementGet(element, KEY_DAMP_ACCELERATION, config.acceleration);
  subElementGet(element, KEY_DAMP_BASE_GAIN, config.baseGain);
  subElementGet(element, KEY_DAMP_FALL_OFF, config.fallOff);
  subElementGet(element, KEY_DAMP_THETA, config.theta);
#endif
}
