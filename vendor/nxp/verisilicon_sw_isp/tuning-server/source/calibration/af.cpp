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

#include "calibration/af.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Af::Af(XMLDocument &document) : Abstract(document) { name = NAME_AF; }

void Af::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Af::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_ALGORITHM, [&](XMLElement &subElement) {
    subElement.SetText(config.searchAlgorithm);
    subElement.SetAttribute(KEY_ONESHOT, config.isOneshot);
    subElementSet(element, KEY_POS, config.pos);
  });
  subElementProc(element, KEY_MODE, [&](XMLElement &subElement) {
    subElement.SetText(config.mode);
  });
}

void Af::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Af::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_ALGORITHM, [&](XMLElement &subElement) {
    subElement.QueryIntText((int32_t *)(&config.searchAlgorithm));
    subElement.QueryBoolAttribute(KEY_ONESHOT, &config.isOneshot);
    subElementGet(element, KEY_POS, config.pos);
  });
  subElementProc(element, KEY_MODE, [&](XMLElement &subElement) {
    subElement.QueryIntText((int32_t *)(&config.mode));
  });
}
