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

#include "calibration/inputs.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

void Input::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_TYPE, [&](XMLElement &subElement) {
    subElement.SetText(config.type);
  });
}

void Input::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_TYPE, [&](XMLElement &subElement) {
    subElement.QueryIntText(reinterpret_cast<int32_t *>(&config.type));
  });
}

Inputs::Inputs(XMLDocument &document) : Abstract(document) {
  name = NAME_INPUTS;

  for (int32_t i = 0; i < ISP_INPUT_MAX; i++) {
    inputs.emplace_back(document);
  }
}

void Inputs::composeSubElements(XMLElement &element) {
  element.DeleteChildren();

  std::for_each(inputs.begin(), inputs.end(), [&](Input &input) {
    XMLElement *pSubElement = nullptr;

    element.InsertEndChild(pSubElement = document.NewElement(KEY_INPUT));

    input.composeSubElements(*pSubElement);
  });

  subElementProc(element, KEY_INDEX, [&](XMLElement &subElement) {
    subElement.SetText(config.index);
  });
}

void Inputs::parseSubElements(XMLElement &element) {
  auto pSubElement = element.FirstChildElement(KEY_INPUT);

  int32_t i = 0;

  while (pSubElement && i < static_cast<int>(inputs.size())) {
    inputs[i++].parseSubElements(*pSubElement);

    pSubElement = pSubElement->NextSiblingElement();
  }

  subElementProc(element, KEY_INDEX, [&](XMLElement &subElement) {
    subElement.QueryIntText(reinterpret_cast<int32_t *>(&config.index));
  });
}
