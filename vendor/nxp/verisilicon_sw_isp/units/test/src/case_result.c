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

#include "case_result.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_LEN 256

#define FILENAME_TEST_RESULT "test_result"
#define FILENAME_TEST_RESULT_EXT ".json"

#define FILENAME_FAILED "failed_list"
#define FILENAME_FAILED_EXT ".txt"

static void getTimestampDesc(char *, int);

const char *RES_GetResultDescription(CaseResult_t caseResult) {
    if (caseResult == CASE_INVALID) {
        return "INVALID";
    } else if (caseResult == CASE_PASS) {
        return "PASS";
    } else if (caseResult == CASE_FAILED) {
        return "FAILED";
    } else {
        return "UNKNOWN";
    }
}

void RES_OutputResult(cJSON *pReport) {
    char timestampDesc[30] = {0};

    getTimestampDesc(timestampDesc, sizeof(timestampDesc));

    char file[MAX_LEN] = {0};

    sprintf(file, FILENAME_TEST_RESULT "_%s" FILENAME_TEST_RESULT_EXT,
         timestampDesc);

    FILE *pf = fopen(file, "w");
    if (pf) {
        char *pDesc = cJSON_Print(pReport);

        fwrite(pDesc, strlen(pDesc), 1, pf);

        free(pDesc);

        fclose(pf);
    }
}

void RES_OutputFailedList(cJSON *pReport) {
    char timestampDesc[30] = {0};

    getTimestampDesc(timestampDesc, sizeof(timestampDesc));

    char file[MAX_LEN] = {0};

    sprintf(file, FILENAME_FAILED "_%s" FILENAME_FAILED_EXT, timestampDesc);

    FILE *pf = fopen(file, "w");
    if (pf) {
        int size = cJSON_GetArraySize(pReport);

        char desc[MAX_LEN] = "";

        for (int i = 0; i < size; i++) {
            cJSON *pItem = cJSON_GetArrayItem(pReport, i);

            cJSON *pVerify = cJSON_GetObjectItem(pItem, "verify");
            if (pVerify) {
                if (strcmp("FAILED", cJSON_GetObjectString(pVerify))) {
                    continue;
                }

                cJSON *pName = cJSON_GetObjectItem(pItem, "name");
                if (pName) {
                    snprintf(desc, sizeof(desc), "%s\n",
                             cJSON_GetObjectString(pName));
                    fwrite(desc, strlen(desc), 1, pf);
                }
            }
        }

        fclose(pf);
    }
}

static void getTimestampDesc(char *pDesc, int length) {
    time_t timer;

    time(&timer);

    struct tm *pTm = localtime(&timer);

    strftime(pDesc, length, "%Y%m%d%H%M%S", pTm);
}
