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

#include "calibration/simp.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Simp::Simp(XMLDocument &document) : Abstract(document) { name = NAME_SIMP; }

void Simp::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Simp::composeSubElements(XMLElement &) {}

void Simp::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Simp::parseSubElements(XMLElement &) {}
