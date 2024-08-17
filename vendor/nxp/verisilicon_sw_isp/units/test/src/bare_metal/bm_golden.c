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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bm_golden.h"

int BMG_AnalyzeBuffer(const char *pFile, BMG_AnalyzeResult_t *pAnalyzeResult) {
    assert(pFile);
    assert(pAnalyzeResult);

    memset(pAnalyzeResult, 0, sizeof(*pAnalyzeResult));

    int ret = 0;

    FILE *pGolden = fopen(pFile, "r");
    if (!pGolden) {
        return -ENFILE;
    }

    unsigned int addr = 0;
    unsigned int value = 0;

    unsigned int previousAddr = 0;

    int count = 0;

    for (;;) {
        ret = fscanf(pGolden, "%08x %08x", &addr, &value);
        if (ret <= 0) {
            break;
        }

        if (count == 0) {
            pAnalyzeResult->startAddr = addr;
            pAnalyzeResult->endAddr = addr;
            pAnalyzeResult->minAddr = addr;
            pAnalyzeResult->maxAddr = addr;

            previousAddr = addr;
        } else if (addr > previousAddr) {
            if (addr > previousAddr + 1) {
                pAnalyzeResult->isNonlinear = true;
            }
        } else if (addr < previousAddr) {
            if (addr < pAnalyzeResult->minAddr) {
                pAnalyzeResult->minAddr = addr;
            }

            pAnalyzeResult->rollbackCount++;

            if (previousAddr > pAnalyzeResult->maxAddr) {
                pAnalyzeResult->maxAddr = previousAddr;
            }
        }

        if (pAnalyzeResult->rollbackCount >= 1 &&
            addr >= pAnalyzeResult->startAddr) {
            pAnalyzeResult->isRollback = true;
        }

        previousAddr = addr;

        count++;

        if (!pAnalyzeResult->isRollback) {
            pAnalyzeResult->bufferBytes++;
        }
    }

    pAnalyzeResult->endAddr = previousAddr;
    pAnalyzeResult->bytes = count;

    if (pAnalyzeResult->isRollback) {
        pAnalyzeResult->bufferLength =
            pAnalyzeResult->maxAddr - pAnalyzeResult->minAddr + 1;
    } else {
        pAnalyzeResult->bufferLength = pAnalyzeResult->bytes;
    }

    if (pGolden) {
        fclose(pGolden);
    }

    return 0;
}

int BMG_SeekBufferStart(FILE *pGolden, BMG_AnalyzeResult_t *pAnalyzeResult) {
    assert(pGolden);
    assert(pAnalyzeResult);

    if (pAnalyzeResult->isRollback) {
        unsigned int addr = 0;
        unsigned int value = 0;

        unsigned int previousAddr = 0;
        unsigned int rollbackCount = 0;

        for (unsigned int i = 0; i < pAnalyzeResult->bytes; i++) {
            if (fscanf(pGolden, "%08x %08x", &addr, &value) <= 0) {
                break;
            }
            if (addr < previousAddr) {
                rollbackCount++;
            }

            if (rollbackCount == pAnalyzeResult->rollbackCount - 1) {
                if (addr >= pAnalyzeResult->endAddr) {
                    char desc = 0;

                    do {
                        desc = fgetc(pGolden);
                    } while (desc != '\n');

                    break;
                }
            }

            previousAddr = addr;
        }
    }

    return 0;
}
