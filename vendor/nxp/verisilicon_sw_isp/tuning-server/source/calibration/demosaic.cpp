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

#include "calibration/demosaic.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Demosaic::Demosaic(XMLDocument &document) : Abstract(document) {
  name = NAME_DMS;
}

void Demosaic::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Demosaic::composeSubElements(XMLElement &element) {
  subElementSet(element, KEY_THRESHOLD, config.threshold);
}

void Demosaic::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Demosaic::parseSubElements(XMLElement &element) {
  subElementGet(element, KEY_THRESHOLD, config.threshold);
}
