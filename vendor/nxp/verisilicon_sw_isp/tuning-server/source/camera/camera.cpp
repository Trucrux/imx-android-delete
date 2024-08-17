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

#include "camera/camera.hpp"
#include "calibration/calibration.hpp"
#include "calibration/dewarp.hpp"
#include "calibration/images.hpp"
#include "calibration/inputs.hpp"
#include "calibration/jpe.hpp"
#include "calibration/sensors.hpp"
#include "camera/halholder.hpp"
#include "camera/mapcaps.hpp"
#include "common/exception.hpp"
#include "common/macros.hpp"
#include "dom/dom.hpp"
#include <algorithm>
#include <functional>
#include <list>

using namespace camera;

bool Camera::captureYUV = false;
std::string Camera::fileNamePre = "";
osEvent Camera::captureDone = Camera::initCaptureDoneEvent();

Camera::Camera(AfpsResChangeCb_t *pcbResChange, void *ctxCbResChange)
    : Abstract(pcbResChange, ctxCbResChange) {
  TRACE_IN;

#if defined CTRL_DEWARP
  DCT_ASSERT(pDewarp = new dwp::Dewarp());
#endif
  DCT_ASSERT(pVom = new Vom());
  DCT_ASSERT(pSom = new Som());
  //DCT_ASSERT(pExa = new Exa());
#ifdef ISP_DRM
  DCT_ASSERT(pDrm = new Drm());
#endif

  pBufferCbContext = new BufferCbContext();

  DCT_ASSERT(!pEngine->bufferCbRegister(bufferCb, (void *)pBufferCbContext));

  TRACE_OUT;
}

Camera::~Camera() {
  TRACE_IN;

  if (state == Running) {
    previewStop();
  }

  if (state == Idle) {
    pipelineDisconnect();
  }

  int32_t ret = pEngine->bufferCbUnregister();
  DCT_ASSERT(ret == RET_SUCCESS);

  if (pBufferCbContext) {
    pBufferCbContext->mainPath.clear();
    pBufferCbContext->selfPath.clear();

    delete pBufferCbContext;
    pBufferCbContext = nullptr;
  }
#if 0
  if (pExa) {
    delete pExa;
    pExa = nullptr;
  }
#endif
  if (pSom) {
    delete pSom;
    pSom = nullptr;
  }

  if (pVom) {
    delete pVom;
    pVom = nullptr;
  }
#ifdef ISP_DRM
  if (pDrm) {
    delete pDrm;
    pDrm = nullptr;
  }
#endif

#if defined CTRL_DEWARP
  if (pDewarp) {
    delete pDewarp;
    pDewarp = nullptr;
  }
#endif

  TRACE_OUT;
}

osEvent Camera::initCaptureDoneEvent() {
  DCT_ASSERT(osEventInit(&captureDone, 1, 0) == OSLAYER_OK);
  return captureDone;
}

static int32_t dumpBufferCombined(FILE *pFile, PicBufMetaData_t tempBuf) {
  // only support YUV422 combined
  uint8_t *pYTemp = tempBuf.Data.YCbCr.combined.pData;
  uint32_t YCPlaneSize = tempBuf.Data.YCbCr.combined.PicWidthBytes * tempBuf.Data.YCbCr.combined.PicHeightPixel;

  // write Y data
  if (tempBuf.Data.YCbCr.combined.PicWidthPixel == tempBuf.Data.YCbCr.combined.PicWidthBytes) {
    // write all ctx at once
    if (1 != fwrite(pYTemp, YCPlaneSize, 1, pFile)) {
      TRACE(ITF_ERR, "%s: write failed\n", __func__);
      return RET_FAILURE;
    }
  } else {
    // write line by line
    for (uint32_t i = 0; i < tempBuf.Data.YCbCr.combined.PicHeightPixel; i++) {
      if (1 != fwrite(pYTemp, tempBuf.Data.YCbCr.combined.PicWidthBytes, 1, pFile)) {
        TRACE(ITF_ERR, "%s: write failed\n", __func__);
        return RET_FAILURE;
      }
      pYTemp += tempBuf.Data.YCbCr.combined.PicWidthBytes;
    }
  }

  return RET_SUCCESS;
}

