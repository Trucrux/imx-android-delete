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

#include "calibration/bls.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Bls::Bls(XMLDocument &document) : Abstract(document) { name = NAME_BLS; }

void Bls::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_BYPASS, config.isBypass);
}

void Bls::composeSubElements(XMLElement &element) {
  subElementSet(element, KEY_RED, config.red);
  subElementSet(element, KEY_GREEN_B, config.greenB);
  subElementSet(element, KEY_GREEN_R, config.greenR);
  subElementSet(element, KEY_BLUE, config.blue);
}

void Bls::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_BYPASS, &config.isBypass);
}

void Bls::parseSubElements(XMLElement &element) {
  subElementGet(element, KEY_RED, config.red);
  subElementGet(element, KEY_GREEN_B, config.greenB);
  subElementGet(element, KEY_GREEN_R, config.greenR);
  subElementGet(element, KEY_BLUE, config.blue);
}
