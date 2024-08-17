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

#include "drm/drm.hpp"
#include "camera/halholder.hpp"
#include <ebase/builtins.h>
#include <MediaBuffer.h>
#include <IMemoryAllocator.h>

Drm::Drm() {
    IMemoryAllocator::create(ALLOCATOR_TYPE_V4L2);
    pDisplay = IDisplay::createObject(VIV_DISPLAY_TYPE_DRM);
}

Drm::~Drm() {
  TRACE_IN;
  delete pDisplay;
  TRACE_OUT;
}

void Drm::bufferCb(MediaBuffer_t *pBuffer) {
  TRACE_IN;

  PicBufMetaData_t *pPicBufMetaData = (PicBufMetaData_t *)(pBuffer->pMetaData);

  int format = 0, width = 0, height = 0;
  MediaAddrBuffer buffer;


  if (pPicBufMetaData->Layout == PIC_BUF_LAYOUT_SEMIPLANAR) {
    if (pPicBufMetaData->Type == PIC_BUF_TYPE_YCbCr422) {
      format = MEDIA_PIX_FMT_YUV422SP;
    } else if (pPicBufMetaData->Type == PIC_BUF_TYPE_YCbCr420) {
      format = MEDIA_PIX_FMT_YUV420SP;
    } else {
      TRACE_OUT;
      return;
    }
    width = pPicBufMetaData->Data.YCbCr.semiplanar.Y.PicWidthPixel;
    height = pPicBufMetaData->Data.YCbCr.semiplanar.Y.PicHeightPixel;
    buffer.vcreate(width, height, format);
    buffer.baseAddress = pPicBufMetaData->Data.YCbCr.semiplanar.Y.BaseAddress;
  } else if (pPicBufMetaData->Layout == PIC_BUF_LAYOUT_COMBINED) {
    format = MEDIA_PIX_FMT_YUV422I;
    width = pPicBufMetaData->Data.YCbCr.combined.PicWidthPixel / 2;
    height = pPicBufMetaData->Data.YCbCr.combined.PicHeightPixel;
    buffer.vcreate(width, height, format);
    buffer.baseAddress = pPicBufMetaData->Data.YCbCr.combined.BaseAddress;
  } else {
    TRACE_OUT;
    return;
  }

  pDisplay->showBufferExt(buffer.getBuffer(), buffer.getPhyAddr(),
            width, height, format, buffer.mSize);

  TRACE_OUT;
}

void Drm::cbCompletion(int cmdId, int32_t result,
                       const void *pUserContext) {
  TRACE_IN;

  TRACE_OUT;
}

int32_t Drm::start(void *) {

  return RET_SUCCESS;
}

int32_t Drm::stop() {

  /* clear screen */
  pDisplay->showBufferExt(NULL, 0, -1, -1, -1, -1);

  return RET_SUCCESS;
}
