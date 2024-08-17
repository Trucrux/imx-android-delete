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
#include "json_app.h"
#include "eng_parse.h"
#include "engine.h"
#include "test_base.h"
#include <assert.h>

int PAS_ParseTestCase(cJSON *pJObject, TestCase_t *pTestCase) {
    assert(pJObject);
    assert(pTestCase);

    pJObject = cJSON_GetObjectItem(pJObject, "caseConfig");
    assert(pJObject);

    cJSON *pJItem = NULL;

    if ((pJItem = cJSON_GetObjectItem(pJObject, "name"))) {
        strncpy(pTestCase->name, cJSON_GetObjectString(pJItem), MAX_LEN - 1);
    }

    if ((pJItem = cJSON_GetObjectItem(pJObject, "path"))) {
        strncpy(pTestCase->golden_sub_path, cJSON_GetObjectString(pJItem),
                sizeof(pTestCase->golden_sub_path) - 1);
    }

    if ((pJItem = cJSON_GetObjectItem(pJObject, "ver"))) {
        strncpy(pTestCase->version, cJSON_GetObjectString(pJItem),
                sizeof(pTestCase->version) - 1);
    }

    if ((pJItem = cJSON_GetObjectItem(pJObject, "date"))) {
        strncpy(pTestCase->date, cJSON_GetObjectString(pJItem),
                sizeof(pTestCase->date) - 1);
    }

    if ((pJItem = cJSON_GetObjectItem(pJObject, "engineConfig"))) {
        pTestCase->type = TEST_TYPE_ENGINE;

        ENG_ParseConfigParam_t param = {
            .pJObject = pJItem,
            .pTestCase = pTestCase,
        };

        ENG_ParseConfig(&param);
    } else if ((pJItem = cJSON_GetObjectItem(pJObject, "bmConfig"))) {
        pTestCase->type = TEST_TYPE_BM;

        BM_ParseConfigParam_t param = {
            .pJObject = pJItem,
            .pTestCase = pTestCase,
        };

        BM_ParseConfig(&param);
    } else {
        pTestCase->type = TEST_TYPE_INVALID;
    }

    return 0;
}


int PAS_ParseGlobalSetting(char *fileName, TestSetting_t *pSetting){

    cJSON *pJRoot           = NULL;
    cJSON *pJGlobalSetting  = NULL;
    cJSON *pJSom            = NULL;
    cJSON *pJTest           = NULL;
    cJSON *pJitem           = NULL;

    pJRoot = JA_ReadFromFile(fileName);
    assert(pJRoot);

    pJGlobalSetting = cJSON_GetObjectItem(pJRoot, "globalSetting");
    if(pJGlobalSetting){

        pJSom = cJSON_GetObjectItem(pJGlobalSetting, "som");
        if (pJSom){
            pJitem = cJSON_GetObjectItem(pJSom, "enable");
            if (pJitem){
                pSetting->som.enable = cJSON_GetObjectValue(pJitem);
            }
        }

        pJTest = cJSON_GetObjectItem(pJGlobalSetting, "test");
        if (pJTest){
            pJitem = cJSON_GetObjectItem(pJTest, "generateGolden");
            if (pJitem){
                pSetting->testCfg.generateGolden = cJSON_GetObjectValue(pJitem);
            }

            pJitem = cJSON_GetObjectItem(pJTest, "verifyGolden");
            if (pJitem){
                pSetting->testCfg.verifyGolden = cJSON_GetObjectValue(pJitem);
            }

            pJitem = cJSON_GetObjectItem(pJTest, "goldenPath");
            if (pJitem){
                pSetting->testCfg.goldenPath = calloc(1, MAX_LEN);

                strncpy(pSetting->testCfg.goldenPath, cJSON_GetObjectString(pJitem), MAX_LEN-1);
            }
        }
    }

    return 0;
}

