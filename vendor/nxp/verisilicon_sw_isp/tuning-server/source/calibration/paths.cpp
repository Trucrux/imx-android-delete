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

#include "calibration/paths.hpp"
#include "calibration/section-name.hpp"
#include "common/macros.hpp"

static const CamEnginePathConfig_t defaultMpConfig = {
    0U,
    0U,
    CAMERIC_MI_DATAMODE_DISABLED,
    CAMERIC_MI_DATASTORAGE_SEMIPLANAR,
    CAMERIC_MI_PIXEL_ALIGN_INVALID,
};

static const CamEnginePathConfig_t defaultSpConfig = {
    1920U,
    1080U,
    CAMERIC_MI_DATAMODE_YUV422,
    CAMERIC_MI_DATASTORAGE_SEMIPLANAR,
    CAMERIC_MI_PIXEL_ALIGN_INVALID,
};

using namespace clb;

Paths::Paths(XMLDocument &document) : Abstract(document) {
  name = NAME_PATHS;

  reset();
}

void Paths::composeSubElements(XMLElement &element) {
  element.DeleteChildren();

  for (int32_t i = 0; i < CAMERIC_MI_PATH_MAX; i++) {
    XMLElement *pSubElement = nullptr;

    element.InsertEndChild(pSubElement = document.NewElement(KEY_PATH));

    pSubElement->SetAttribute(KEY_INDEX, i);

    pathComposeSubElements(*pSubElement, config.config[i]);
  }
}

void Paths::parseSubElements(XMLElement &element) {
  auto pSubElement = element.FirstChildElement(KEY_PATH);

  int32_t i = 0;

  while (pSubElement && i < CAMERIC_MI_PATH_MAX) {
    int index = 0;

    pSubElement->QueryIntAttribute(KEY_INDEX, &index);

    pathParseSubElements(*pSubElement, config.config[index]);

    pSubElement = pSubElement->NextSiblingElement();
  }
}

void Paths::pathComposeSubElements(XMLElement &element,
                                   CamEnginePathConfig_t &config) {
  subElementProc(element, KEY_HEIGHT, [&](XMLElement &subElement) {
    subElement.SetText(config.height);
  });

  subElementProc(element, KEY_LAYOUT, [&](XMLElement &subElement) {
    subElement.SetText((int32_t)config.layout);
  });

  subElementProc(element, KEY_MODE, [&](XMLElement &subElement) {
    subElement.SetText((int32_t)config.mode);
  });

  subElementProc(element, KEY_WIDTH, [&](XMLElement &subElement) {
    subElement.SetText(config.width);
  });
}

void Paths::pathParseSubElements(XMLElement &element,
                                 CamEnginePathConfig_t &config) {
  subElementProc(element, KEY_HEIGHT, [&](XMLElement &subElement) {
    subElement.QueryUnsignedText(reinterpret_cast<uint32_t *>(&config.height));
  });

  subElementProc(element, KEY_LAYOUT, [&](XMLElement &subElement) {
    subElement.QueryIntText(reinterpret_cast<int32_t *>(&config.layout));
  });

  subElementProc(element, KEY_MODE, [&](XMLElement &subElement) {
    subElement.QueryIntText(reinterpret_cast<int32_t *>(&config.mode));
  });

  subElementProc(element, KEY_WIDTH, [&](XMLElement &subElement) {
    subElement.QueryUnsignedText(reinterpret_cast<uint32_t *>(&config.width));
  });
}

void Paths::reset() {
  config.config[CAMERIC_MI_PATH_MAIN] = defaultMpConfig;
  config.config[CAMERIC_MI_PATH_SELF] = defaultSpConfig;
}
