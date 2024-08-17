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

#include "camera/abstract.hpp"
#include "common/version.hpp"
#if defined CTRL_DEWARP
#include "dewarp/dewarp.hpp"
#endif
//#include "exa/exa.hpp"
#include "som/som.hpp"
#include "vom/vom.hpp"

#ifdef ISP_DRM
#include "drm/drm.hpp"
#endif

namespace camera {

struct Camera : Abstract, Version {
  Camera(AfpsResChangeCb_t * = nullptr, void * = nullptr);
  virtual ~Camera();

  static void bufferCb(CamEnginePathType_t path, MediaBuffer_t *pMediaBuffer,
                       void *pCtx);

  int32_t pipelineConnect(bool = false, ItfBufferCb * = nullptr);
  int32_t pipelineDisconnect();

  int32_t captureDma(std::string, SnapshotType);
  int32_t captureSensor(std::string, SnapshotType, uint32_t,
                        CamEngineLockType_t = CAM_ENGINE_LOCK_ALL);
  int32_t captureSensorYUV(std::string fileName);
  static osEvent initCaptureDoneEvent();

#if defined CTRL_DEWARP
  dwp::Dewarp &dewarp();
#endif

  // int32_t externalAlgorithmEnable(bool_t = BOOL_TRUE);
  // int32_t externalAlgorithmRegister(exaCtrlSampleCb_t, void *, uint8_t);

  int32_t previewStart();
  int32_t previewStop();

  int32_t reset();
  
  void versionGet(std::string &name, std::string &number,
                  std::string &time) override;

  int32_t getPicBufLayout(uint32_t &mode, uint32_t &layout);
  int32_t setPicBufLayout(uint32_t mode, uint32_t layout);

  // friend class PfidItf;

#if defined CTRL_DEWARP
  dwp::Dewarp *pDewarp = nullptr;
#endif

  //Exa *pExa = nullptr;
  Som *pSom = nullptr;
  Vom *pVom = nullptr;
#ifdef ISP_DRM
  Drm *pDrm = nullptr;
#endif

  struct BufferCbContext {
    std::list<ItfBufferCb *> mainPath;
    std::list<ItfBufferCb *> selfPath;
  };

  BufferCbContext *pBufferCbContext = nullptr;

  //exaCtrlSampleCb_t exaCtrlSampleCb = nullptr;
  void *pSampleCbContext = nullptr;
  uint8_t sampleSkip = 0;
  static bool captureYUV;
  static std::string fileNamePre;
  static osEvent captureDone;

  uint32_t miMode = CAMERIC_MI_DATAMODE_YUV422;
  uint32_t miLayout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
  uint32_t miAlgn = CAMERIC_MI_PIXEL_UN_ALIGN;
};

} // namespace camera

extern camera::Camera *pCamera;
