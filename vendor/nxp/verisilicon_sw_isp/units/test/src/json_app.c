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

#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

cJSON *JA_ReadFromFile(const char *pFilename) {
    assert(pFilename);

    cJSON *json = NULL;

    FILE *pF = fopen(pFilename, "r");
    if (!pF){
        printf("%s, open file error: %s\n ", __func__, pFilename);
    }
    assert(pF);

    fseek(pF, 0, SEEK_END);
    size_t len = ftell(pF);

    char *pData = (char *)calloc(1, len + 1);
    assert(pData);

    rewind(pF);

    size_t num = fread(pData, len, 1, pF);
    assert(num == 1);
    (void)num; //avoid compiler's complaint

    pData[len] = '\0';

    json = cJSON_Parse(pData);
    assert(json);

    free(pData);

    fclose(pF);

    return json;
}
