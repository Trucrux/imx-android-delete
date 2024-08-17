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

#include "calibration/dehaze.hpp"
#include "calibration/section-name.hpp"

using namespace clb;

Dehaze::Dehaze(XMLDocument &document) : Abstract(document) {
  name = NAME_DEHAZE;

#if defined CTRL_DEHAZE
  config.pCproc = new Cproc(document);
  config.pWdr = new Wdr(document);
#endif
}

Dehaze::~Dehaze() {
  if (config.pCproc) {
    delete config.pCproc;
    config.pCproc = nullptr;
  }

  if (config.pWdr) {
    delete config.pWdr;
    config.pWdr = nullptr;
  }
}

void Dehaze::composeAttributes(XMLElement &element) {
#if defined CTRL_DEHAZE
  element.SetAttribute(KEY_CPROC, enable.isCproc);
  element.SetAttribute(KEY_WDR1, enable.isGwdr);
  element.SetAttribute(KEY_WDR3, enable.isWdr3);
#endif

  (void)element;
}

void Dehaze::composeSubElements(XMLElement &element) {
#if defined CTRL_DEHAZE
  subElementProc(element, NAME_WDR, [&](XMLElement &subElement) {
    config.pWdr->composeSubElements(subElement);
  });

  subElementProc(element, NAME_CPROC, [&](XMLElement &subElement) {
    config.pCproc->composeSubElements(subElement);
  });
#endif

  (void)element;
}

void Dehaze::parseAttributes(XMLElement &element) {
#if defined CTRL_DEHAZE
  element.QueryBoolAttribute(KEY_CPROC, &enable.isCproc);
  element.QueryBoolAttribute(KEY_WDR1, &enable.isGwdr);
  element.QueryBoolAttribute(KEY_WDR3, &enable.isWdr3);
#endif

  (void)element;
}

void Dehaze::parseSubElements(XMLElement &element) {
#if defined CTRL_DEHAZE
  subElementProc(element, NAME_WDR, [&](XMLElement &subElement) {
    config.pWdr->parseSubElements(subElement);
  });

  subElementProc(element, NAME_CPROC, [&](XMLElement &subElement) {
    config.pCproc->parseSubElements(subElement);
  });
#endif

  (void)element;
}
