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

#include "shell/shell.hpp"

#include "common/exception.hpp"
#include "shell/abstract.hpp"
#include "shell/ae.hpp"
#include "shell/af.hpp"
#include "shell/api.hpp"
#include "shell/avs.hpp"
#include "shell/awb.hpp"
#include "shell/bls.hpp"
#include "shell/cac.hpp"
#include "shell/camera.hpp"
#include "shell/cli.hpp"
#include "shell/cnr.hpp"
#include "shell/cproc.hpp"
#include "shell/demosaic.hpp"
#include "shell/dewarp.hpp"
#include "shell/dnr2.hpp"
#include "shell/dnr3.hpp"
#include "shell/dpcc.hpp"
#include "shell/dpf.hpp"
#include "shell/ec.hpp"
#include "shell/ee.hpp"
#include "shell/engine.hpp"
#include "shell/filter.hpp"
#include "shell/fs.hpp"
#include "shell/gc.hpp"
#include "shell/hdr.hpp"
#include "shell/ie.hpp"
#include "shell/image.hpp"
#include "shell/lsc.hpp"
#include "shell/reg.hpp"
#include "shell/sensor.hpp"
#include "shell/simp.hpp"
#include "shell/wb.hpp"
#include "shell/wdr.hpp"
#include <algorithm>

using namespace sh;

ShellApi::ShellApi() {
  list.push_back(new Abstract());
  list.push_back(new Ae());
  list.push_back(new Af());
  list.push_back(new Avs());
  list.push_back(new Awb());
  list.push_back(new Bls());
  list.push_back(new Cac());
  list.push_back(new Camera());
  list.push_back(new Cli());
  list.push_back(new Cnr());
  list.push_back(new Cproc());
  list.push_back(new Demosaic());
  list.push_back(new Dewarp());
  list.push_back(new Dnr2());
  list.push_back(new Dnr3());
  list.push_back(new Dpcc());
  list.push_back(new Dpf());
  list.push_back(new Ec());
  list.push_back(new Ee());
  list.push_back(new FileSystem());
  list.push_back(new Filter());
  list.push_back(new Gc());
  list.push_back(new Hdr());
  list.push_back(new Ie());
  list.push_back(new Image());
  list.push_back(new Lsc());
  list.push_back(new Reg());
  list.push_back(new Sensor());
  list.push_back(new Simp());
  list.push_back(new Wb());
  list.push_back(new Wdr());
}

ShellApi &ShellApi::process(Json::Value &jQuery, Json::Value &jResponse) {
  std::find_if(list.begin(), list.end(), [&](Shell *pRest) {
    try {
      jResponse.clear();

      pRest->process(jQuery, jResponse);

      if (jResponse[REST_RET].asInt() == RET_WRONG_STATE) {
        if (jResponse[REST_MSG].asString().empty()) {
          jResponse[REST_MSG] =
              "Wrong state, make sure already loaded sensor driver";
        }
      }
    } catch (const exc::LogicError &e) {
      if (e.error == RET_IRRELATIVE) {
        return false;
      }

      jResponse[REST_RET] = e.error;
      jResponse[REST_MSG] = e.what();
    } catch (const std::exception &e) {
      jResponse[REST_MSG] = e.what();
    }

    return true;
  });

  return *this;
}