static int32_t dumpBufferSemiplanar(FILE *pFile, PicBufMetaData_t tempBuf) {
  // only support YUV420SP, YUV422SP
  uint8_t *pYTemp = tempBuf.Data.YCbCr.semiplanar.Y.pData;
  uint8_t *pCbCrTemp = tempBuf.Data.YCbCr.semiplanar.CbCr.pData;
  uint32_t YCPlaneSize = tempBuf.Data.YCbCr.semiplanar.Y.PicWidthBytes * tempBuf.Data.YCbCr.semiplanar.Y.PicHeightPixel;
  uint32_t CbCrPlaneSize = tempBuf.Data.YCbCr.semiplanar.CbCr.PicWidthBytes * tempBuf.Data.YCbCr.semiplanar.CbCr.PicHeightPixel;

  // write Y data
  if (tempBuf.Data.YCbCr.semiplanar.Y.PicWidthPixel == tempBuf.Data.YCbCr.semiplanar.Y.PicWidthBytes) {
    // write all ctx at once
    if (1 != fwrite(pYTemp, YCPlaneSize, 1, pFile)) {
      TRACE(ITF_ERR, "%s: write failed\n", __func__);
      return RET_FAILURE;
    }
  } else {
    // write line by line
    for (uint32_t i = 0; i < tempBuf.Data.YCbCr.semiplanar.Y.PicHeightPixel; i++) {
      if (1 != fwrite(pYTemp, tempBuf.Data.YCbCr.semiplanar.Y.PicWidthBytes, 1, pFile)) {
        TRACE(ITF_ERR, "%s: write failed\n", __func__);
        return RET_FAILURE;
      }
      pYTemp += tempBuf.Data.YCbCr.semiplanar.Y.PicWidthBytes;
    }
  }

  // write CbCr data
  if (tempBuf.Data.YCbCr.semiplanar.CbCr.PicWidthPixel == tempBuf.Data.YCbCr.semiplanar.CbCr.PicWidthBytes) {
    // write all ctx at once
    if (1 != fwrite(pCbCrTemp, CbCrPlaneSize, 1, pFile)) {
      TRACE(ITF_ERR, "%s: write failed\n", __func__);
      return RET_FAILURE;
    }
  } else {
    // write line by line
    for (uint32_t i = 0; i < tempBuf.Data.YCbCr.semiplanar.CbCr.PicHeightPixel; i++) {
      if (1 != fwrite(pCbCrTemp, tempBuf.Data.YCbCr.semiplanar.CbCr.PicWidthBytes, 1, pFile)) {
        TRACE(ITF_ERR, "%s: write failed\n", __func__);
        return RET_FAILURE;
      }
      pCbCrTemp += tempBuf.Data.YCbCr.semiplanar.CbCr.PicWidthBytes;
    }
  }

  return RET_SUCCESS;
}

static int32_t dumpBufferPlanar(FILE *pFile, PicBufMetaData_t tempBuf) {
  // only suport YUV420P, YUV422P
  uint8_t *pYTemp = tempBuf.Data.YCbCr.planar.Y.pData;
  uint8_t *pCbTemp = tempBuf.Data.YCbCr.planar.Cb.pData;
  uint8_t *pCrTemp = tempBuf.Data.YCbCr.planar.Cr.pData;
  uint32_t YPlaneSize = tempBuf.Data.YCbCr.planar.Y.PicWidthBytes * tempBuf.Data.YCbCr.planar.Y.PicHeightPixel;
  uint32_t CbPlaneSize = tempBuf.Data.YCbCr.planar.Cb.PicWidthBytes * tempBuf.Data.YCbCr.planar.Cb.PicHeightPixel;
  uint32_t CrPlaneSize = tempBuf.Data.YCbCr.planar.Cr.PicWidthBytes * tempBuf.Data.YCbCr.planar.Cr.PicHeightPixel;

  // write Y data
  if (tempBuf.Data.YCbCr.planar.Y.PicWidthPixel == tempBuf.Data.YCbCr.planar.Y.PicWidthBytes) {
    // write all ctx at once
    if (1 != fwrite(pYTemp, YPlaneSize, 1, pFile)) {
      TRACE(ITF_ERR, "%s: write failed\n", __func__);
      return RET_FAILURE;
    }
  } else {
    // write line by line
    for (uint32_t i = 0; i < tempBuf.Data.YCbCr.planar.Y.PicHeightPixel; i++) {
      if (1 != fwrite(pYTemp, tempBuf.Data.YCbCr.planar.Y.PicWidthBytes, 1, pFile)) {
        TRACE(ITF_ERR, "%s: write failed\n", __func__);
        return RET_FAILURE;
      }
      pYTemp += tempBuf.Data.YCbCr.planar.Y.PicWidthBytes;
    }
  }

  // write Cb data
  if (tempBuf.Data.YCbCr.planar.Cb.PicWidthPixel == tempBuf.Data.YCbCr.planar.Cb.PicWidthBytes) {
    // write all ctx at once
    if (1 != fwrite(pCbTemp, CbPlaneSize, 1, pFile)) {
      TRACE(ITF_ERR, "%s: write failed\n", __func__);
      return RET_FAILURE;
    }
  } else {
    // write line by line
    for (uint32_t i = 0; i < tempBuf.Data.YCbCr.planar.Cb.PicHeightPixel; i++) {
      if (1 != fwrite(pCbTemp, tempBuf.Data.YCbCr.planar.Cb.PicWidthBytes, 1, pFile)) {
        TRACE(ITF_ERR, "%s: write failed\n", __func__);
        return RET_FAILURE;
      }
      pCbTemp += tempBuf.Data.YCbCr.planar.Cb.PicWidthBytes;
    }
  }

  // write Cr data
  if (tempBuf.Data.YCbCr.planar.Cr.PicWidthPixel == tempBuf.Data.YCbCr.planar.Cr.PicWidthBytes) {
    // write all ctx at once
    if (1 != fwrite(pCrTemp, CrPlaneSize, 1, pFile)) {
      TRACE(ITF_ERR, "%s: write failed\n", __func__);
      return RET_FAILURE;
    }
  } else {
    // write line by line
    for (uint32_t i = 0; i < tempBuf.Data.YCbCr.planar.Cr.PicHeightPixel; i++) {
      if (1 != fwrite(pCrTemp, tempBuf.Data.YCbCr.planar.Cr.PicWidthBytes, 1, pFile)) {
        TRACE(ITF_ERR, "%s: write failed\n", __func__);
        return RET_FAILURE;
      }
      pCrTemp += tempBuf.Data.YCbCr.planar.Cr.PicWidthBytes;
    }
  }

  return RET_SUCCESS;
}

