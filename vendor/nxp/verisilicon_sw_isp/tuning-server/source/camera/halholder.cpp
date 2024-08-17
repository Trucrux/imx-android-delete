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

#include "camera/halholder.hpp"
#include "common/macros.hpp"

HalHolder::HalHolder() {
#ifdef REST_SIMULATE
  HalContext_t *pHalContext = (HalContext_t *)calloc(1, sizeof(HalContext_t));
  pHalContext->refCount++;
  hHal = (HalHandle_t)pHalContext;
#else
  hHal = HalOpen(SERVER_ISP_PORT);
  DCT_ASSERT(hHal);
#endif

  // reference marvin software HAL
  int32_t ret = HalAddRef(hHal);
  DCT_ASSERT(ret == RET_SUCCESS);
}

HalHolder::~HalHolder() {
  return;

  DCT_ASSERT(hHal);

  // dereference marvin software HAL
  int32_t ret = HalDelRef(hHal);
  DCT_ASSERT(ret == RET_SUCCESS);

  ret = HalClose(hHal);
  DCT_ASSERT(ret == RET_SUCCESS);

  hHal = NULL;
}

HalHolder *pHalHolder;
