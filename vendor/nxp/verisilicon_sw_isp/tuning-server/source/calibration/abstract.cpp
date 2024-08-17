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

#include "calibration/abstract.hpp"
#include "common/json-app.hpp"

using namespace clb;

Abstract::Abstract(XMLDocument &document, std::string name)
    : document(document), name(name) {}

Abstract::~Abstract() {}

void Abstract::compose(XMLElement &element) {
  composeAttributes(element);

  composeSubElements(element);
}

void Abstract::composeAttributes(XMLElement &) {}

void Abstract::composeSubElements(XMLElement &) {}

void Abstract::parse(XMLElement &element) {
  if (name != element.Name()) {
    throw std::exception();
  }

  parseAttributes(element);

  parseSubElements(element);
}

void Abstract::parseAttributes(XMLElement &) {}

void Abstract::parseSubElements(XMLElement &) {}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    std::string &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    if (pSubElement->GetText()) {
      value = pSubElement->GetText();
    }
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    int16_t &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    int32_t value32 = 0;

    pSubElement->QueryIntText(&value32);

    value = static_cast<int16_t>(value32);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    int32_t &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    pSubElement->QueryIntText(&value);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    uint8_t &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    uint32_t value32 = 0;

    pSubElement->QueryUnsignedText(&value32);

    value = static_cast<uint8_t>(value32);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    uint16_t &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    uint32_t value32 = 0;

    pSubElement->QueryUnsignedText(&value32);

    value = static_cast<uint16_t>(value32);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    uint32_t &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    pSubElement->QueryUnsignedText(&value);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    double &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    pSubElement->QueryDoubleText(&value);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    float &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    pSubElement->QueryFloatText(&value);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    bool &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    pSubElement->QueryBoolText(&value);
  }

  return pSubElement;
}

XMLElement *Abstract::subElementGet(XMLElement &element, const char *pKey,
                                    Json::Value &value) {
  XMLElement *pSubElement = nullptr;

  if ((pSubElement = element.FirstChildElement(pKey))) {
    value = JsonApp::fromString(pSubElement->GetText());
  }

  return pSubElement;
}

XMLElement *Abstract::subElementSet(XMLElement &element, const char *pKey,
                                    std::string value) {
  XMLElement *pSubElement = nullptr;

  if (!(pSubElement = element.FirstChildElement(pKey))) {
    element.InsertEndChild(pSubElement = document.NewElement(pKey));
  }

  pSubElement->SetText(value.c_str());

  return pSubElement;
}

XMLElement *Abstract::subElementSet(XMLElement &element, const char *pKey,
                                    Json::Value value) {
  XMLElement *pSubElement = nullptr;

  if (!(pSubElement = element.FirstChildElement(pKey))) {
    element.InsertEndChild(pSubElement = document.NewElement(pKey));
  }

  pSubElement->SetText(JsonApp::toString(value, "").c_str());

  return pSubElement;
}
