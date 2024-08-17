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

#pragma once

#pragma GCC diagnostic ignored "-Wpedantic"

#include "keys.hpp"
#include <json/json.h>
#include <string>
#include <tinyxml2.h>

using namespace tinyxml2;

namespace clb {

struct Abstract {
  Abstract(XMLDocument &document, std::string name = std::string());
  virtual ~Abstract();

  virtual void compose(XMLElement &);

  virtual void composeAttributes(XMLElement &);
  virtual void composeSubElements(XMLElement &);

  virtual void parse(XMLElement &);

  virtual void parseAttributes(XMLElement &);
  virtual void parseSubElements(XMLElement &);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            std::string &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            int16_t &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            int32_t &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            uint8_t &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            uint16_t &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            uint32_t &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            double &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            float &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey, bool &value);

  XMLElement *subElementGet(XMLElement &element, const char *pKey,
                            Json::Value &value);

  template <typename Func>
  XMLElement *subElementProc(XMLElement &element, const char *pKey, Func f) {
    XMLElement *pSubElement = nullptr;

    if (!(pSubElement = element.FirstChildElement(pKey))) {
      element.InsertEndChild(pSubElement = document.NewElement(pKey));
    }

    f(*pSubElement);

    return pSubElement;
  }

  template <typename T>
  XMLElement *subElementSet(XMLElement &element, const char *pKey, T value) {
    XMLElement *pSubElement = nullptr;

    if (!(pSubElement = element.FirstChildElement(pKey))) {
      element.InsertEndChild(pSubElement = document.NewElement(pKey));
    }

    pSubElement->SetText(value);

    return pSubElement;
  }

  XMLElement *subElementSet(XMLElement &element, const char *pKey,
                            std::string value);

  XMLElement *subElementSet(XMLElement &element, const char *pKey,
                            Json::Value value);

  XMLDocument &document;
  std::string name;
  int32_t level = 0;
};

} // namespace clb