static bool mapYCbCrBuffer(HalHandle_t hal, const PicBufMetaData_t *pSrcBuffer,
                           PicBufMetaData_t *pDstBuffer) {
  DCT_ASSERT(NULL != hal);

  DCT_ASSERT(NULL != pDstBuffer);
  DCT_ASSERT(NULL != pSrcBuffer);

  switch(pSrcBuffer->Layout) {
  case PIC_BUF_LAYOUT_COMBINED:
    if ((0 == pSrcBuffer->Data.YCbCr.combined.BaseAddress)    ||
      (0 == pSrcBuffer->Data.YCbCr.combined.PicWidthBytes) ||
      (0 == pSrcBuffer->Data.YCbCr.combined.PicHeightPixel)) {
        MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
        return false;
      }
    break;
  case PIC_BUF_LAYOUT_SEMIPLANAR:
    if ((0 == pSrcBuffer->Data.YCbCr.semiplanar.Y.BaseAddress)      ||
      (0 == pSrcBuffer->Data.YCbCr.semiplanar.CbCr.BaseAddress)     ||
      (0 == pSrcBuffer->Data.YCbCr.semiplanar.Y.PicWidthBytes)   ||
      (0 == pSrcBuffer->Data.YCbCr.semiplanar.CbCr.PicWidthBytes)||
      (0 == pSrcBuffer->Data.YCbCr.semiplanar.Y.PicHeightPixel)  ||
      (0 == pSrcBuffer->Data.YCbCr.semiplanar.CbCr.PicHeightPixel)) {
        MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
        return false;
      }
    break;
  case PIC_BUF_LAYOUT_PLANAR:
    if ((0 == pSrcBuffer->Data.YCbCr.planar.Y.BaseAddress)     ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Cb.BaseAddress)      ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Cr.BaseAddress)      ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Y.PicWidthBytes)  ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Cb.PicWidthBytes) ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Cr.PicWidthBytes) ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Y.PicHeightPixel) ||
      (0 == pSrcBuffer->Data.YCbCr.planar.Cb.PicHeightPixel)||
      (0 == pSrcBuffer->Data.YCbCr.planar.Cr.PicHeightPixel)) {
        MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
        return false;
      }
    break;
  default:
    MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
    return false;
  }

  // copy meta data
  *pDstBuffer = *pSrcBuffer;

  if (PIC_BUF_LAYOUT_COMBINED == pSrcBuffer->Layout) {
    pDstBuffer->Data.YCbCr.combined.pData = NULL;

    // get sizes & base addresses of plane
    uint32_t YCPlaneSize = pSrcBuffer->Data.YCbCr.combined.PicWidthBytes *
                           pSrcBuffer->Data.YCbCr.combined.PicHeightPixel;
    uint32_t YCBaseAddr =
        (uint32_t)(uint64_t)(pSrcBuffer->Data.YCbCr.combined.BaseAddress);

    // map combined plane
    if (RET_SUCCESS !=
        HalMapMemory(hal, YCBaseAddr, YCPlaneSize, HAL_MAPMEM_READONLY,
                     (void **)&(pDstBuffer->Data.YCbCr.combined.pData))) {
      MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
      return false;
    }
  } else if (PIC_BUF_LAYOUT_SEMIPLANAR == pSrcBuffer->Layout) {
    pDstBuffer->Data.YCbCr.semiplanar.Y.pData = NULL;
    pDstBuffer->Data.YCbCr.semiplanar.CbCr.pData = NULL;

    // get sizes & base addresses of planes
    uint32_t YPlaneSize = pSrcBuffer->Data.YCbCr.semiplanar.Y.PicWidthBytes *
                          pSrcBuffer->Data.YCbCr.semiplanar.Y.PicHeightPixel;
    uint32_t CbCrPlaneSize =
        pSrcBuffer->Data.YCbCr.semiplanar.CbCr.PicWidthBytes *
        pSrcBuffer->Data.YCbCr.semiplanar.CbCr.PicHeightPixel;
    uint32_t YBaseAddr =
        (uint32_t)(uint64_t)(pSrcBuffer->Data.YCbCr.semiplanar.Y.BaseAddress);
    uint32_t CbCrBaseAddr =
        (uint32_t)(uint64_t)(pSrcBuffer->Data.YCbCr.semiplanar.CbCr.BaseAddress);

    // map luma plane
    if (RET_SUCCESS !=
        HalMapMemory(hal, YBaseAddr, YPlaneSize, HAL_MAPMEM_READONLY,
                     (void **)&(pDstBuffer->Data.YCbCr.semiplanar.Y.pData))) {
      MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
      return false;
    }

    // map combined chroma plane
    if (RET_SUCCESS !=
        HalMapMemory(
            hal, CbCrBaseAddr, CbCrPlaneSize, HAL_MAPMEM_READONLY,
            (void **)&(pDstBuffer->Data.YCbCr.semiplanar.CbCr.pData))) {
      (void)HalUnMapMemory(hal, pDstBuffer->Data.YCbCr.semiplanar.Y.pData);
      MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
      return false;
    }
  } else if (PIC_BUF_LAYOUT_PLANAR == pSrcBuffer->Layout) {
    pDstBuffer->Data.YCbCr.planar.Y.pData = NULL;
    pDstBuffer->Data.YCbCr.planar.Cb.pData = NULL;
    pDstBuffer->Data.YCbCr.planar.Cr.pData = NULL;

    // get sizes & base addresses of planes
    uint32_t YPlaneSize = pSrcBuffer->Data.YCbCr.planar.Y.PicWidthBytes *
                          pSrcBuffer->Data.YCbCr.planar.Y.PicHeightPixel;
    uint32_t CbPlaneSize = pSrcBuffer->Data.YCbCr.planar.Cb.PicWidthBytes *
                           pSrcBuffer->Data.YCbCr.planar.Cb.PicHeightPixel;
    uint32_t CrPlaneSize = pSrcBuffer->Data.YCbCr.planar.Cr.PicWidthBytes *
                           pSrcBuffer->Data.YCbCr.planar.Cr.PicHeightPixel;
    uint32_t YBaseAddr =
        (uint32_t)(uint64_t)(pSrcBuffer->Data.YCbCr.planar.Y.BaseAddress);
    uint32_t CbBaseAddr =
        (uint32_t)(uint64_t)(pSrcBuffer->Data.YCbCr.planar.Cb.BaseAddress);
    uint32_t CrBaseAddr =
        (uint32_t)(uint64_t)(pSrcBuffer->Data.YCbCr.planar.Cr.BaseAddress);

    // map luma plane
    if (RET_SUCCESS !=
        HalMapMemory(hal, YBaseAddr, YPlaneSize, HAL_MAPMEM_READONLY,
                     (void **)&(pDstBuffer->Data.YCbCr.planar.Y.pData))) {
      MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
      return false;
    }

    // map Cb plane
    if (RET_SUCCESS !=
        HalMapMemory(hal, CbBaseAddr, CbPlaneSize, HAL_MAPMEM_READONLY,
                     (void **)&(pDstBuffer->Data.YCbCr.planar.Cb.pData))) {
      (void)HalUnMapMemory(hal, pDstBuffer->Data.YCbCr.semiplanar.Y.pData);
      MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
      return false;
    }

    // map Cr plane
    if (RET_SUCCESS !=
        HalMapMemory(hal, CrBaseAddr, CrPlaneSize, HAL_MAPMEM_READONLY,
                     (void **)&(pDstBuffer->Data.YCbCr.planar.Cr.pData))) {
      (void)HalUnMapMemory(hal, pDstBuffer->Data.YCbCr.planar.Cb.pData);
      (void)HalUnMapMemory(hal, pDstBuffer->Data.YCbCr.planar.Y.pData);
      MEMSET(pDstBuffer, 0, sizeof(PicBufMetaData_t));
      return false;
    }
  }

  return true;
}

