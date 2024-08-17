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

#include "calibration/cnr.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Cnr::Cnr(XMLDocument &document) : Abstract(document) { name = NAME_CNR; }

void Cnr::composeAttributes(XMLElement &element) {
#if 1
  element.SetAttribute(KEY_ENABLE, isEnable);
#endif
}

void Cnr::composeSubElements(XMLElement &element) {
#if 1
  subElementSet(element, KEY_THRESHOLD_CHANNEL ".1", config.tc1);
  subElementSet(element, KEY_THRESHOLD_CHANNEL ".2", config.tc2);
#endif
}

void Cnr::parseAttributes(XMLElement &element) {
#if 1
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
#endif
}

void Cnr::parseSubElements(XMLElement &element) {
#if 1
  subElementGet(element, KEY_THRESHOLD_CHANNEL ".1", config.tc1);
  subElementGet(element, KEY_THRESHOLD_CHANNEL ".2", config.tc2);
#endif
}
