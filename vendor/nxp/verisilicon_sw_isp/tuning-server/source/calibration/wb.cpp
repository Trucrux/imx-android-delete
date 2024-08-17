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

#include "calibration/wb.hpp"
#include "calibration/section-name.hpp"
#include "common/exception.hpp"

using namespace clb;

Wb::Wb(XMLDocument &document) : Abstract(document) { name = NAME_WB; }

void Wb::composeSubElements(XMLElement &element) {
  Json::Value jCcMatrix;

  for (int32_t i = 0; i < 9; i++) {
    jCcMatrix.append(config.ccMatrix.Coeff[i]);
  }

  subElementSet(element, KEY_CC_MATRIX, jCcMatrix);

  subElementProc(element, KEY_CC_OFFSET, [&](XMLElement &subElement) {
    subElementSet(subElement, KEY_BLUE, config.ccOffset.Blue);
    subElementSet(subElement, KEY_GREEN, config.ccOffset.Green);
    subElementSet(subElement, KEY_RED, config.ccOffset.Red);
  });

  subElementProc(element, KEY_WB_GAINS, [&](XMLElement &subElement) {
    subElementSet(subElement, KEY_BLUE, config.wbGains.Blue);
    subElementSet(subElement, KEY_GREEN_B, config.wbGains.GreenB);
    subElementSet(subElement, KEY_GREEN_R, config.wbGains.GreenR);
    subElementSet(subElement, KEY_RED, config.wbGains.Red);
  });
}

void Wb::parseSubElements(XMLElement &element) {
  Json::Value jCcMatrix;

  subElementGet(element, KEY_CC_MATRIX, jCcMatrix);

  for (int32_t i = 0; i < 9; i++) {
    config.ccMatrix.Coeff[i] = jCcMatrix[i].asFloat();
  }

  subElementProc(element, KEY_CC_OFFSET, [&](XMLElement &subElement) {
    subElementGet(subElement, KEY_BLUE, config.ccOffset.Blue);
    subElementGet(subElement, KEY_GREEN, config.ccOffset.Green);
    subElementGet(subElement, KEY_RED, config.ccOffset.Red);
  });

  subElementProc(element, KEY_WB_GAINS, [&](XMLElement &subElement) {
    subElementGet(subElement, KEY_BLUE, config.wbGains.Blue);
    subElementGet(subElement, KEY_GREEN_B, config.wbGains.GreenB);
    subElementGet(subElement, KEY_GREEN_R, config.wbGains.GreenR);
    subElementGet(subElement, KEY_RED, config.wbGains.Red);
  });
}
