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
#include <assert.h>

static CamEngineWbGains_t defaultWbGains = {1.0f, 1.0f, 1.0f, 1.0f};

static CamEngineCcMatrix_t defaultCcMatrix = {
    {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};

static CamEngineCcOffset_t defaultCcOffset = {0, 0, 0};

static CamEngineBlackLevel_t defaultBlvl = {0U, 0U, 0U, 0U};

static int PAS_PathConfig(cJSON *, CamEnginePathConfig_t *);

int ENGPAS_ParseEngineConfig(cJSON *pJObject,
                             CamEngineConfig_t *pCamEngineConfig) {
    assert(pJObject);
    assert(pCamEngineConfig);

    cJSON *pJItem = NULL;

    pJItem = cJSON_GetObjectItem(pJObject, "type");
    if (pJItem) {
        pCamEngineConfig->type = cJSON_GetObjectValue(pJItem);
    }

    if ( (pCamEngineConfig->type == CAM_ENGINE_CONFIG_IMAGE)
      || (pCamEngineConfig->type == CAM_ENGINE_CONFIG_TPG  ) ){
        struct CamEngineConfigImage_s *pImage = &pCamEngineConfig->data.image;

        pImage->pWbGains = &defaultWbGains;
        pImage->pCcMatrix = &defaultCcMatrix;
        pImage->pCcOffset = &defaultCcOffset;
        pImage->pBlvl = &defaultBlvl;
    }

    pJItem = cJSON_GetObjectItem(pJObject, "mainPathConfig");
    if (pJItem) {
        PAS_PathConfig(pJItem,
                       &pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_MAIN]);
    }

    pJItem = cJSON_GetObjectItem(pJObject, "selfPathConfig");
    if (pJItem) {
        PAS_PathConfig(pJItem,
                       &pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_SELF]);
    }


#ifdef ISP_MI_BP
	pJItem = cJSON_GetObjectItem(pJObject, "bayerPatternPathConfig");
#else
	pJItem = cJSON_GetObjectItem(pJObject, "self2PathConfig");
#endif

	if (pJItem) {
		PAS_PathConfig(pJItem,
					   &pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_SELF2_BP]);
	} else {
		pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_SELF2_BP].width = 0;
		pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_SELF2_BP].height = 0;
		pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_SELF2_BP].mode = CAMERIC_MI_DATAMODE_DISABLED;
		pCamEngineConfig->pathConfig[CAM_ENGINE_PATH_SELF2_BP].layout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
	}

    return 0;
}

static int PAS_PathConfig(cJSON *pJObject,
                          CamEnginePathConfig_t *pCamEnginePathConfig) {
    assert(pJObject);
    assert(pCamEnginePathConfig);

    cJSON *pJItem = NULL;

    pJItem = cJSON_GetObjectItem(pJObject, "width");
    if (pJItem) {
        pCamEnginePathConfig->width = cJSON_GetObjectValue(pJItem);
    }

    pJItem = cJSON_GetObjectItem(pJObject, "height");
    if (pJItem) {
        pCamEnginePathConfig->height = cJSON_GetObjectValue(pJItem);
    }

    pJItem = cJSON_GetObjectItem(pJObject, "mode");
    if (pJItem) {
        pCamEnginePathConfig->mode = cJSON_GetObjectValue(pJItem);
    }

    pJItem = cJSON_GetObjectItem(pJObject, "layout");
    if (pJItem) {
        pCamEnginePathConfig->layout = cJSON_GetObjectValue(pJItem);
    }
    pJItem = cJSON_GetObjectItem(pJObject, "alignMode");
    if (pJItem) {
        pCamEnginePathConfig->alignMode = cJSON_GetObjectValue(pJItem);
    }
    return 0;
}
