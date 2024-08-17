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

#include "calibration/ec.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Ec::Ec(XMLDocument &document) : Abstract(document) {}

void Ec::composeSubElements(XMLElement &element) {
  subElementSet(element, KEY_GAIN, config.gain);
  subElementSet(element, KEY_INTEGRATION_TIME, config.integrationTime);
}

void Ec::parseSubElements(XMLElement &element) {
  subElementGet(element, KEY_GAIN, config.gain);
  subElementGet(element, KEY_INTEGRATION_TIME, config.integrationTime);
}
