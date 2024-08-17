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

#include "cproc.hpp"
#include "wdr.hpp"

namespace clb {

struct Dehaze : Abstract {
  Dehaze(XMLDocument &);
  ~Dehaze();

  void composeAttributes(XMLElement &) override;
  void composeSubElements(XMLElement &) override;

  void parseAttributes(XMLElement &) override;
  void parseSubElements(XMLElement &) override;

  struct Config {
    Cproc *pCproc = nullptr;
    Wdr *pWdr = nullptr;
  } config;

  struct Enable {
    bool isCproc = false;
    bool isGwdr = false;
    bool isWdr3 = false;
  } enable;
};

} // namespace clb
