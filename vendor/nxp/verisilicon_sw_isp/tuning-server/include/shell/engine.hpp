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

#include <string>
#include <cameric_reg_drv/cameric_reg_description.h>

#include <cam_engine/cam_engine_api.h>

#include <cam_engine/cam_engine_aaa_api.h>
#include <cam_engine/cam_engine_cproc_api.h>
#include <cam_engine/cam_engine_imgeffects_api.h>
#include <cam_engine/cam_engine_isp_api.h>
#include <cam_engine/cam_engine_jpe_api.h>
#include <cam_engine/cam_engine_mi_api.h>
#include <cam_engine/cam_engine_simp_api.h>

#include "shell.hpp"

namespace sh {

class Engine : public Shell {
public:
  enum {
    Begin = Shell::Engine * Shell::Step,

    Ae,
    Af,
    Avs,
    Awb,
    Bls,
    Cac,
    Cnr,
    Cproc,
    Demosaic,
    Dnr2,
    Dnr3,
    Dpcc,
    Dpf,
    Ec,
    Ee,
    Filter,
    Gc,
    Hdr,
    Ie,
    Lsc,
    Reg,
    Simp,
    Wb,
    Wdr,

    End,

    Step = 100,
  };

  Engine() : Shell() {
    idBegin = Begin;
    idEnd = End;
  }
};

} // namespace sh
