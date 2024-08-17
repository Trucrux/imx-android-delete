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

#include "calibration/calibration.hpp"
#include "assert.h"
#include "calibration/ae.hpp"
#include "calibration/af.hpp"
#include "calibration/avs.hpp"
#include "calibration/awb.hpp"
#include "calibration/bls.hpp"
#include "calibration/cac.hpp"
#include "calibration/cnr.hpp"
#include "calibration/cproc.hpp"
#include "calibration/dehaze.hpp"
#include "calibration/demosaic.hpp"
#include "calibration/dewarp.hpp"
#include "calibration/dnr2.hpp"
#include "calibration/dnr3.hpp"
#include "calibration/dpcc.hpp"
#include "calibration/dpf.hpp"
#include "calibration/ee.hpp"
#include "calibration/filter.hpp"
#include "calibration/gc.hpp"
#include "calibration/hdr.hpp"
#include "calibration/ie.hpp"
#include "calibration/images.hpp"
#include "calibration/inputs.hpp"
#include "calibration/lsc.hpp"
#include "calibration/paths.hpp"
#include "calibration/section-name.hpp"
#include "calibration/sensors.hpp"
#include "calibration/simp.hpp"
#include "calibration/wb.hpp"
#include "calibration/wdr.hpp"
#include "common/exception.hpp"
#include <algorithm>
#include <iostream>

using namespace clb;

Calibration::Calibration() {
  list.push_back(new Ae(document));
  list.push_back(new Af(document));
  list.push_back(new Avs(document));
  list.push_back(new Awb(document));
  list.push_back(new Bls(document));
  list.push_back(new Cac(document));
  list.push_back(new Cnr(document));
  list.push_back(new Cproc(document));
  list.push_back(new Dehaze(document));
  list.push_back(new Demosaic(document));
  list.push_back(new Dewarp(document));
  list.push_back(new Dnr2(document));
  list.push_back(new Dnr3(document));
  list.push_back(new Dpcc(document));
  list.push_back(new Dpf(document));
  list.push_back(new Ee(document));
  list.push_back(new Filter(document));
  list.push_back(new Gc(document));
  list.push_back(new Hdr(document));
  list.push_back(new Ie(document));
  list.push_back(new Images(document));
  list.push_back(new Inputs(document));
  list.push_back(new Lsc(document));
  list.push_back(new Paths(document));
  list.push_back(new Sensors(document));
  list.push_back(new Simp(document));
  list.push_back(new Wb(document));
  list.push_back(new Wdr(document));
}

Calibration::~Calibration() {
  dbLegacy.uninstall();

  std::for_each(list.begin(), list.end(),
                [&](Abstract *pAbstract) { delete pAbstract; });

  delete &document;
}

void Calibration::restore(std::string fileName) {
  dbLegacy.uninstall();
  document.Clear();

  if (access(fileName.c_str(), F_OK) != 0) {
    throw exc::LogicError(RET_FAILURE, "calibration XML " + fileName + " is not exist!");
  }

  auto ret = document.LoadFile(fileName.c_str());
  if (ret != XML_SUCCESS) {
    throw exc::LogicError(RET_FAILURE, "Can't load XML file: " + fileName);
  }

  dbLegacy.install(document);

  auto *pRoot = document.RootElement();
  if (!pRoot) {
    return;
  }

  auto *pSubElement = pRoot->FirstChildElement(NAME_TUNING);
  if (!pSubElement) {
    return;
  }

  pSubElement = pSubElement->FirstChildElement();

  while (pSubElement) {
    std::for_each(list.begin(), list.end(), [&](Abstract *pAbstract) {
      try {
        pAbstract->parse(*pSubElement);
      } catch (const std::exception &) {
      }
    });

    pSubElement = pSubElement->NextSiblingElement();
  }
}

void Calibration::store(std::string fileName) {
  auto *pRoot = document.RootElement();
  if (!pRoot) {
    document.InsertFirstChild(document.NewDeclaration());
    document.InsertEndChild(pRoot = document.NewElement("matfile"));
  }

  auto *pElementTuning = pRoot->FirstChildElement(NAME_TUNING);
  if (!pElementTuning) {
    pRoot->InsertEndChild(pElementTuning = document.NewElement(NAME_TUNING));
  }

  std::for_each(list.begin(), list.end(), [&](Abstract *pAbstract) {
    XMLElement *pSubElement = nullptr;

    if (!(pSubElement =
              pElementTuning->FirstChildElement(pAbstract->name.c_str()))) {
      pElementTuning->InsertEndChild(
          pSubElement = document.NewElement(pAbstract->name.c_str()));
    }

    pAbstract->compose(*pSubElement);
  });

  auto ret = document.SaveFile(fileName.c_str());
  if (ret != XML_SUCCESS) {
    std::cerr << "XML save file error: " << fileName << std::endl;
  }
}

Calibration *pCalibration;
