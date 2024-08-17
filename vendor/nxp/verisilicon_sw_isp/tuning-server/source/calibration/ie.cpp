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

#include "calibration/ie.hpp"
#include "base64/base64.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Ie::Ie(XMLDocument &document) : Abstract(document) { name = NAME_IE; }

void Ie::composeAttributes(XMLElement &element) {
#if defined ISP_IE
  element.SetAttribute(KEY_ENABLE, isEnable);
#endif

  (void)element;
}

void Ie::composeSubElements(XMLElement &element) {
#if defined ISP_IE
  subElementSet(element, KEY_MODE, config.config.mode);
  subElementSet(element, KEY_RANGE, config.config.range);

  subElementProc(element, KEY_MODE_CONFIG, [&](XMLElement &subElement) {
    std::string stringData =
        base64_encode((unsigned char const *)&config.config.ModeConfig,
                      sizeof(config.config.ModeConfig));

    subElement.SetText(stringData.c_str());
  });
#endif

  (void)element;
}

void Ie::parseAttributes(XMLElement &element) {
#if defined ISP_IE
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
#endif

  (void)element;
}

void Ie::parseSubElements(XMLElement &element) {
#if defined ISP_IE
  subElementGet(element, KEY_MODE, (int32_t &)config.config.mode);
  subElementGet(element, KEY_RANGE, (int32_t &)config.config.range);

  subElementProc(element, KEY_MODE_CONFIG, [&](XMLElement &subElement) {
    std::string decodedString = base64_decode(subElement.GetText());

    std::copy(decodedString.begin(), decodedString.end(),
              (char *)&config.config.ModeConfig);
  });
#endif

  (void)element;
}
