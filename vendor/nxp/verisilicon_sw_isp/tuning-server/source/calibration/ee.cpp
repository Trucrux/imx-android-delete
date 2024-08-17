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

#include "calibration/ee.hpp"
#include "calibration/section-name.hpp"
#include "common/json-app.hpp"

using namespace clb;

Ee::Ee(XMLDocument &document) : Abstract(document) {
  name = NAME_EE;

  config.reset();
  table.reset();
}

void Ee::composeAttributes(XMLElement &element) {
#if defined ISP_EE
  element.SetAttribute(KEY_ENABLE, isEnable);
#endif
}

void Ee::composeSubElements(XMLElement &element) {
#if defined ISP_EE
  element.SetAttribute(KEY_AUTO, config.isAuto);

  subElementSet(element, KEY_EDGE_GAIN, config.config.edgeGain);
  subElementSet(element, KEY_STRENGTH, config.config.strength);
  subElementSet(element, KEY_UV_GAIN, config.config.uvGain);
  subElementSet(element, KEY_Y_GAIN_DOWN, config.config.yDownGain);
  subElementSet(element, KEY_Y_GAIN_UP, config.config.yUpGain);

  subElementSet(element, KEY_TABLE, table.jTable);
#endif
}

void Ee::parseAttributes(XMLElement &element) {
#if defined ISP_EE
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
#endif
}

void Ee::parseSubElements(XMLElement &element) {
#if defined ISP_EE
  element.QueryBoolAttribute(KEY_AUTO, &config.isAuto);

  subElementGet(element, KEY_EDGE_GAIN, config.config.edgeGain);
  subElementGet(element, KEY_STRENGTH, config.config.strength);
  subElementGet(element, KEY_UV_GAIN, config.config.uvGain);
  subElementGet(element, KEY_Y_GAIN_DOWN, config.config.yDownGain);
  subElementGet(element, KEY_Y_GAIN_UP, config.config.yUpGain);

  subElementGet(element, KEY_TABLE, table.jTable);
#endif
}

void Ee::Config::reset() {
  isAuto = true;

  config.edgeGain = 1800;
  config.strength = 100;
  config.uvGain = 512;
  config.yDownGain = 10000;
  config.yUpGain = 10000;
}

void Ee::Table::reset() {
  std::string data = "{ \"columns\": [\"HDR\", \"Gain\", \"Integration Time\", "
                     "\"Edge Gain\", \"Strength\", \"UV Gain\", \"Y Gain "
                     "Down\", \"Y Gain Up\"], "
                     "\"rows\": [] }";

  jTable = JsonApp::fromString(data);
}
