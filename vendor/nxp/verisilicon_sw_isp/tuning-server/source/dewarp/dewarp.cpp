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

#include "dewarp/dewarp.hpp"
#include "calibration/calibration.hpp"
#include "camera/halholder.hpp"
#include "common/bash-color.hpp"
#include "common/exception.hpp"
#include "common/picture_buffer.h"
#include "common/tracer.hpp"

#if defined CTRL_DEWARP
#include "BufferManager.h"
#include "IMemoryAllocator.h"
#endif

#define CONST_BUFFER 1

#define DST_BUFFER_ADDR 0x1D000000

#define OUTPUT_YUV 0

using namespace dwp;

Dewarp::Dewarp() {
  TRACE_IN;

  DCT_ASSERT(osEventInit(&eventProcessed, 1, 0) == OSLAYER_OK);

#if defined CTRL_DEWARP
  IMemoryAllocator::create(ALLOCATOR_TYPE_V4L2);

  DCT_ASSERT(!pDriver);
  pDriver = new DW200Driver();
  DCT_ASSERT(pDriver);

  int32_t ret = pDriver->open();
  DCT_ASSERT(!ret);

  pDriver->registerBufferCallback(std::bind(&Dewarp::onFrameAvailable, this,
                                            std::placeholders::_1,
                                            std::placeholders::_2));
#endif

  TRACE_OUT;
}

Dewarp::~Dewarp() {
  TRACE_IN;

  stop();

#if defined CTRL_DEWARP
  pDriver->close(0);

  delete pDriver;
  pDriver = nullptr;
#endif

  TRACE_OUT;
}

void Dewarp::bufferCb(MediaBuffer_t *pBuffer) {
#if CONST_BUFFER
  bufferCbBufferConst(pBuffer);
#else
  bufferCbBufferDynamic(pBuffer);
#endif
}

void Dewarp::bufferCbBufferConst(MediaBuffer_t *pBuffer) {
  TRACE_IN;

#if defined CTRL_DEWARP
  if (state == Running) {
    GPUSH(pDriver, 1, 0, DST_BUFFER_ADDR);

    PicBufMetaData_t *pPicBufMetaData =
        reinterpret_cast<PicBufMetaData_t *>(pBuffer->pMetaData);

    GPUSH(pDriver, 0, 0, pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress);

    osEventWait(&eventProcessed);

    pPicBufMetaData = reinterpret_cast<PicBufMetaData_t *>(pBuffer->pMetaData);

    int width = 0, height = 0;

    pDriver->getDstSize(width, height);

    int size = width * height;

    MediaAddrBuffer m;
#if OUTPUT_YUV
    m.vcreate(width, height, MEDIA_PIX_FMT_YUV422SP);
    m.baseAddress = pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress;
    TRACE(ITF_INF, "%s: dst size %d %d 0x%08x\n", __func__, width, height, m.baseAddress);
    m.save("src.yuv");
    m.release();
#endif

    // uint64_t dst_addr = GPOP(pDriver, 2, 0);
    m.vcreate(width, height, MEDIA_PIX_FMT_YUV422SP);
    // printf("####  dst size %d %d\n", width, height);
    m.baseAddress = DST_BUFFER_ADDR;
#if OUTPUT_YUV
    m.save("dst.yuv");
#endif
    unsigned char *data = m.getBuffer();
    int32_t ret = HalWriteMemory(
        pHalHolder->hHal, pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress,
        data, size);

    ret |=
        HalWriteMemory(pHalHolder->hHal,
                       pPicBufMetaData->Data.YCbCr.semiplanar.CbCr.BaseAddress,
                       data + size, size);

    if (ret != RET_SUCCESS) {
      TRACE(ITF_ERR, COLOR_STRING(COLOR_RED, "HalWriteMemory() FAILED!!!\n"));
    }
  }

  std::for_each(bufferCbCollection.begin(), bufferCbCollection.end(),
                [&](ItfBufferCb *pBufferCb) { pBufferCb->bufferCb(pBuffer); });
#else
  pBuffer = pBuffer;
#endif

  TRACE_OUT;
}

