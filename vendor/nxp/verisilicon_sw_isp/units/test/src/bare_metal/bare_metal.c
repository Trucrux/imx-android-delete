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
#include <libgen.h>

#include "bm_golden.h"

#include "test_base.h"

#include "bare_metal.h"
#include "bm_parse.h"
#include "file.h"

//#define BM_ISP_MIV2

CREATE_TRACER(BM_TEST_INF, "BM-TEST: ", INFO, 1);
CREATE_TRACER(BM_TEST_ERR, "BM-TEST: ", ERROR, 1);

typedef struct {
    TestCtx_t *pCtx;
    int index;
    BMTestConfig_t *pBmTestConfig;
    cJSON *pReport;
} BM_FrameTestParam_t;

typedef struct {
    TestCtx_t *pCtx;
    BMTestConfigIO_t *pInput;
    cJSON *pReport;
} BM_LoadInputParam_t;

typedef struct {
    TestCtx_t *pCtx;
    BMTestConfig_t *pTestConfig;
    cJSON *pReport;
} BM_VerifyFrameParam_t;

typedef struct {
    char *pGoldenFile;
    BMG_AnalyzeResult_t *pAnalyzeResult;
    cJSON *pReport;
} BM_VerifyChannelParam_t;

static int BM_FrameTest(BM_FrameTestParam_t *);
static int BM_LoadInput(BM_LoadInputParam_t *);
static int BM_VerifyFrame(BM_VerifyFrameParam_t *);
static int BM_VerifyChannel(BM_VerifyChannelParam_t *pParam);

int BM_ParseConfig(BM_ParseConfigParam_t *pParam) {
    assert(pParam);

    return BMPAS_Parse(pParam->pJObject, pParam->pTestCase);
}

int BM_TestCase(BM_TestCaseParam_t *pParam) {
    assert(pParam);

    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    if (!pCtx->pBmTest) {
        BMTestCreate(pCtx);
    }

    int ret = BMTestInitAllBuffers(pCtx);
    if (ret) {
        return ret;
    }
    ret = BMTestCheckAllBuffers(pCtx);
    if (ret) {
        return ret;
    }
    TestCase_t *pTestCase = pParam->pTestCase;
    assert(pTestCase);

    ret = 0;

    for (int i = 0; i < pTestCase->configCount; i++) {
        BM_FrameTestParam_t frameTestParam = {
            .pCtx = pCtx,
            .index = i,
            .pBmTestConfig = &pTestCase->config[i].bm,
            .pReport = pParam->pReport,
        };

        ret = BM_FrameTest(&frameTestParam);
        if (ret != 0) {
            break;
        }
    }

    if (pCtx->pBmTest) {
        BMTestDestroy(pCtx, pCtx->pBmTest);
        pCtx->pBmTest = NULL;
    }

    return ret;
}

static int BM_FrameTest(BM_FrameTestParam_t *pParam) {
    assert(pParam);

    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    cJSON *pReport = pParam->pReport;

    cJSON *pFrame = cJSON_CreateObject();
    cJSON_AddItemToArray(pReport, pFrame);

    cJSON_AddNumberToObject(pFrame, "frame", pParam->index);

    BMTestConfig_t *pBmTestConfig = pParam->pBmTestConfig;
    assert(pBmTestConfig);

    int ret;
    //int ret = BMTestInitAllBuffers(pCtx);
    //if (ret) {
    //    return ret;
    //}

    //ret = BMTestCheckAllBuffers(pCtx);
    //if (ret) {
    //    return ret;
    //}

    BM_LoadInputParam_t loadInputParam = {
        .pCtx = pCtx,
        .pInput = &pBmTestConfig->input,
        .pReport = pFrame,
    };

    ret = BM_LoadInput(&loadInputParam);
    if (ret) {
        return ret;
    }

    /* Load commands and patch the buffer address. */
    ret = BMTestLoadCommands(pCtx, pBmTestConfig->registerFile, pFrame);
    if (ret) {
        return ret;
    }

    /* Execute the commands. */
    ret = BMTestExecCommands(pCtx, pCtx->pBmTest);
    if (ret) {
        return ret;
    }

    BM_VerifyFrameParam_t verifyOutputParam = {
        .pCtx = pCtx,
        .pTestConfig = pBmTestConfig,
        .pReport = pFrame,
    };

    ret = BM_VerifyFrame(&verifyOutputParam);
    if (ret) {
        return ret;
    }

    return ret;
}

