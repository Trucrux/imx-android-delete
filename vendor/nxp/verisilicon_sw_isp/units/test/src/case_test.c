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

#include "bare_metal.h"
#include "cJSON.h"
#include "case_list.h"
#include "case_result.h"
#include "engine.h"
#include "json_app.h"
#include "parse.h"
#include "test_base.h"
#include <assert.h>
#include <libgen.h>
#include <stdio.h>

#include "isp_version.h"

#define MAX_LEN 256

CREATE_TRACER(CASE_TEST_INF, "ISP-TEST: ", INFO, 1);

static CaseResult_t CST_TestCase(TestCtx_t *, TestCase_t *, cJSON *);

int CST_TestFromList(TestCtx_t *pCtx, const char *pFilename) {
    int ret = 0;

    cJSON *pReport = cJSON_CreateArray();

    int passCount = 0;
    int invalidCount = 0;
    int failedCount = 0;

    char **ppCaseList = NULL;
    int caseCount = 0;
    unsigned int repeatTimes = 1;

    ret = CL_LoadCaseList(pFilename, &ppCaseList, &caseCount);
    assert(!ret);

    TestCase_t testCase = {{0}};

testStart:
    printf( "Test list repeat times: %d / %d\n", repeatTimes, pCtx->testRepeatTimes );

    for (int i = 0; i < caseCount; i++) {
        char caseFilename[MAX_LEN] = "";

        snprintf(caseFilename, sizeof(caseFilename), CASE_PREFIX"%s", ppCaseList[i]);

        TRACE(CASE_TEST_INF, "CASE [%03d/%03d]: %s START...\n", i + 1,
              caseCount, basename(ppCaseList[i]));

        cJSON *pJRoot = JA_ReadFromFile(caseFilename);
        assert(pJRoot);

        pCtx->hHal = HalOpen(0);
        assert(pCtx->hHal);

        memset(&testCase, 0, sizeof(testCase));

        strncpy(testCase.path, pFilename, sizeof(testCase.path) - 1);
        dirname(testCase.path);

        ret = PAS_ParseTestCase(pJRoot, &testCase);
        assert(!ret);

        cJSON *pCase = cJSON_CreateObject();
        cJSON_AddItemToArray(pReport, pCase);

        cJSON_AddItemToObject(pCase, "name",
                              cJSON_CreateString(ppCaseList[i]));

        CaseResult_t caseResult = CST_TestCase(pCtx, &testCase, pCase);

        if (pCtx->pEngineTest)
        {
            EngineTestDestroy(pCtx, pCtx->pEngineTest);
            pCtx->pEngineTest = NULL;
        }
        HalClose(pCtx->hHal);
        pCtx->hHal = NULL;

        if (caseResult == CASE_PASS) {
            passCount++;
        } else if (caseResult == CASE_INVALID) {
            invalidCount++;
        } else if (caseResult == CASE_FAILED) {
            failedCount++;
        }

        TRACE(CASE_TEST_INF, "CASE [%03d/%03d]: %s = %s \n", i + 1, caseCount,
              basename(ppCaseList[i]), RES_GetResultDescription(caseResult));
        printf("CASE %d/%d:\033[32mPASS %d, \033[33mINVALID %d, \033[31mFAILED %d \033[0m\n",
                 i + 1, caseCount,passCount, invalidCount, failedCount);

        cJSON_AddItemToObject(
            pCase, "verify",
            cJSON_CreateString(RES_GetResultDescription(caseResult)));
    }

    char totalDesc[100] = "";

    snprintf(totalDesc, sizeof(totalDesc), "PASS %d/%d: INVALID %d, FAILED %d",
             passCount, caseCount, invalidCount, failedCount);

    TRACE(CASE_TEST_INF, "%s \n", totalDesc);

    cJSON_AddStringToObject(pReport, "total", totalDesc);

    RES_OutputResult(pReport);

    RES_OutputFailedList(pReport);

    if ( repeatTimes < pCtx->testRepeatTimes ){

        passCount = 0;
        invalidCount = 0;
        failedCount = 0;

        repeatTimes++ ;
        goto testStart ;        /* repeat the list */
    }

    cJSON_Delete(pReport);

    CL_FreeCaseList(ppCaseList, caseCount);

    return ret;
}

static CaseResult_t CST_TestCase(TestCtx_t *pCtx, TestCase_t *pTestCase,
                                 cJSON *pCase) {
    int ret = 0;

    if (pTestCase->type == TEST_TYPE_ENGINE) {
#if 0
        cJSON *pItem = cJSON_CreateObject();
        cJSON_AddItemToObject(pCase, "frames", pItem);
#else
        cJSON *pItem = cJSON_CreateObject();

        cJSON_AddItemToObject(pCase, "TotalCaseConfigs",
            cJSON_CreateNumber(pTestCase->configCount));

        pItem = cJSON_CreateArray();
        cJSON_AddItemToObject(pCase, "CaseConfigs", pItem);

#endif

        ENG_TestCaseParam_t param = {
            .pCtx = pCtx,
            .pTestCase = pTestCase,
            .pReport = pItem,
        };

        ret = ENG_TestCase(&param);
    } else if (pTestCase->type == TEST_TYPE_BM) {
        cJSON *pItem = cJSON_CreateArray();
        cJSON_AddItemToObject(pCase, "frames", pItem);

        BM_TestCaseParam_t param = {
            .pCtx = pCtx,
            .pTestCase = pTestCase,
            .pReport = pItem,
        };

        ret = BM_TestCase(&param);
    } else {
        printf("Invalid test type.\n");
    }

    if (ret == -ENFILE) {
        return CASE_INVALID;
    } else if (ret != 0) {
        return CASE_FAILED;
    } else {
        return CASE_PASS;
    }

    return CASE_UNKNOWN;
}
