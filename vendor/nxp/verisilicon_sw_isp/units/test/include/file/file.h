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

#ifndef FILE__h
#define FILE__h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

int FLE_CompareBuffer(FILE *, FILE *, unsigned int *);
int FLE_BinaryCompare(FILE *, FILE *);
int FLE_Size(const char *, unsigned int *);
int FLE_Lines(const char *, unsigned int *);
int FLE_GetBufferBaseAddress(const char *, unsigned int *);
int FLE_GetBufferLastAddress(const char *, unsigned int *);
int FLE_GetBufferLength(const char *, unsigned int *);
int FLE_CopyFile(const char *sourceFileNameWithPath, const char *targetFileNameWithPath);

#ifdef __cplusplus
}
#endif

#endif