static int BM_LoadInput(BM_LoadInputParam_t *pParam) {
    assert(pParam);

    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    BMTestConfigIO_t *pInput = pParam->pInput;
    assert(pInput);

    cJSON *pReport = pParam->pReport;
    assert(pReport);

    unsigned int size = 0;

    int ret = 0;

    for (int i = 0; i < BM_CHANNEL_MAX; i++) {
        if (!strlen(pInput->filename[i])) {
           continue; //break;
        }

        ret = FLE_Size(pInput->filename[i], &size);
        if (ret < 0) {
            return ret;
        }

        if (size == 0) {
            continue;
        }

        ret = BMTestLoadInput(pCtx, pInput, i, pReport);
        TRACE(BM_TEST_INF,"load input file %d %s.\n",i,pInput->filename[i]);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static int BM_VerifyFrame(BM_VerifyFrameParam_t *pParam) {
    assert(pParam);

    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    BMTestConfig_t *pTestConfig = pParam->pTestConfig;
    assert(pTestConfig);

    cJSON *pReport = pParam->pReport;
    assert(pReport);

    int ret = 0;

    bool result = true;

    for (int j = 0; j < BM_PATH_MAX; j++) {
        if (!strlen(pTestConfig->output[j].filename[BM_CHANNEL_Y])) {
            //continue;
        }

        for (int i = 0; i < BM_CHANNEL_MAX; i++) {
            char *pGoldenFile = pTestConfig->output[j].filename[i];

            if (!strlen(pGoldenFile)) {
               continue; //break;
            }

            cJSON *pItem = cJSON_CreateObject();
            cJSON_AddItemToObject(pReport, basename(pGoldenFile), pItem);

            BMG_AnalyzeResult_t analyzeResult = {0};

            ret = BMG_AnalyzeBuffer(pGoldenFile, &analyzeResult);
            if (ret < 0) {
                cJSON_AddFalseToObject(pItem, "is.exist");
                return ret;
            }

            if (!analyzeResult.bufferLength) {
                cJSON_AddTrueToObject(pItem, "is.empty.file");
                continue;
            }

            if (analyzeResult.isRollback) {
                cJSON_AddTrueToObject(pItem, "is.rollback");

                char desc[100] = {0};

                if (analyzeResult.minAddr > 0) {
                    snprintf(desc, sizeof(desc), "0x%X", analyzeResult.minAddr);
                    cJSON_AddStringToObject(pItem, "output.offset", desc);
                }

                snprintf(desc, sizeof(desc), "0x%X",
                         analyzeResult.bufferLength);
                cJSON_AddStringToObject(pItem, "buffer.length", desc);
            }

            if (analyzeResult.isNonlinear) {
                cJSON_AddTrueToObject(pItem, "is.nonlinear");
            }

            ret = BMTestSaveOutput(pCtx, pTestConfig, &analyzeResult, j, i);
            if (ret < 0) {
                return ret;
            }

            BM_VerifyChannelParam_t verifyChannelParam = {
                .pGoldenFile = pGoldenFile,
                .pAnalyzeResult = &analyzeResult,
                .pReport = pItem,
            };

            ret = BM_VerifyChannel(&verifyChannelParam);
            if (ret < 0) {
                result = false;
            }
        }
    }

    return result ? 0 : -1;
}

static int BM_VerifyChannel(BM_VerifyChannelParam_t *pParam) {
    assert(pParam);

    char *pGoldenFile = pParam->pGoldenFile;
    assert(pGoldenFile);

    FILE *pGolden = fopen(pGoldenFile, "r");
    assert(pGolden);

    char out[MAX_LEN] = {0};
    snprintf(out, MAX_LEN, "%s.out", pGoldenFile);

    FILE *pOut = fopen(out, "r");
    assert(pOut);

    BMG_AnalyzeResult_t *pAnalyzeResult = pParam->pAnalyzeResult;
    assert(pAnalyzeResult);

    int ret = 0;

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

    unsigned int percent = 0;

    ret |= FLE_CompareBuffer(pGolden, pOut, &percent);
    if (ret < 0) {
        TRACE(BM_TEST_ERR, "Compare %s NG \n", basename(pGoldenFile));
    } else {
        TRACE(BM_TEST_INF, "Compare %s OK \n", basename(pGoldenFile));
    }

    char desc[20] = "OK";

    if (percent < 100) {
        snprintf(desc, sizeof(desc), "%d%%", percent);
    }

    cJSON *pReport = pParam->pReport;
    assert(pReport);

    cJSON_AddStringToObject(pReport, "compare", desc);

    if (pGolden) {
        fclose(pGolden);
        pGolden = NULL;
    }

    if (pOut) {
        fclose(pOut);
        pOut = NULL;
    }

    return ret;
}
