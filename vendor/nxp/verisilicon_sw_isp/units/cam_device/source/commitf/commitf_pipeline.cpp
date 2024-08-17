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

#include "cam_device_api.hpp"
#include "commitf_pipeline.hpp"
#include "cam_mapcaps.hpp"
#include "cam_common.hpp"
#include <isi/isi.h>
#include <isi/isi_iss.h>
#include <vvsensor.h>

using namespace camdev;

RESULT CitfPipeline::process(int ctrlId, Json::Value &jRequest, Json::Value &jResponse) {

    if (!pCitfHandle || !pCitfHandle->pOperation)
        return RET_NULL_POINTER;
    switch (ctrlId) {
    case ISPCORE_MODULE_PIPELINE_WARM_UP           : return warmUp(jRequest, jResponse);
    case ISPCORE_MODULE_PIPELINE_3A_LOCK_SET       : return lock3ASet(jRequest, jResponse);
    case ISPCORE_MODULE_PIPELINE_3A_LOCK_GET       : return lock3AGet(jRequest, jResponse);
    };

    return RET_FAILURE;
}

RESULT CitfPipeline::warmUp(Json::Value &jRequest, Json::Value &jResponse) {
    TRACE_CMD;

    sensor().checkValid();
    int32_t ret = RET_SUCCESS;

    if (sensor().state < CamStatus::Running) {
        ret = sensor().streamEnableSet(true);
    }
    jResponse[CITF_RET] = ret;

    return ret;
}

RESULT CitfPipeline::lock3ASet(Json::Value &jRequest, Json::Value &jResponse) {
    TRACE_CMD;

    int32_t ret  = RET_SUCCESS;
    CamEngineAwbLockStatus_t awbLock = CAM_ENGINE_AWB_UNLOCK;
    CamEngineAecLockStatus_t aeLock  = CAM_ENGINE_AEC_UNLOCK;

    if (jRequest[PIPELINE_3A_LOCK_STATUS].asInt() & CAM_ENGINE_LOCK_AWB) {
        awbLock = CAM_ENGINE_AWB_LOCK;
    }

    if (jRequest[PIPELINE_3A_LOCK_STATUS].asInt() & CAM_ENGINE_LOCK_AEC) {
        aeLock = CAM_ENGINE_AEC_LOCK;
    }

    ret |= pCitfHandle->pOperation->pEngine->awbLockSet(awbLock);

    ret |= pCitfHandle->pOperation->pEngine->aeLockSet(aeLock);

    return ret;
}

RESULT CitfPipeline::lock3AGet(Json::Value &jRequest, Json::Value &jResponse) {
    TRACE_CMD;

    int32_t ret  = RET_SUCCESS;
    CamEngineAwbLockStatus_t awbLock = CAM_ENGINE_AWB_UNLOCK;
    CamEngineAecLockStatus_t aeLock  = CAM_ENGINE_AEC_UNLOCK;
    CamEngineAfLockStatus_t  afLock  = CAM_ENGINE_AF_UNLOCK;

    ret |= pCitfHandle->pOperation->pEngine->awbLockGet(awbLock);

    ret |= pCitfHandle->pOperation->pEngine->aeLockGet(aeLock);

    jResponse[PIPELINE_3A_LOCK_STATUS] = (awbLock << 2) | (aeLock << 1) | (afLock << 0);

    return ret;
}