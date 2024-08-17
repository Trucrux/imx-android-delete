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

#include "calibration/gc.hpp"
#include "base64/base64.hpp"
#include "calibration/section-name.hpp"
#include "common/macros.hpp"

using namespace clb;

static const CamEngineGammaOutCurve_t standardCurve = {
#ifndef ISP_RGBGC
    CAM_ENGINE_GAMMAOUT_XSCALE_LOG,
    {0x000, 0x049, 0x089, 0x0B7, 0x0DF, 0x11F, 0x154, 0x183, 0x1AD, 0x1F6,
     0x235, 0x26F, 0x2D3, 0x32A, 0x378, 0x3BF, 0x3FF}
#else
{6,6, 6, 6, 6 ,6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 , 6, 6, 6, 6, 6, 6, 6, 6, 6},
{0x040,0x080,0x0c0,0x100,0x140,0x180,0x1c0,0x200,0x240,0x280,0x2c0,0x300,0x340,0x380,0x3c0,0x400,0x440,0x480,0x4c0,0x500,0x540,0x580,0x5c0,0x600,0x640,0x680,0x6c0,0x700,0x740,0x780,0x7c0,0x800,0x840,0x880,0x8c0,0x900,0x940,0x980,0x9c0,0xa00,0xa40,0xa80,0xac0,0xb00,0xb40,0xb80,0xbc0,0xc00,0xc40,0xc80,0xcc0,0xd00,0xd40,0xd80,0xdc0,0xe00,0xe40,0xe80,0xec0,0xf00,0xf40,0xf80,0xfc0},
{0x000,0x09a,0x0d3,0x0fe,0x122,0x141,0x15c,0x176,0x18d,0x1a3,0x1b8,0x1cb,0x1de,0x1ef,0x200,0x211,0x220,0x230,0x23e,0x24d,0x25a,0x268,0x275,0x282,0x28f,0x29b,0x2a7,0x2b3,0x2be,0x2c9,0x2d5,0x2df,0x2ea,0x2f5,0x2ff,0x309,0x313,0x31d,0x327,0x330,0x33a,0x343,0x34c,0x355,0x35e,0x367,0x370,0x379,0x381,0x38a,0x392,0x39a,0x3a2,0x3ab,0x3b3,0x3bb,0x3c2,0x3ca,0x3d2,0x3d9,0x3e1,0x3e9,0x3f0,0x3f7}

#endif
};

Gc::Gc(XMLDocument &document) : Abstract(document) {
  name = NAME_GC;

  config.curve = standardCurve;
}

void Gc::composeAttributes(XMLElement &element) {
  element.SetAttribute(KEY_ENABLE, isEnable);
}

void Gc::composeSubElements(XMLElement &element) {
  subElementProc(element, KEY_CURVE, [&](XMLElement &subElement) {
    subElement.SetText(base64_encode((unsigned char const *)&config.curve,
                                     sizeof(CamEngineGammaOutCurve_t))
                           .c_str());
  });
}

void Gc::parseAttributes(XMLElement &element) {
  element.QueryBoolAttribute(KEY_ENABLE, &isEnable);
}

void Gc::parseSubElements(XMLElement &element) {
  subElementProc(element, KEY_CURVE, [&](XMLElement &subElement) {
    std::string decodedString = base64_decode(subElement.GetText());

    std::copy(decodedString.begin(), decodedString.end(),
              (char *)&config.curve);
  });
}