void Dewarp::bufferCbBufferDynamic(MediaBuffer_t *pBuffer) {
  TRACE_IN;

#if defined CTRL_DEWARP
  clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();

  if (state == Running) {
    int width = 0, height = 0;

    pDriver->getDstSize(width, height);

    MediaAddrBuffer dstBuffer;

    dstBuffer.create(width, height, dewarp.config.params.output_res[0].format);

    GPUSH(pDriver, 1, 0, dstBuffer.baseAddress);

    PicBufMetaData_t *pPicBufMetaData =
        reinterpret_cast<PicBufMetaData_t *>(pBuffer->pMetaData);

    GPUSH(pDriver, 0, 0, pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress);

    osEventWait(&eventProcessed);

    pPicBufMetaData = reinterpret_cast<PicBufMetaData_t *>(pBuffer->pMetaData);

    // pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress =
    //     dstBuffer.baseAddress;

    // pPicBufMetaData->Data.YCbCr.semiplanar.CbCr.BaseAddress =
    //     dstBuffer.baseAddress + width * height;

    int size = width * height;

    int32_t ret = HalWriteMemory(
        pHalHolder->hHal, pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress,
        GMAP(dstBuffer.baseAddress, size), size);

    ret |=
        HalWriteMemory(pHalHolder->hHal,
                       pPicBufMetaData->Data.YCbCr.semiplanar.CbCr.BaseAddress,
                       GMAP(dstBuffer.baseAddress, size) + size, size);
    system("sync");

    if (ret != RET_SUCCESS) {
      TRACE(ITF_ERR, COLOR_STRING(COLOR_RED, "HalWriteMemory() FAILED!!!\n"));
    }

    GPOP(pDriver, 2, 0);
  }

  std::for_each(bufferCbCollection.begin(), bufferCbCollection.end(),
                [&](ItfBufferCb *pBufferCb) { pBufferCb->bufferCb(pBuffer); });
#else
  pBuffer = pBuffer;
#endif

  TRACE_OUT;
}

int32_t Dewarp::configGet(clb::Dewarp::Config &config) {
  clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();

#if defined CTRL_DEWARP

#else
  throw exc::LogicError(RET_NOTAVAILABLE, "Dewarp not open");
#endif

  config = dewarp.config;

  return RET_SUCCESS;
}

int32_t Dewarp::configSet(clb::Dewarp::Config config) {
  clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();

  dewarp.config = config;

  return RET_SUCCESS;
}

int32_t Dewarp::enableGet(bool &isEnable) {
  clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();

  isEnable = dewarp.isEnable;

  return RET_SUCCESS;
}

int32_t Dewarp::enableSet(bool isEnable) {
  if (isEnable) {
    int32_t ret = start();
    REPORT(ret);
  } else {
    int32_t ret = stop();
    REPORT(ret);

    osEventSignal(&eventProcessed);
  }

  clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();

  dewarp.isEnable = isEnable;

  return RET_SUCCESS;
}

void Dewarp::onFrameAvailable(uint64_t, uint64_t) {
  osEventSignal(&eventProcessed);
}

int32_t Dewarp::start(void *) {
  TRACE_IN;

  if (state == Running) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

#if defined CTRL_DEWARP
  clb::Dewarp &dewarp = pCalibration->module<clb::Dewarp>();
  DCT_ASSERT(pDriver->setParams(&dewarp.config.params));
  DCT_ASSERT(pDriver->setDistortionMap(&dewarp.config.distortionMap[0]));
  system("sync");
  if (!pDriver->start()) {
    REPORT(RET_FAILURE);
  }

  state = Running;
#endif

  TRACE_OUT;

  return RET_SUCCESS;
}

int32_t Dewarp::stop() {
  TRACE_IN;

  if (state <= Idle) {
    TRACE_OUT;

    return RET_SUCCESS;
  }

#if defined CTRL_DEWARP
  if (!pDriver->stop()) {
    REPORT(RET_FAILURE);
  }

  state = Idle;
#endif

  TRACE_OUT;

  return RET_SUCCESS;
}
