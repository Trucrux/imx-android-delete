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

#include "calibration/images.hpp"
#include "calibration/section-name.hpp"
#include <algorithm>

using namespace clb;

void Image::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_FILE_NAME, [&](XMLElement &subElement) {
    subElement.SetText(config.fileName.c_str());
  });
}

void Image::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_FILE_NAME, [&](XMLElement &subElement) {
    if (subElement.GetText()) {
      config.fileName = subElement.GetText();
    }
  });
}

Images::Images(XMLDocument &document) : Abstract(document) {
  name = NAME_IMAGES;

  for (int32_t i = 0; i < ISP_INPUT_MAX; i++) {
    images.emplace_back(document);
  }
}

void Images::composeSubElements(XMLElement &element) {
  element.DeleteChildren();

  std::for_each(images.begin(), images.end(), [&](Image &image) {
    XMLElement *pSubElement = nullptr;

    element.InsertEndChild(pSubElement = document.NewElement(KEY_IMAGE));

    image.composeSubElements(*pSubElement);
  });
}

void Images::parseSubElements(XMLElement &element) {
  auto pSubElement = element.FirstChildElement(KEY_IMAGE);

  int32_t i = 0;

  while (pSubElement) {
    images[i++].parseSubElements(*pSubElement);

    pSubElement = pSubElement->NextSiblingElement();
  }
}
