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

#include "case_test.h"
#include "test_base.h"
#include "parse.h"
#include "isp_version.h"


#if defined(HAL_CMODEL)
#include <cmodel_wrapper/cmodel_wrapper.h>
#endif

CREATE_TRACER(ISP_TEST_INFO, "ISP-TEST: ", INFO, 1);
CREATE_TRACER(ISP_TEST_ERROR, "ISP-TEST: ", ERROR, 1);

static int init(TestCtx_t *);
static int fini(TestCtx_t *);
static void loadGlobalSetting(char *, TestSetting_t *);
static void testBenchVersion();

#define TEST_TYPE_BARE_METAL 1
#define TEST_TYPE_CAMENGINE  2

/******************************************************************************
 * usage()
 *****************************************************************************/
void usage(void)
{
    printf("Only one param supported, default for camEngine cases:\n");
    printf("    1 : run bare metal cases\n");
    printf("    2 : run software camEngine cases\n");
}

int main(int argc, char *argv[])
{
    int test_type;
    int repeat_times = 1;

    testBenchVersion();

    if ((argc > 3))
    {
        usage();
        exit(1);
    }

    if (argc == 1)
    {
        test_type = TEST_TYPE_CAMENGINE;
        printf("Using default configuration, preparing camEngine test\n");

    }

    if (argc == 2){
        test_type = atoi(argv[1]);
        switch(test_type)
        {
            case TEST_TYPE_BARE_METAL:
                test_type = TEST_TYPE_BARE_METAL;
                printf("Preparing bare metal test\n");
            break;
            case TEST_TYPE_CAMENGINE:
                test_type = TEST_TYPE_CAMENGINE;
                printf("Preparing camEngine test\n");
            break;
            default:
                usage();
                printf("Invalid input, exit\n");
                exit(1);
        }
    }

    if (argc == 3){
        repeat_times = atoi(argv[2]);
    }

    int ret = 0;

    TestCtx_t *pCtx = calloc(1, sizeof(TestCtx_t));
    assert(pCtx);
    memset(pCtx, 0, sizeof(TestCtx_t));

    pCtx->settingFileName = SETTING_FILE_NAME;
    pCtx->testRepeatTimes = repeat_times;

    ret = init(pCtx);
    assert(!ret);

    if(test_type == TEST_TYPE_BARE_METAL)
    {
        ret = CST_TestFromList(pCtx, BM_LIST_FILE);
        assert(!ret);
    }else{
        ret = CST_TestFromList(pCtx, ENG_LIST_FILE);
        assert(!ret);
    }

    ret = fini(pCtx);
    assert(!ret);

    if (pCtx) {
        free(pCtx);
        pCtx = NULL;
    }

    return ret;
}

static int init(TestCtx_t *pCtx)
{
    assert(pCtx);
#if 0
    pCtx->hHal = HalOpen();
    assert(pCtx->hHal);
#endif
    loadGlobalSetting(pCtx->settingFileName, &pCtx->settings);

    return 0;
}

static int fini(TestCtx_t *pCtx)
{
#if 0
    if (pCtx->pEngineTest)
    {
        EngineTestDestroy(pCtx, pCtx->pEngineTest);
        pCtx->pEngineTest = NULL;
    }
#endif
    if (pCtx->pBmTest)
    {
        BMTestDestroy(pCtx, pCtx->pBmTest);
        pCtx->pBmTest = NULL;
    }
#if 0
    HalClose(pCtx->hHal);
    pCtx->hHal = NULL;
#endif

    return 0;
}

static void loadGlobalSetting(char *fileName, TestSetting_t *pSetting)
{

    //pSetting->som.enable = 1;
    PAS_ParseGlobalSetting(fileName, pSetting);
}

static void testBenchVersion()
{
    printf("--------------------------------------\n");
    printf("ISP model: %s\n", ISP_VERSION);
    printf("Build time: %s, %s\n", __DATE__, __TIME__);
    printf("--------------------------------------\n");
}


