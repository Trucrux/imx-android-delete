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

#include "calibration/hdr.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Hdr::Hdr(XMLDocument &document) : Abstract(document) { name = NAME_HDR; }

void Hdr::composeAttributes(XMLElement &element) {
#if defined ISP_HDR_STITCH
  element.SetAttribute(KEY_ENABLE, isEnable);
#endif
}

void Hdr::composeSubElements(XMLElement &element) {
#if defined ISP_HDR_STITCH
  subElementSet(element, KEY_EXPOSURE_RATIO, config.exposureRatio);
  subElementSet(element, KEY_EXTENSION_BIT, config.extensionBit);
#endif
}

void Hdr::parseAttributes(XMLElement &element) {
#if defined ISP_HDR_STITCH
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
#endif
}

void Hdr::parseSubElements(XMLElement &element) {
#if defined ISP_HDR_STITCH
  subElementGet(element, KEY_EXPOSURE_RATIO, config.exposureRatio);
  subElementGet(element, KEY_EXTENSION_BIT, config.extensionBit);
#endif
}
