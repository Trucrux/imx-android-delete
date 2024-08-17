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
#include <algorithm>

namespace clb {

struct Input : Abstract {
  Input(XMLDocument &document) : Abstract(document) {}

  enum Type { Invalid, Sensor, Image, Tpg, Max };

  void composeSubElements(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  struct Config {
    Type type;
  } config;
};

struct Inputs : Abstract {
  Inputs(XMLDocument &);

  void composeSubElements(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  struct Config {
    int32_t index = 0;
  } config;

  Input &input() { return inputs[config.index]; }

  std::vector<Input> inputs;
};

} // namespace clb
