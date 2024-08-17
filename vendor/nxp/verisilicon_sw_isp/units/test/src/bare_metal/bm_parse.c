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
#include "test_base.h"
#include "bare_metal.h"

#include <assert.h>
#include <stdarg.h>

#define DATA_EXTENSION ".txt"

static int PAS_ParseConfigIo(cJSON *, const char *, BMTestConfigIO_t *);
static int PAS_ParseConfigIos(cJSON *, const char *, BMTestConfigIO_t *);
static int PAS_ParseFiles(cJSON *, const char *, BMTestConfigIO_t *);

static inline int _snprintf(char *dst, size_t len,
                size_t num, const char *src, ...) {
    size_t index = 0, i;
    va_list args;
    va_start(args, src);
    for (i = 0; i < num && index < len; ++i) {
        snprintf(&dst[index], len - index, src, args);
        index = strlen(dst);
        src = va_arg(args, const char *);
    }
    va_end(args);
    return 0;
}

int BMPAS_Parse(cJSON *pJObject, TestCase_t *pTestCase) {
    assert(pJObject);
    assert(pTestCase);

    char casePath[MAX_LEN] = {0};
    _snprintf(casePath, sizeof(casePath), 3, pTestCase->path, "/data/",
            pTestCase->golden_sub_path);

    for (int i = 0; i < cJSON_GetArraySize(pJObject) && i < CONFIG_MAX; i++) {
        cJSON *pChildItem = cJSON_GetArrayItem(pJObject, i);

        BMTestConfig_t *pTestConfig = &pTestCase->config[i].bm;

        cJSON *pJItem = NULL;

        pJItem = cJSON_GetObjectItem(pChildItem, "input");
        if (pJItem) {
            PAS_ParseConfigIo(pJItem, casePath, &pTestConfig->input);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "output");
        if (pJItem) {
            if (cJSON_GetObjectItem(pJItem, "file")) {
                PAS_ParseConfigIo(pJItem, casePath, pTestConfig->output);
            } else {
                PAS_ParseConfigIos(pJItem, casePath, pTestConfig->output);
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "register");
        if (pJItem) {
            pJItem = cJSON_GetObjectItem(pJItem, "file");
            if (pJItem) {
                _snprintf(pTestConfig->registerFile, MAX_LEN, 3, casePath,
                        cJSON_GetObjectString(cJSON_GetArrayItem(pJItem, 0)),
                        DATA_EXTENSION);
            }
        }

        pTestCase->configCount++;
    }

    return 0;
}

static int PAS_ParseConfigIo(cJSON *pJObject, const char *pPath,
                             BMTestConfigIO_t *pConfigIo) {
    cJSON *pJItem = NULL;

    pJItem = cJSON_GetObjectItem(pJObject, "file");
    if (pJItem) {
        return PAS_ParseFiles(pJItem, pPath, pConfigIo);
    }

    return -1;
}

static int PAS_ParseConfigIos(cJSON *pJObject, const char *pPath,
                              BMTestConfigIO_t *pConfigIo) {
    int ret = 0;

    for (int i = 0; i < BM_PATH_MAX; i++) {
        cJSON *pJItem = NULL;

        pJItem = cJSON_GetArrayItem(pJObject, i);
        if (pJItem) {
            BMTestConfigIO_t configIo = {{{0}}};

            ret = PAS_ParseConfigIo(pJItem, pPath, &configIo);

#if 0
            int tmp=strcmp(configIo.filename[BM_CHANNEL_Y],{0});
            if((configIo.filename[BM_CHANNEL_Y] == {0})
             &&(configIo.filename[BM_CHANNEL_RAW0] == {0})
             &&(configIo.filename[BM_CHANNEL_RAW1] == {0})){
               continue;
            }
#endif

            if (!ret) {
                BMPath_t path = BM_PATH_MAIN;
                //BMPath_t path = BM_PATH_SELF1;
#ifdef BM_ISP_MIV2
                if (strstr(configIo.filename[BM_CHANNEL_Y], "_sp1_"))
                  {
                  path = BM_PATH_SELF1;
                }

                if (strstr(configIo.filename[BM_CHANNEL_Y], "_sp2_")
                  ||strstr(configIo.filename[BM_CHANNEL_RAW0],"_sp2_")){
                    path = BM_PATH_SELF2;
                }
                if(strstr(configIo.filename[BM_CHANNEL_Y],"_mi_mcm_")
                 ||strstr(configIo.filename[BM_CHANNEL_CB],"_mi_mcm_")){
                    path = BM_PATH_MCM;
                 }

#endif
#ifdef BM_VSE_MI
                if (strstr(configIo.filename[BM_CHANNEL_Y], "_mi1_")){
                    path = BM_PATH_SELF1;
                }

                if (strstr(configIo.filename[BM_CHANNEL_Y], "_mi2_")){
                    path = BM_PATH_SELF2;
                }
#endif

                //BMTestConfigIO_t xconfigIo = *(pConfigIo + path);
                // for(i=0;i<BM_CHANNEL_MAX;i++){
                //   if(configIo.filename[i] != NULL)
                //    memcpy(xconfigIo.filename[i], configIo.filename[i], MAX_LEN);
                //    //xconfigIo.filename[i] =configIo.filename[i];
                // }
                //*(pConfigIo + path) = xconfigIo ;

                *(pConfigIo + path) = configIo ;

            }
        }
    }

    return 0;
}

static int PAS_ParseFiles(cJSON *pJObject, const char *pPath,
                          BMTestConfigIO_t *pConfigIo) {
    for (int i = 0; i < cJSON_GetArraySize(pJObject) && i < BM_CHANNEL_MAX;
         i++) {
#ifdef BM_ISP_MIV2
            int j = 0;
            char *tmp = cJSON_GetObjectString(cJSON_GetArrayItem(pJObject, i));

            if (strstr(tmp, "_y_")
              ||strstr(tmp, "_mcm_raw0_")
              ||strstr(tmp, "_fe_cfg_"))
              j=0;
            else if(strstr(tmp, "_cb_")
                  ||strstr(tmp, "_mcm_raw1_"))
              j=1;
            else if(strstr(tmp, "_cr_"))
              j=2;
            else if(strstr(tmp, "_sp2_dma_raw_")

                  ||strstr(tmp,"_data_raw_"))
              j=3;
            else if(strstr(tmp, "load_mem_mcm")
                  ||strstr(tmp, "_jdp_"))

              j=4;
            else
            return -1;

        snprintf(pConfigIo->filename[j], MAX_LEN,
                 "%s/%s" DATA_EXTENSION, pPath,
                 tmp);
#else
      snprintf(pConfigIo->filename[i], MAX_LEN,
                 "%s/%s" DATA_EXTENSION, pPath,
                 cJSON_GetObjectString(cJSON_GetArrayItem(pJObject, i)));
#endif

    }

    return 0;
}