static bool unmapYCbCrBuffer(HalHandle_t hal, PicBufMetaData_t *pData) {
  DCT_ASSERT(NULL != hal);

  DCT_ASSERT(NULL != pData);

  if (PIC_BUF_LAYOUT_COMBINED == pData->Layout) {
    if (RET_SUCCESS !=
        HalUnMapMemory(hal, (void *)(pData->Data.YCbCr.combined.pData))) {
      return false;
    }
  } else if (PIC_BUF_LAYOUT_SEMIPLANAR == pData->Layout) {
    if (RET_SUCCESS !=
        HalUnMapMemory(hal, (void *)(pData->Data.YCbCr.semiplanar.Y.pData))) {
      return false;
    }
    pData->Data.YCbCr.semiplanar.Y.pData = NULL;

    if (RET_SUCCESS !=
        HalUnMapMemory(hal,
                       (void *)(pData->Data.YCbCr.semiplanar.CbCr.pData))) {
      return false;
    }
  } else if (PIC_BUF_LAYOUT_PLANAR == pData->Layout) {
    if (RET_SUCCESS !=
        HalUnMapMemory(hal, (void *)(pData->Data.YCbCr.planar.Y.pData))) {
      return false;
    }
    pData->Data.YCbCr.planar.Y.pData = NULL;

    if (RET_SUCCESS !=
        HalUnMapMemory(hal, (void *)(pData->Data.YCbCr.planar.Cb.pData))) {
      return false;
    }
    pData->Data.YCbCr.planar.Cb.pData = NULL;

    if (RET_SUCCESS !=
        HalUnMapMemory(hal, (void *)(pData->Data.YCbCr.planar.Cr.pData))) {
      return false;
    }
  }

  MEMSET(pData, 0, sizeof(PicBufMetaData_t));
  return true;
}

int32_t captureYUVDumpBuffer(std::string fileName, MediaBuffer_t *pMediaBuffer) {
  TRACE_IN;

  int32_t result = RET_SUCCESS;
  PicBufMetaData_t tempBuf;
  memset(&tempBuf, 0, sizeof(PicBufMetaData_t));

  if (fileName.length() <= 0) {
    TRACE(ITF_ERR, "%s: invalid file name\n", __func__);
    return RET_INVALID_PARM;
  }

  if (pMediaBuffer == NULL) {
    TRACE(ITF_ERR, "%s: NULL pointer of pMediaBuffer\n", __func__);
    return RET_NULL_POINTER;
  }

  if (!mapYCbCrBuffer(pHalHolder->hHal, (PicBufMetaData_t *)(pMediaBuffer->pMetaData), &tempBuf)) {
    TRACE(ITF_ERR, "%s: bufferMap failed\n", __func__);
    return RET_FAILURE;
  }

  // write data to file
  fileName += ".yuv";
  FILE *pFile = fopen(fileName.c_str(), "wb");
  if (pFile == NULL) {
    TRACE(ITF_ERR, "%s: Couldn't open file '%s'.\n", __func__, fileName.c_str());
  } else {
    switch(tempBuf.Layout) {
    case PIC_BUF_LAYOUT_COMBINED:
      result = dumpBufferCombined(pFile, tempBuf);
      break;
    case PIC_BUF_LAYOUT_SEMIPLANAR:
      result = dumpBufferSemiplanar(pFile, tempBuf);
      break;
    case PIC_BUF_LAYOUT_PLANAR:
      result = dumpBufferPlanar(pFile, tempBuf);
      break;
    default:
      TRACE(ITF_ERR, "%s: not support yuv layout %d\n", __func__, tempBuf.Layout);
      result = RET_NOTSUPP;
      break;
    }
  }

  if (!unmapYCbCrBuffer(pHalHolder->hHal, &tempBuf)) {
    TRACE(ITF_ERR, "%s: buffer UnMap failed\n", __func__);
    result =  RET_FAILURE;
  }

  if (pFile != NULL) {
    fclose(pFile);
  }

  TRACE_OUT;

  return result;
}

