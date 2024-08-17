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

#ifndef _BARE_METAL_h
#define _BARE_METAL_h

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "test_base.h"

#define BM_ISP_MIV2

typedef struct {
    cJSON *pJObject;
    TestCase_t *pTestCase;
} BM_ParseConfigParam_t;

typedef struct {
    TestCtx_t *pCtx;
    TestCase_t *pTestCase;
    cJSON *pReport;
} BM_TestCaseParam_t;

int BM_ParseConfig(BM_ParseConfigParam_t *);
int BM_TestAll(void);
int BM_TestCase(BM_TestCaseParam_t *);

#ifdef __cplusplus
}
#endif

#endif
