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

#include "calibration/dewarp.hpp"
#include "base64/base64.hpp"
#include "calibration/section-name.hpp"
#include "common/exception.hpp"

using namespace clb;

Dewarp::Dewarp(XMLDocument &document) : Abstract(document) {
  name = NAME_DEWARP;
}

Dewarp::~Dewarp() {
#if defined CTRL_DEWARP
  config.freeUserMap();
#endif
}

void Dewarp::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Dewarp::composeSubElements(XMLElement &element) {
#if defined CTRL_DEWARP
  subElementSet(element, KEY_CONFIG,
                base64_encode((unsigned char const *)&config, sizeof(config)));

  auto &map = config.distortionMap[0];

  if (map.pUserMap) {
    subElementSet(element, KEY_MAP,
                  base64_encode((unsigned char const *)map.pUserMap,
                                sizeof(map.userMapSize)));
  }
#else
  (void)element;
#endif
}

void Dewarp::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Dewarp::parseSubElements(XMLElement &element) {
#if defined CTRL_DEWARP
  std::string text;

  subElementGet(element, KEY_CONFIG, text);

  auto decodedString = base64_decode(text);

  std::copy(decodedString.begin(), decodedString.end(), (char *)&config);

  if (subElementGet(element, KEY_MAP, text)) {
    decodedString = base64_decode(text);

    auto &map = config.distortionMap[0];

    map.userMapSize = decodedString.length();

    map.pUserMap = static_cast<uint32_t *>(calloc(map.userMapSize, 1));
    std::copy(decodedString.begin(), decodedString.end(), (char *)map.pUserMap);
  }
#else
  (void)element;
#endif
}

#if defined CTRL_DEWARP
Dewarp::Config &Dewarp::Config::operator=(const Config &other) {
  if (this != &other) {
    this->freeUserMap();

    memcpy(this, &other, sizeof(*this));
  }

  return *this;
}

void Dewarp::Config::freeUserMap() {
  if (distortionMap->pUserMap) {
    free(distortionMap->pUserMap);
  }

  for (int32_t i = 0; i < DEWARP_CORE_MAX; i++) {
    dewarp_distortion_map *pDistortion = distortionMap + i;

    if (pDistortion->pUserMap) {
      pDistortion->pUserMap = nullptr;
    }

    pDistortion->userMapSize = 0;
  }
}
#endif
