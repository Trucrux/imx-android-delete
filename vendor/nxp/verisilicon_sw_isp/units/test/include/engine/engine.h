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

#ifndef _TEST_ENGINE_
#define _TEST_ENGINE_

#include "cJSON.h"
#include "test_base.h"

#define FORMAT_RGGB		0
#define FORMAT_BGGR		1
#define FORMAT_GRBG		2
#define FORMAT_GBRG		3
#define FORMAT_GRAY		4
#define FORMAT_RGB		5
#define FORMAT_RGBA		6
#define FORMAT_YUV420	7
#define FORMAT_YUV422	8
#define FORMAT_YUV444	9

typedef struct {
    cJSON *pJObject;
    TestCase_t *pTestCase;
} ENG_ParseConfigParam_t;

typedef struct {
    TestCtx_t *pCtx;
    TestCase_t *pTestCase;
    cJSON *pReport;
} ENG_TestCaseParam_t;

int ENG_ParseConfig(ENG_ParseConfigParam_t *);
int ENG_GoldenStore(ENG_TestCaseParam_t *pParam, int config_cnt, int frame_cnt, cJSON *pFrameItem);
int ENG_GoldenVerify(ENG_TestCaseParam_t *pParam, int config_cnt, int frame_cnt, cJSON *pFrameItem);
int ENG_TestCase(ENG_TestCaseParam_t *);
void readBayerRawImage(const char* filename, int width, int height, int bits, uint8_t *rawimage);
int CreateVawImage(uint8_t *rawimage, int width, int height, int bitdepth, int format);

#endif
