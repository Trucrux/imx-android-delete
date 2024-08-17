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

#include "calibration/ae.hpp"
#include "calibration/af.hpp"
#include "calibration/avs.hpp"
#include "calibration/awb.hpp"
#include "calibration/bls.hpp"
#include "calibration/cac.hpp"
#include "calibration/cnr.hpp"
#include "calibration/cproc.hpp"
#include "calibration/demosaic.hpp"
#include "calibration/dnr2.hpp"
#include "calibration/dnr3.hpp"
#include "calibration/dpcc.hpp"
#include "calibration/dpf.hpp"
#include "calibration/ee.hpp"
#include "calibration/filter.hpp"
#include "calibration/gc.hpp"
#include "calibration/hdr.hpp"
#include "calibration/ie.hpp"
#include "calibration/jpe.hpp"
#include "calibration/lsc.hpp"
#include "calibration/paths.hpp"
#include "calibration/simp.hpp"
#include "calibration/wb.hpp"
#include "calibration/wdr.hpp"
#include "camera/image.hpp"
#include "camera/sensor.hpp"
#include "common/interface.hpp"

#include <cam_engine/cam_engine_api.h>

#include <cam_engine/cam_engine_aaa_api.h>
#include <cam_engine/cam_engine_cproc_api.h>
#include <cam_engine/cam_engine_imgeffects_api.h>
#include <cam_engine/cam_engine_isp_api.h>
#include <cam_engine/cam_engine_jpe_api.h>
#include <cam_engine/cam_engine_mi_api.h>
#include <cam_engine/cam_engine_simp_api.h>

namespace camera {
class Engine : public Object {
public:
  Engine();
  ~Engine();

public:
  int32_t aeConfigGet(clb::Ae::Config &);
  int32_t aeConfigSet(clb::Ae::Config);
  int32_t aeEcmGet(clb::Ae::Ecm &);
  int32_t aeEcmSet(clb::Ae::Ecm);
  int32_t aeEnableGet(bool &);
  int32_t aeEnableSet(bool);
  int32_t aeFlickerPeriodGet(float &) const;
  int32_t aeStatus(clb::Ae::Status &);
  int32_t aeReset();

  int32_t afAvailableGet(bool &);
  int32_t afConfigGet(clb::Af::Config &);
  int32_t afConfigSet(clb::Af::Config);
  int32_t afEnableGet(bool &);
  int32_t afEnableSet(bool);

  static void afpsResChangeCb(uint32_t, const void *);
  static int32_t entryAfpsResChange(void *);
  typedef int32_t(AfpsResChangeRequestCb_t)(uint16_t, uint16_t, const void *);

  int32_t avsConfigGet(clb::Avs::Config &);
  int32_t avsConfigSet(clb::Avs::Config);
  int32_t avsEnableGet(bool &);
  int32_t avsEnableSet(bool);
  int32_t avsStatusGet(clb::Avs::Status &);

  int32_t awbSetupIsiHandle();
  int32_t awbConfigGet(clb::Awb::Config &);
  int32_t awbConfigSet(clb::Awb::Config);
  int32_t awbEnableGet(bool &);
  int32_t awbEnableSet(bool);
  int32_t awbReset();
  int32_t awbStatusGet(clb::Awb::Status &);

  int32_t blsConfigGet(clb::Bls::Config &);
  int32_t blsConfigSet(clb::Bls::Config);

  int32_t bufferCbRegister(CamEngineBufferCb_t, void *);
  int32_t bufferCbUnregister();
  int32_t bufferCbGet(CamEngineBufferCb_t *, void **);

  int32_t cacEnableGet(bool &);
  int32_t cacEnableSet(bool);
  int32_t cacStatusGet(clb::Cac::Status &);

  static void cbCompletion(CamEngineCmdId_t, int32_t, const void *);

  int32_t cnrConfigGet(clb::Cnr::Config &);
  int32_t cnrConfigSet(clb::Cnr::Config);
  int32_t cnrEnableGet(bool &);
  int32_t cnrEnableSet(bool);

  int32_t cprocConfigGet(clb::Cproc::Config &);
  int32_t cprocConfigSet(clb::Cproc::Config);
  int32_t cprocEnableGet(bool &);
  int32_t cprocEnableSet(bool);

  int32_t demosaicConfigGet(clb::Demosaic::Config &);
  int32_t demosaicConfigSet(clb::Demosaic::Config);
  int32_t demosaicEnableGet(bool &);
  int32_t demosaicEnableSet(bool);

  int32_t dnr2ConfigGet(clb::Dnr2::Config &, clb::Dnr2::Generation);
  int32_t dnr2ConfigSet(clb::Dnr2::Config, clb::Dnr2::Generation);
  int32_t dnr2EnableGet(bool &, clb::Dnr2::Generation);
  int32_t dnr2EnableSet(bool, clb::Dnr2::Generation);
  int32_t dnr2Reset(clb::Dnr2::Generation);
  int32_t dnr2StatusGet(clb::Dnr2::Status &, clb::Dnr2::Generation);
  int32_t dnr2TableGet(Json::Value &, clb::Dnr2::Generation);
  int32_t dnr2TableSet(Json::Value, clb::Dnr2::Generation);

  int32_t dnr3ConfigGet(clb::Dnr3::Config &, clb::Dnr3::Generation);
  int32_t dnr3ConfigSet(clb::Dnr3::Config, clb::Dnr3::Generation);
  int32_t dnr3EnableGet(bool &, clb::Dnr3::Generation);
  int32_t dnr3EnableSet(bool, clb::Dnr3::Generation);
  int32_t dnr3Reset(clb::Dnr3::Generation);
  int32_t dnr3StatusGet(clb::Dnr3::Status &, clb::Dnr3::Generation);
  int32_t dnr3TableGet(Json::Value &, clb::Dnr3::Generation);
  int32_t dnr3TableSet(Json::Value, clb::Dnr3::Generation);

