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

#include "calibration/sensors.hpp"
#include "calibration/section-name.hpp"
#include <algorithm>

using namespace clb;

void Sensor::composeSubElements(XMLElement &element) {
  subElementSet(element, KEY_DRIVER_FILE, config.driverFileName);

  subElementProc(element, KEY_EC, [&](XMLElement &subElement) {
    subElementSet(subElement, KEY_GAIN, config.ec.gain);
    subElementSet(subElement, KEY_INTEGRATION_TIME, config.ec.integrationTime);
  });

  subElementSet(element, KEY_TEST_PATTERN, config.isTestPattern);
}

void Sensor::parseSubElements(XMLElement &element) {
  subElementGet(element, KEY_DRIVER_FILE, config.driverFileName);

  subElementProc(element, KEY_EC, [&](XMLElement &subElement) {
    subElementGet(subElement, KEY_GAIN, config.ec.gain);
    subElementGet(subElement, KEY_INTEGRATION_TIME, config.ec.integrationTime);
  });

  subElementGet(element, KEY_TEST_PATTERN, config.isTestPattern);
}

Sensors::Sensors(XMLDocument &document) : Abstract(document) {
  name = NAME_SENSORS;

  for (auto i = 0; i < ISP_INPUT_MAX; i++) {
    sensors.emplace_back(document);
  }
}

void Sensors::composeSubElements(XMLElement &element) {
  element.DeleteChildren();

  std::for_each(sensors.begin(), sensors.end(), [&](Sensor &sensor) {
    XMLElement *pSubElement = nullptr;

    element.InsertEndChild(pSubElement = document.NewElement(KEY_SENSOR));

    sensor.composeSubElements(*pSubElement);
  });
}

void Sensors::parseSubElements(XMLElement &element) {
  auto pSubElement = element.FirstChildElement(KEY_SENSOR);

  auto i = 0;

  while (pSubElement && i < static_cast<int>(sensors.size())) {
    sensors[i++].parseSubElements(*pSubElement);

    pSubElement = pSubElement->NextSiblingElement();
  }
}
