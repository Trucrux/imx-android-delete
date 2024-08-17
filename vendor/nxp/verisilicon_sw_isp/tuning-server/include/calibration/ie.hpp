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
#include <cameric_drv/cameric_drv_api.h>
#include <cameric_drv/cameric_ie_drv_api.h>

namespace clb {

struct Ie : Abstract {
  Ie(XMLDocument &);

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  bool isEnable = false;

  struct Config {
    Config() {
      config.mode = CAMERIC_IE_MODE_COLOR;
      config.range = CAMERIC_IE_RANGE_BT601;

      if (config.mode == CAMERIC_IE_MODE_COLOR) {
        config.ModeConfig.ColorSelection.col_selection =
            CAMERIC_IE_COLOR_SELECTION_RGB;
      }
    }

    CamerIcIeConfig_t config;
  } config;
};

} // namespace clb
