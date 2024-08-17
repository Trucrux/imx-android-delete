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

#include "calibration/awb.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Awb::Awb(XMLDocument &document) : Abstract(document) { name = NAME_AWB; }

void Awb::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Awb::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_DAMPING, [&](XMLElement &subElement) {
    subElement.SetText(config.isDamping);
  });

  subElementProc(element, KEY_INDEX, [&](XMLElement &subElement) {
    subElement.SetText(config.index);
  });

  subElementProc(element, KEY_MODE, [&](XMLElement &subElement) {
    subElement.SetText(config.mode);
  });
}

void Awb::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Awb::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_DAMPING, [&](XMLElement &subElement) {
    subElement.QueryBoolText(&config.isDamping);
  });

  subElementProc(element, KEY_INDEX, [&](XMLElement &subElement) {
    subElement.QueryUnsignedText(&config.index);
  });

  subElementProc(element, KEY_MODE, [&](XMLElement &subElement) {
    subElement.QueryIntText((int32_t *)(&config.mode));
  });
}
