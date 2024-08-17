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

#include "versions.hpp"
#include "camera/camera.hpp"
#include <algorithm>
#include <shell/shell.hpp>

Versions::Versions() {
#if defined EDITION_B1
  collection.push_back(new Version("Edition", "", "B1"));
#elif defined EDITION_F1
  collection.push_back(new Version("Edition", "", "F1"));
#elif defined EDITION_J1
  collection.push_back(new Version("Edition", "", "J1"));
#elif defined EDITION_L1
  collection.push_back(new Version("Edition", "", "L1"));
#elif defined EDITION_N1
  collection.push_back(new Version("Edition", "", "N1"));
#elif defined EDITION_S1
  collection.push_back(new Version("Edition", "", "S1"));
#elif defined EDITION_S2
  collection.push_back(new Version("Edition", "", "S2"));
#elif defined EDITION_W1
  collection.push_back(new Version("Edition", "", "W1"));
#elif defined EDITION_W2
  collection.push_back(new Version("Edition", "", "W2"));
#else
  collection.push_back(new Version("Edition", "", "U1"));
#endif

  std::string name;
  std::string number;
  std::string date;

  if (pCamera) {
    pCamera->versionGet(name, number, date);

    collection.push_back(new Versions::Version(name, number, date));
  }

  sh::Shell().versionGet(name, number, date);

  collection.push_back(new Versions::Version(name, number, date));

#if defined UNITS_NAME
  collection.push_back(
      new Versions::Version(UNITS_NAME, UNITS_NUMBER, UNITS_DATE));
#endif
}

Versions::~Versions() {
  std::for_each(collection.begin(), collection.end(),
                [&](Version *pVersion) { delete pVersion; });

  collection.clear();
}
