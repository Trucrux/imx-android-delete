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

#ifndef DEVELOPER_DEWARP_CMODEL_BASIC_H_
#define DEVELOPER_DEWARP_CMODEL_BASIC_H_

#include <stdint.h>

#define VS_32F 1
#define VS_64F 2
#define MAP_16SC2 3
#define MAP_32FC1 4
#define MAP_32FC2 5

#define INTER_BITS 4
#define INTER_TAB_SIZE (1 << INTER_BITS)

#ifndef MIN
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#endif


#define MAP_BLOCK_WIDTH 16
#define MAP_BLOCK_HEIGHT 16
#define MAP_FRACTIONAL 4   // 4 bit fractional part
#define FLOAT_MAP 0
#define INT_MAP   1
#define RESIZE_FRACTIONAL 7  // 7 bit fractional precision

typedef unsigned char uchar;
typedef uint16_t ushort;

enum decomEnum {
    VS_DECOMP_LU = 0,
    VS_DECOMP_SVD,
    VS_DECOMP_EIG,
    VS_DECOMP_CHOLESKY
};

typedef struct matrix {
    int rows;
    int cols;
    int step;
    int type;
    void* data;
} VSMatrix;

double vsInvert(VSMatrix *_src, VSMatrix *_dst, int method);

#endif  // DEVELOPER_DEWARP_CMODEL_BASIC_H_
