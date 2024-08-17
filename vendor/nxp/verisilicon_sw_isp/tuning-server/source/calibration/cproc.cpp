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

#include "calibration/cproc.hpp"
#include "base64/base64.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Cproc::Cproc(XMLDocument &document) : Abstract(document) { name = NAME_CPROC; }

void Cproc::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Cproc::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_CONFIG, [&](XMLElement &subElement) {
    subElement.SetText(
        base64_encode((unsigned char const *)&config, sizeof(config)).c_str());
  });
}

void Cproc::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Cproc::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_CONFIG, [&](XMLElement &subElement) {
    std::string decodedString = base64_decode(subElement.GetText());

    std::copy(decodedString.begin(), decodedString.end(), (char *)&config);
  });
}