  int32_t dpccEnableGet(bool &);
  int32_t dpccEnableSet(bool);

  int32_t dpfConfigGet(clb::Dpf::Config &);
  int32_t dpfConfigSet(clb::Dpf::Config);
  int32_t dpfEnableGet(bool &);
  int32_t dpfEnableSet(bool);

  int32_t eeConfigGet(clb::Ee::Config &);
  int32_t eeConfigSet(clb::Ee::Config);
  int32_t eeEnableGet(bool &);
  int32_t eeEnableSet(bool);
  int32_t eeReset();
  int32_t eeStatusGet(clb::Ee::Status &);
  int32_t eeTableGet(Json::Value &);
  int32_t eeTableSet(Json::Value);

  int32_t filterConfigGet(clb::Filter::Config &);
  int32_t filterConfigSet(clb::Filter::Config);
  int32_t filterEnableGet(bool &);
  int32_t filterEnableSet(bool);
  int32_t filterStatusGet(clb::Filter::Status &);
  int32_t filterTableGet(Json::Value &);
  int32_t filterTableSet(Json::Value);

  int32_t gcConfigGet(clb::Gc::Config &);
  int32_t gcConfigSet(clb::Gc::Config);
  int32_t gcEnableGet(bool &);
  int32_t gcEnableSet(bool);

  int32_t hdrConfigGet(clb::Hdr::Config &);
  int32_t hdrConfigSet(clb::Hdr::Config);
  int32_t hdrEnableGet(bool &);
  int32_t hdrEnableSet(bool);
  int32_t hdrReset();

  int32_t ieConfigGet(clb::Ie::Config &);
  int32_t ieConfigSet(clb::Ie::Config);
  int32_t ieEnableGet(bool &);
  int32_t ieEnableSet(bool);

  // int32_t getDemosaicMode(CamerIcIspDemosaicMode_t &, uint8_t &) const;
  // int32_t setDemosaicMode(const CamerIcIspDemosaicMode_t &, uint8_t);

  // bool isGammaInActivated() const;
  // int32_t activateGammaIn(bool = BOOL_TRUE);
  // bool isGammaOutActivated() const;
  // int32_t activateGammaOut(bool = BOOL_TRUE);
  // bool isWBActivated() const;
  // int32_t activateGammaWB(bool = BOOL_TRUE);

  int32_t lscConfigGet(clb::Lsc::Config &);
  int32_t lscConfigSet(clb::Lsc::Config);
  int32_t lscEnableGet(bool &);
  int32_t lscEnableSet(bool);
  int32_t lscStatusGet(clb::Lsc::Status &);

  int32_t jpeConfigGet(clb::Jpe::Config &);
  int32_t jpeConfigSet(clb::Jpe::Config);
  int32_t jpeEnableGet(bool &);
  int32_t jpeEnableSet(bool);

  int32_t pathConfigGet(clb::Paths::Config &);
  int32_t pathConfigSet(clb::Paths::Config);

  int32_t pictureOrientationSet(CamerIcMiOrientation_t);

  int32_t reset();

  int32_t resolutionSet(CamEngineWindow_t, CamEngineWindow_t, CamEngineWindow_t,
                        uint32_t = 0);

  int32_t searchAndLock(CamEngineLockType_t);

  int32_t simpConfigGet(clb::Simp::Config &);
  int32_t simpConfigSet(clb::Simp::Config);
  int32_t simpEnableGet(bool &);
  int32_t simpEnableSet(bool);

  int32_t start();
  int32_t stop();

  int32_t streamingStart(uint32_t = 0);
  int32_t streamingStop();

  int32_t unlock(CamEngineLockType_t);

  int32_t wbConfigGet(clb::Wb::Config &);
  int32_t wbConfigSet(clb::Wb::Config);

  int32_t wdrConfigGet(clb::Wdr::Config &, clb::Wdr::Generation);
  int32_t wdrConfigSet(clb::Wdr::Config, clb::Wdr::Generation);
  int32_t wdrEnableGet(bool &, clb::Wdr::Generation);
  int32_t wdrEnableSet(bool, clb::Wdr::Generation);
  int32_t wdrReset(clb::Wdr::Generation);
  int32_t wdrStatusGet(clb::Wdr::Status &, clb::Wdr::Generation);
  int32_t wdrTableGet(Json::Value &, clb::Wdr::Generation);
  int32_t wdrTableSet(Json::Value, clb::Wdr::Generation);

public:
  CamEngineBlackLevel_t blackLevel;

  CamEngineConfig_t camEngineConfig;

  CamEngineCcMatrix_t ccMatrix;
  CamEngineCcOffset_t ccOffset;

  osEvent eventStart;
  osEvent eventStop;
  osEvent eventStartStreaming;
  osEvent eventStopStreaming;
  osEvent eventAcquireLock;
  osEvent eventReleaseLock;

  CamEngineHandle_t hCamEngine;

  AfpsResChangeRequestCb_t *pAfpsResChangeRequestCb = NULL;

  const void *pUserCbCtx = NULL;

  osQueue queueAfpsResChange;

  CamEngineWbGains_t wbGains;

  Image *pSimpImage = NULL;

private:
  osThread threadAfpsResChange;
};
} // namespace camera
