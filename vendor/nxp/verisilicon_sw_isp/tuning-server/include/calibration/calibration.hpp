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

#include "abstract.hpp"
#include "calibdb/calibdb.hpp"
#include <algorithm>
#include <list>
#include <string>
#include <tinyxml2.h>

namespace clb {

struct Calibration {
  Calibration();
  ~Calibration();

  void restore(std::string fileName);
  void store(std::string fileName);

  template <typename T> T &module() {
    return dynamic_cast<T &>(
        **std::find_if(list.begin(), list.end(), [&](Abstract *pAbstract) {
          if (dynamic_cast<T *>(pAbstract)) {
            return true;
          } else {
            return false;
          }
        }));
  }

  CalibDb dbLegacy;

  std::list<clb::Abstract *> list;

  tinyxml2::XMLDocument &document =
      *new tinyxml2::XMLDocument(true, COLLAPSE_WHITESPACE);

  bool isReadOnly = false;
};

} // namespace clb

extern clb::Calibration *pCalibration;
