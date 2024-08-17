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

#ifndef _BM_GOLDEN_h
#define _BM_GOLDEN_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    unsigned int startAddr;
    unsigned int endAddr;
    unsigned int minAddr;
    unsigned int maxAddr;
    unsigned int bufferBytes;
    unsigned int bufferLength;
    unsigned int bytes;
    bool isNonlinear;
    bool isRollback;
    unsigned int rollbackCount;
} BMG_AnalyzeResult_t;

int BMG_AnalyzeBuffer(const char *, BMG_AnalyzeResult_t *);
int BMG_SeekBufferStart(FILE *, BMG_AnalyzeResult_t *);

#ifdef __cplusplus
}
#endif

#endif