void Camera::bufferCb(CamEnginePathType_t path, MediaBuffer_t *pMediaBuffer,
                      void *pCtx) {
  TRACE_IN;

  if (!pMediaBuffer->pOwner) {
    TRACE_OUT;
    return;
  }

  std::list<ItfBufferCb *> *pList = nullptr;
  BufferCbContext *pBufferCbContext = static_cast<BufferCbContext *>(pCtx);

  if (path == CAM_ENGINE_PATH_MAIN) {
    pList = &pBufferCbContext->mainPath;
  } else if (path == CAM_ENGINE_PATH_SELF) {
    pList = &pBufferCbContext->selfPath;
  }

  std::for_each(
      pList->begin(), pList->end(),
      std::bind2nd(std::mem_fun(&ItfBufferCb::bufferCb), pMediaBuffer));

  if (captureYUV) {
    if (captureYUVDumpBuffer(fileNamePre, pMediaBuffer) == RET_SUCCESS) {
      osEventSignal(&captureDone);
    }
    captureYUV = false;
  }

  TRACE_OUT;
}

int32_t Camera::pipelineConnect(bool preview, ItfBufferCb *bufferCb) {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

  if (state == Idle) {
    REPORT(RET_BUSY);
  }

  clb::Input::Type type =
      pCalibration->module<clb::Inputs>().input().config.type;

  if (preview) {
    if (type == clb::Input::Sensor) {
      sensor().checkValid().reset();
    }
    clb::Paths::Config config;

    using layoutType = std::remove_reference<decltype((config.config[CAMERIC_MI_PATH_MAIN].layout))>::type;
    //int miLayout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
    if (type == clb::Input::Sensor) {
        IsiSensorMode_t *pSensorMode = &sensor().SensorMode;
        config.config[CAMERIC_MI_PATH_MAIN].mode = (CamerIcMiDataMode_t)miMode;
        config.config[CAMERIC_MI_PATH_MAIN].layout = (CamerIcMiDataLayout_t)miLayout;
        config.config[CAMERIC_MI_PATH_MAIN].width = pSensorMode->size.width;
        config.config[CAMERIC_MI_PATH_MAIN].height =  pSensorMode->size.height;
        config.config[CAMERIC_MI_PATH_MAIN].alignMode = (CamerIcMiDataAlignMode_t)miAlgn;

        config.config[CAMERIC_MI_PATH_SELF].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_SELF].layout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
        config.config[CAMERIC_MI_PATH_SELF].width = 0;
        config.config[CAMERIC_MI_PATH_SELF].height = 0;
        config.config[CAMERIC_MI_PATH_SELF].alignMode = (CamerIcMiDataAlignMode_t)miAlgn;
        config.config[CAMERIC_MI_PATH_SELF2_BP].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_SELF2_BP].layout = (layoutType)miLayout;
        config.config[CAMERIC_MI_PATH_SELF2_BP].width = 0;
        config.config[CAMERIC_MI_PATH_SELF2_BP].height = 0;
        config.config[CAMERIC_MI_PATH_SELF2_BP].alignMode = (CamerIcMiDataAlignMode_t)miAlgn;
        config.config[CAMERIC_MI_PATH_RDI].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_RDI].layout = (layoutType)miLayout;
        config.config[CAMERIC_MI_PATH_RDI].width = 0;
        config.config[CAMERIC_MI_PATH_RDI].height = 0;
        config.config[CAMERIC_MI_PATH_RDI].alignMode = (CamerIcMiDataAlignMode_t)miAlgn;

        config.config[CAMERIC_MI_PATH_META].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_META].layout = (layoutType)miLayout;
        config.config[CAMERIC_MI_PATH_META].width = 0;
        config.config[CAMERIC_MI_PATH_META].height = 0;
        config.config[CAMERIC_MI_PATH_META].alignMode = (CamerIcMiDataAlignMode_t)miAlgn;
    } else {
        IsiResolution_t ImageResolution;
        resolutionGet(ImageResolution.width, ImageResolution.height);
        config.config[CAMERIC_MI_PATH_MAIN].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_MAIN].layout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
        config.config[CAMERIC_MI_PATH_MAIN].width = 0;
        config.config[CAMERIC_MI_PATH_MAIN].height = 0;

        config.config[CAMERIC_MI_PATH_SELF].mode = CAMERIC_MI_DATAMODE_YUV422;
        config.config[CAMERIC_MI_PATH_SELF].layout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
        config.config[CAMERIC_MI_PATH_SELF].width = ImageResolution.width;
        config.config[CAMERIC_MI_PATH_SELF].height = ImageResolution.height;
        config.config[CAMERIC_MI_PATH_SELF2_BP].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_SELF2_BP].layout = (layoutType)miLayout;
        config.config[CAMERIC_MI_PATH_SELF2_BP].width = 0;
        config.config[CAMERIC_MI_PATH_SELF2_BP].height = 0;
        config.config[CAMERIC_MI_PATH_RDI].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_RDI].layout = (layoutType)miLayout;
        config.config[CAMERIC_MI_PATH_RDI].width = 0;
        config.config[CAMERIC_MI_PATH_RDI].height = 0;

        config.config[CAMERIC_MI_PATH_META].mode = CAMERIC_MI_DATAMODE_DISABLED;
        config.config[CAMERIC_MI_PATH_META].layout = (layoutType)miLayout;
        config.config[CAMERIC_MI_PATH_META].width = 0;
        config.config[CAMERIC_MI_PATH_META].height = 0;
    }

    ret = pEngine->pathConfigSet(config);
    REPORT(ret);
  }

  std::list<ItfBufferCb *> *pList = nullptr;

  if (type == clb::Input::Sensor) {
    pList = &pBufferCbContext->mainPath;
  } else {
    pList = &pBufferCbContext->selfPath;
  }

#if defined CTRL_DEWARP
  if (pDewarp) {
    pList->push_back(pDewarp);
  }
#endif

#if defined ISP_DOM
  if (pDom) {
#if defined CTRL_DEWARP
    if (pDewarp) {
      pDewarp->bufferCbCollection.push_back(pDom);
    } else {
      pList->push_back(pDom);
    }
#else
    pList->push_back(pDom);
#endif
  }
#endif

  if (pVom) {
    pList->push_back(pVom);
  }
#ifdef ISP_DRM
  if (pDrm) {
      pList->push_back(pDrm);
  }
#endif
#if 0
  if (pExa) {
    pList->push_back(pExa);
  }
#endif
  if (bufferCb) {
    pList->push_back(bufferCb);
  }

  ret = inputConnect();
  REPORT(ret);

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Camera::pipelineDisconnect() {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

#if defined CTRL_DEWARP
  if (pDewarp) {
    pDewarp->bufferCbCollection.clear();
  }
#endif

  ret = inputDisconnect();
  REPORT(ret);

  pBufferCbContext->mainPath.clear();
  pBufferCbContext->selfPath.clear();

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Camera::captureDma(std::string filename,
                           SnapshotType /*snapshotType*/) {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

  // CamEnginePathConfig_t curMpConfig;

  // ret = pEngine->pathConfigGet(curMpConfig);
  // REPORT(ret);

  // CamEnginePathConfig_t mpConfig;
  // REFSET(mpConfig, 0);

  // switch (snapshotType) {
  // case RGB:
  //   mpConfig.mode = CAMERIC_MI_DATAMODE_YUV422;
  //   mpConfig.layout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
  //   break;

  // case RAW8:
  //   mpConfig.mode = CAMERIC_MI_DATAMODE_RAW8;
  //   mpConfig.layout = CAMERIC_MI_DATASTORAGE_INTERLEAVED;
  //   break;

  // case RAW12:
  //   mpConfig.mode = CAMERIC_MI_DATAMODE_RAW12;
  //   mpConfig.layout = CAMERIC_MI_DATASTORAGE_INTERLEAVED;
  //   break;

  // case JPEG:
  //   mpConfig.mode = CAMERIC_MI_DATAMODE_JPEG;
  //   mpConfig.layout = CAMERIC_MI_DATASTORAGE_INTERLEAVED;
  //   break;

  // default:
  //   return RET_NOTSUPP;
  // }

  // uint16_t width = 0;
  // uint16_t height = 0;

  // ret = resolutionGet(width, height);
  // REPORT(ret);

  // if (snapshotType == JPEG) {
  //   JPE.config.width = width; // TODO
  //   JPE.config.height = height;

  //   ret = pEngine->jpeConfigSet(JPE.config);
  //   REPORT(ret);

  //   ret = pEngine->jpeEnableSet(true);
  //   REPORT(ret);
  // }

  // ret = pEngine->pathConfigSet(mpConfig);
  // REPORT(ret);

  // remove callbacks, add som ctrl callback
  std::list<ItfBufferCb *> pathBackup = pBufferCbContext->mainPath;

  pBufferCbContext->mainPath.clear();
  pBufferCbContext->mainPath.push_back(pSom);

  Som::StartParam somStartParam;
  REFSET(somStartParam, 0);

  somStartParam.pFilenameBase = filename.c_str();
  somStartParam.frameCount = 1;
  somStartParam.isForceRgbOut = BOOL_TRUE;

  ret = pSom->start(&somStartParam);
  REPORT(ret);

  ret = streamingStart(1);
  REPORT(ret);

  // string orientation = pConfig->control.orientation;
  // CamerIcMiOrientation_t camerIcMiOrientaion =
  // CAMERIC_MI_ORIENTATION_ORIGINAL;

  // if (orientation == "horizontal.flip") {
  //   camerIcMiOrientaion = CAMERIC_MI_ORIENTATION_HORIZONTAL_FLIP;
  // } else if (orientation == "left") {
  //   camerIcMiOrientaion = CAMERIC_MI_ORIENTATION_ROTATE90;
  // } else if (orientation == "original") {
  //   camerIcMiOrientaion = CAMERIC_MI_ORIENTATION_ORIGINAL;
  // } else if (orientation == "right") {
  //   camerIcMiOrientaion = CAMERIC_MI_ORIENTATION_ROTATE270;
  // } else if (orientation == "vertical.flip") {
  //   camerIcMiOrientaion = CAMERIC_MI_ORIENTATION_VERTICAL_FLIP;
  // } else {
  //   DCT_ASSERT(!"UNKNOWN Orientation_t");
  // }

  // ret = pEngine->pictureOrientationSet(camerIcMiOrientaion);
  // REPORT(ret);

  osEventWait(&pSom->eventDone);

  ret = pSom->stop();
  REPORT(ret);

  // osEventWait(&pEngine->eventStopStreaming);

  pBufferCbContext->mainPath = pathBackup;

  // ret = pEngine->pathConfigSet(curMpConfig);
  // REPORT(ret);

  // if (JPEG == snapshotType) {
  //   ret = pEngine->jpeEnableSet(false);
  //   REPORT(ret);
  // }

  TRACE_OUT;

  return ret;
}

int32_t Camera::captureSensor(std::string fileName, SnapshotType snapshotType,
                              uint32_t resolution,
                              CamEngineLockType_t lockType) {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

  if (resolution && state != Running) {
    throw exc::LogicError(RET_WRONG_STATE,
                          "Start preview first, then capture it");
  }

  IsiResolution_t ImageResolution;
  resolutionGet(ImageResolution.width, ImageResolution.height);

  //const IsiSensorConfig_t *pCurSensorConfig = &sensor().config;

  // pCurSensorConfig->Resolution == 0 if DMA mode

  clb::Input::Type &type =
      pCalibration->module<clb::Inputs>().input().config.type;

  if (type == clb::Input::Sensor && !sensor().isTestPattern()) {
    ret = pEngine->searchAndLock(lockType);
    REPORT(ret);
  }

  ret = streamingStop();
  REPORT(ret);

  //ret = resolutionSet(ImageResolution.width,ImageResolution.height);
  //REPORT(ret);

  clb::Paths::Config curConfig;

  ret = pEngine->pathConfigGet(curConfig);
  REPORT(ret);

  clb::Paths::Config config = curConfig;

  switch (snapshotType) {
  case RGB:
  case YUV:
    config.config[CAMERIC_MI_PATH_MAIN].mode = CAMERIC_MI_DATAMODE_YUV420;
    config.config[CAMERIC_MI_PATH_MAIN].layout =
        CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
    break;

  case RAW8:
    config.config[CAMERIC_MI_PATH_MAIN].mode = CAMERIC_MI_DATAMODE_RAW8;
    config.config[CAMERIC_MI_PATH_MAIN].layout =
        CAMERIC_MI_DATASTORAGE_INTERLEAVED;
    config.config[CAMERIC_MI_PATH_MAIN].alignMode = CAMERIC_MI_PIXEL_UN_ALIGN;
    break;
  case RAW10:
    config.config[CAMERIC_MI_PATH_MAIN].mode = CAMERIC_MI_DATAMODE_RAW10;
    config.config[CAMERIC_MI_PATH_MAIN].layout =
        CAMERIC_MI_DATASTORAGE_INTERLEAVED;
    config.config[CAMERIC_MI_PATH_MAIN].alignMode = CAMERIC_MI_PIXEL_ALIGN_16BIT;
    break;

  case RAW12:
    config.config[CAMERIC_MI_PATH_MAIN].mode = CAMERIC_MI_DATAMODE_RAW12;
    config.config[CAMERIC_MI_PATH_MAIN].layout =
        CAMERIC_MI_DATASTORAGE_INTERLEAVED;
    config.config[CAMERIC_MI_PATH_MAIN].alignMode = CAMERIC_MI_PIXEL_ALIGN_16BIT;
    break;

  case JPEG:
    config.config[CAMERIC_MI_PATH_MAIN].mode = CAMERIC_MI_DATAMODE_JPEG;
    config.config[CAMERIC_MI_PATH_MAIN].layout =
        CAMERIC_MI_DATASTORAGE_INTERLEAVED;
    break;

  default:
    return RET_NOTSUPP;
  }

  config.config[CAMERIC_MI_PATH_SELF].mode = CAMERIC_MI_DATAMODE_DISABLED;
  config.config[CAMERIC_MI_PATH_SELF].layout = CAMERIC_MI_DATASTORAGE_PLANAR;

  if (snapshotType == JPEG) {
    clb::Jpe &jpe = pCalibration->module<clb::Jpe>();

    jpe.config.width = ImageResolution.width; // TODO
    jpe.config.height = ImageResolution.height;

    ret = pEngine->jpeConfigSet(jpe.config);
    REPORT(ret);

    ret = pEngine->jpeEnableSet(true);
    REPORT(ret);
  }

  ret = pEngine->pathConfigSet(config);
  REPORT(ret);

  // remove callbacks, add som ctrl callback
  std::list<ItfBufferCb *> mainPathBackup = pBufferCbContext->mainPath;
  std::list<ItfBufferCb *> selfPathBackup = pBufferCbContext->selfPath;

  pBufferCbContext->mainPath.clear();
  pBufferCbContext->selfPath.clear();

  pBufferCbContext->mainPath.push_back(pSom);

  ret = streamingStart();
  REPORT(ret);
  if (type == clb::Input::Sensor && !sensor().isTestPattern()) {
    ret = pEngine->searchAndLock(lockType);
    REPORT(ret);
  }

  Som::StartParam somStartParam;
  REFSET(somStartParam, 0);

  somStartParam.pFilenameBase = fileName.c_str();
  somStartParam.frameCount = 1;
  somStartParam.skipCount = 30;

  if (snapshotType == YUV) {
    somStartParam.isForceRgbOut = BOOL_FALSE;
  } else {
    somStartParam.isForceRgbOut = BOOL_TRUE;
  }
  // somStartParam.isForceRgbOut =
  //     config.config[CAMERIC_MI_PATH_MAIN].mode == CAMERIC_MI_DATAMODE_YUV422
  //         ? BOOL_TRUE
  //         : BOOL_FALSE;

  ret = pSom->start(&somStartParam);
  REPORT(ret);

  osEventWait(&pSom->eventDone);

  ret = pSom->stop();
  REPORT(ret);

  if (type == clb::Input::Sensor && !sensor().isTestPattern()) {
    ret = pEngine->unlock(lockType);
    REPORT(ret);
  }

  ret = streamingStop();
  REPORT(ret);

  //ret = resolutionSet(ImageResolution.width,ImageResolution.height);
  //REPORT(ret);

  pBufferCbContext->mainPath = mainPathBackup;
  pBufferCbContext->selfPath = selfPathBackup;

  if (JPEG == snapshotType) {
    ret = pEngine->jpeEnableSet(false);
    REPORT(ret);
  }

  ret = pEngine->pathConfigSet(curConfig);
  REPORT(ret);

  ret = streamingStart();
  REPORT(ret);

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Camera::captureSensorYUV(std::string fileName) {
  TRACE_IN;

  int32_t result = RET_FAILURE;

  if (state != Running) {
    throw exc::LogicError(RET_WRONG_STATE,
                          "Start preview first, then capture it");
  }

  fileNamePre = fileName;
  captureYUV = true;
  if (osEventTimedWait(&captureDone, 500) == OSLAYER_OK) {
    if (!access((fileNamePre+".yuv").c_str(), 0)) {
      result = RET_SUCCESS;
    } else {
      TRACE(ITF_ERR, "%s: last capture is still in progress\n", __func__);
    }
  }

  captureYUV = false;

  TRACE_OUT;

  return result;
}

#if defined CTRL_DEWARP
dwp::Dewarp &Camera::dewarp() {
  if (!pDewarp) {
    throw exc::LogicError(RET_NOTSUPP, "Dewarp isn't suppoted");
  }

  return *pDewarp;
}
#endif

#if 0
int32_t Camera::externalAlgorithmEnable(bool_t isEnable) {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

  if (isEnable) {
    if ((state == Running) && (exaCtrlSampleCb)) {
      Exa::StartParam param;
      REFSET(param, 0);

      param.sampleCb = exaCtrlSampleCb;
      param.pSampleCbContext = pSampleCbContext;
      param.sampleSkip = sampleSkip;

      ret = pExa->start(&param);
      REPORT(ret);
    }
  } else {
    ret = pExa->stop();
    REPORT(ret);
  }

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Camera::externalAlgorithmRegister(exaCtrlSampleCb_t sampleCb,
                                          void *pSampleContext,
                                          uint8_t sampleSkip) {
  TRACE_IN;

  exaCtrlSampleCb = sampleCb;
  pSampleCbContext = pSampleContext;
  sampleSkip = sampleSkip;

  TRACE_OUT;

  return RET_SUCCESS;
}
#endif

int32_t Camera::previewStart() {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;
#if 0
  if (exaCtrlSampleCb) {
    Exa::StartParam param;
    REFSET(param, 0);

    param.sampleCb = exaCtrlSampleCb;
    param.pSampleCbContext = pSampleCbContext;
    param.sampleSkip = sampleSkip;

    ret = pExa->start(&param);
    REPORT(ret);
  }
#endif

#if defined CTRL_DEWARP
  if (pDewarp) {
    clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();

    if (dewarp.isEnable) {
      ret = pDewarp->start();
      REPORT(ret);
    }
  }
#endif

#if defined ISP_DOM
  // show preview widget
  if (pDom) {
    ret = pDom->start();
    REPORT(ret);
  }
#endif

  if (pVom) {
    ret = pVom->start();
    REPORT(ret);
  }
#ifdef ISP_DRM
   if (pDrm) {
    ret = pDrm->start();
    REPORT(ret);
  }
#endif
  ret = streamingStart();
  REPORT(ret);

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Camera::previewStop() {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

  ret = streamingStop();
  REPORT(ret);

  if (pVom) {
    ret = pVom->stop();
    REPORT(ret);
  }
#ifdef ISP_DRM
  if (pDrm) {
    ret = pDrm->stop();
    REPORT(ret);
  }
#endif
#if defined ISP_DOM
  if (pDom) {
    ret = pDom->stop();
    REPORT(ret);
  }
#endif

#if defined CTRL_DEWARP
  if (pDewarp) {
    ret = pDewarp->stop();
    REPORT(ret);
  }
#endif

#if 0
  if (pExa) {
    ret = pExa->stop();
    REPORT(ret);
  }
#endif
  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Camera::reset() {
  TRACE_IN;

  int32_t ret = RET_SUCCESS;

  ret = pipelineDisconnect();
  REPORT(ret);

  ret = reset();
  REPORT(ret);

  TRACE_OUT;

  return RET_SUCCESS;
}

void Camera::versionGet(std::string &name, std::string &number,
                        std::string &date) {
  name = "Camera";
  number = VERSION_CAMERA;
  date = BUILD_TIME;
}

int32_t Camera::getPicBufLayout(uint32_t &mode, uint32_t &layout) {
  TRACE_IN;
  int32_t ret = RET_SUCCESS;
  clb::Paths::Config curConfig;

  ret = pEngine->pathConfigGet(curConfig);
  REPORT(ret);
  mode = curConfig.config[CAMERIC_MI_PATH_MAIN].mode;
  layout = curConfig.config[CAMERIC_MI_PATH_MAIN].layout;

  return RET_SUCCESS;
}

int32_t Camera::setPicBufLayout(uint32_t mode, uint32_t layout) {
  TRACE_IN;

  miMode = mode;
  miLayout = layout;

  return RET_SUCCESS;
}

Camera *pCamera = nullptr;
