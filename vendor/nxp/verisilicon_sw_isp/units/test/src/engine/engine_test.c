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

#include <ebase/builtins.h>
#include <ebase/trace.h>
#include <ebase/types.h>

#include "engine_test_reg_init.h"
#include "test_base.h"
#include <cam_calibdb.h>
#include <cam_calibdb/cam_calibdb_api.h>
#include <cam_engine.h>

#include <cameric_drv.h>
#include "isp_version.h"

CREATE_TRACER(ISP_ENGINE_TEST_INFO, "ISP-ENGINE-TEST: ", INFO, 1);
CREATE_TRACER(ISP_ENGINE_TEST_ERROR, "ISP-ENGINE-TEST: ", ERROR, 1);

static void cbCamEngine(CamEngineCmdId_t cmdId, RESULT result,
                        const void *pUserContext) {
    EngineTest_t *pEngineTest = (EngineTest_t *)pUserContext;

    TRACE(ISP_ENGINE_TEST_INFO, "%s (enter)\n", __func__);

    if (!pUserContext) {
        goto exit;
    }

    switch (cmdId) {
    case CAM_ENGINE_CMD_START:
        TRACE(ISP_ENGINE_TEST_INFO, "%s: CAM_ENGINE_CMD_START completed\n",
              __func__);
        osEventSignal(&pEngineTest->eventEngineStarted);
        break;

    case CAM_ENGINE_CMD_STOP:
        TRACE(ISP_ENGINE_TEST_INFO, "%s: CAM_ENGINE_CMD_STOP completed\n",
              __func__);
        osEventSignal(&pEngineTest->eventEngineStop);
        break;

    case CAM_ENGINE_CMD_STOP_STREAMING:
        TRACE(ISP_ENGINE_TEST_INFO,
              "%s: CAM_ENGINE_CMD_STOP_STREAMING completed\n", __func__);
        osEventSignal(&pEngineTest->eventEngineCaptureDone);
        break;

    case CAM_ENGINE_CMD_START_STREAMING:
        TRACE(ISP_ENGINE_TEST_INFO,
              "%s: CAM_ENGINE_CMD_START_STREAMING completed\n", __func__);
        osEventSignal(&pEngineTest->eventStreamStarted);
        break;

    case CAM_ENGINE_CMD_HW_DMA_FINISHED:
        TRACE(ISP_ENGINE_TEST_INFO,
              "%s: CAM_ENGINE_CMD_HW_DMA_FINISHED completed\n", __func__);
        osEventSignal(&pEngineTest->eventDmaFinished);
        break;

    default:
        TRACE(ISP_ENGINE_TEST_ERROR, "%s: Invalid command (%d)\n", __func__,
              cmdId);
        break;
    }

exit:
    TRACE(ISP_ENGINE_TEST_INFO, "%s (exit)\n", __func__);

    return;
}

static void cbBuffer(CamEnginePathType_t path, MediaBuffer_t *pMediaBuffer,
                     void *pBufferCbCtx) {
    EngineTest_t *pEngineTest = (EngineTest_t *)pBufferCbCtx;

    TRACE(ISP_ENGINE_TEST_INFO, "%s (enter)\n", __func__);

    TRACE(ISP_ENGINE_TEST_INFO,
          "Path type: %d, pMediaBuffer: %p, pBufferCbCtx: %p.\n", path,
          (void *)pMediaBuffer, pBufferCbCtx);

    if (!pBufferCbCtx) {
        goto exit;
    }

    somCtrlStoreBuffer(pEngineTest->hSomCtrl, pMediaBuffer);
exit:
    TRACE(ISP_ENGINE_TEST_INFO, "%s (exit)\n", __func__);

    return;
}

static void cbSomCtrl(somCtrlCmdID_t cmdId, RESULT result,
                      somCtrlCmdParams_t *pParams,
                      somCtrlCompletionInfo_t *pInfo, void *pUserContext) {
    EngineTest_t *pEngineTest;

    TRACE(ISP_ENGINE_TEST_INFO, "%s (enter)\n", __func__);

    if (!pUserContext) {
        goto exit;
    }

    pEngineTest = (EngineTest_t *)pUserContext;

    TRACE(ISP_ENGINE_TEST_INFO, "%s CMD:%d, result:\n", __func__, cmdId,
          result);

    switch (cmdId) {
    case SOM_CTRL_CMD_START:
        if (RET_PENDING == result) {
            TRACE(ISP_ENGINE_TEST_INFO, "%s eventSomStart raised\n", __func__);
            osEventSignal(&pEngineTest->eventSomStarted);
        } else {
            TRACE(ISP_ENGINE_TEST_INFO, "%s eventSomFinished raised\n",
                  __func__);
            osEventSignal(&pEngineTest->eventSomFinished);
        }
        break;

    case SOM_CTRL_CMD_STOP:
        osEventSignal(&pEngineTest->eventSomStop);
        break;

    default:
        break;
    }

exit:
    TRACE(ISP_ENGINE_TEST_INFO, "%s (exit)\n", __func__);

    return;
}

static inline void ListInitForDma(List *p_list) {
    ASSERT(p_list != NULL);
    p_list->p_next = malloc(sizeof(List));
    if (p_list->p_next == NULL) {
        TRACE(ISP_ENGINE_TEST_INFO, "%s (allocating control context failed)\n",
              __func__);
    }
    MEMSET(p_list->p_next, 0, sizeof(List));
}

int ENG_Test() {
    int ret = 0;

    return ret;
}

void InitCalibDbForDma(CamCalibDbHandle_t *hCalibDb) {
    CamCalibDbContext_t *pCamCalibDbCtx;

    TRACE(ISP_ENGINE_TEST_INFO, "%s (enter)\n", __func__);

    /* pre-check */
    if (hCalibDb == NULL) {
    }

    /* allocate control context */
    pCamCalibDbCtx = malloc(sizeof(CamCalibDbContext_t));
    if (pCamCalibDbCtx == NULL) {
        TRACE(ISP_ENGINE_TEST_INFO, "%s (allocating control context failed)\n",
              __func__);
    }
    MEMSET(pCamCalibDbCtx, 0, sizeof(CamCalibDbContext_t));

    ListInitForDma(&pCamCalibDbCtx->resolution);
    ListInitForDma(&pCamCalibDbCtx->awb_global);

    /* Initial AEC */
    /*
    pCamCalibDbCtx->pAecGlobal = malloc( sizeof(CamCalibAecGlobal_t) );
    if ( pCamCalibDbCtx->pAecGlobal == NULL )
    {
        TRACE( ISP_TEST_INFO, "%s (allocating control context failed)\n",
    __func__ ); result =  RET_OUTOFMEM;
    }
    MEMSET( pCamCalibDbCtx->pAecGlobal, 0, sizeof(CamCalibAecGlobal_t) );

    pCamCalibDbCtx->pAecGlobal->SetPoint = 80;
    pCamCalibDbCtx->pAecGlobal->ClmTolerance = 20;
    pCamCalibDbCtx->pAecGlobal->DampOver = 0.2;
    pCamCalibDbCtx->pAecGlobal->DampUnder = 0.3;
    pCamCalibDbCtx->pAecGlobal->AfpsMaxGain = 16;
    */
    /* End of AEC */

    ListInitForDma(&pCamCalibDbCtx->ecm_profile);
    ListInitForDma(&pCamCalibDbCtx->illumination);
    ListInitForDma(&pCamCalibDbCtx->lsc_profile);
    ListInitForDma(&pCamCalibDbCtx->cc_profile);
    ListInitForDma(&pCamCalibDbCtx->cac_profile);
    ListInitForDma(&pCamCalibDbCtx->dpf_profile);
    ListInitForDma(&pCamCalibDbCtx->dpcc_profile);

    *hCalibDb = (CamCalibDbHandle_t)pCamCalibDbCtx;

    // check for invalid parameter handling

    // result = CamCalibDbRelease( &hCalibDb );
    // TEST_ASSERT_EQUAL_INT( RET_SUCCESS, result );

    TRACE(ISP_ENGINE_TEST_INFO, "%s (exit)\n", __func__);
}

/******************************************************************************
 * Degamma Curve init Added by LYX
 ******************************************************************************/
void InitDegammaForDma(CamEngineContext_t *pCamEngineCtx,
                       bool_t init_gamma_enable) {
    CamerIcDrvContext_t *pCamerIcDrvCtx =
        (CamerIcDrvContext_t *)(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);

#ifdef MRV_GAMMA_IN_VERSION
    pCamerIcDrvCtx->ic_dev.deg.enable = init_gamma_enable;
#endif
}

int EngineTestCreate(TestCtx_t *pCtx, EngineTest_t **ppEngineTest) {
    CamEngineInstanceConfig_t insConfig;
    EngineTest_t *pEngineTest;

    RESULT result;
    int ret = 0;

    if (!pCtx || !ppEngineTest) {
        return -EINVAL;
    }

    TRACE(ISP_ENGINE_TEST_INFO, "%s (enter)\n", __func__);

    pEngineTest = malloc(sizeof(EngineTest_t));
    if (!pEngineTest) {
        ret = -ENOMEM;
        goto err;
    }

#if defined(HAL_CMODEL)

    /* Hw init for Cmodel*/
    // Nano for 1
#ifdef ISP_NANO
    HalCmHwInit(pCtx->hHal, 1, NULL);
#else
    HalCmHwInit(pCtx->hHal, 0, NULL);
#endif
#else
    // Load default registers
    FpgaResetControl(pCtx->hHal, 1);
    FpgaResetControl(pCtx->hHal, 0);

#endif

    EngineTestInitialRegisterFile(pCtx->hHal, ENGINE_DUMP_ISP_REGS_INIT_SETTINGS);

    /* Setup engine. */
    osEventInit(&pEngineTest->eventEngineStarted, 1, 0);
    osEventInit(&pEngineTest->eventEngineStop, 1, 0);
    osEventInit(&pEngineTest->eventEngineCaptureDone, 1, 0);

    osEventInit(&pEngineTest->eventStreamStarted, 1, 0);
    osEventInit(&pEngineTest->eventDmaFinished, 1, 0);

    /* create and initialize the cam-engine instance */
    insConfig.maxPendingCommands = 4;
    insConfig.isSystem3D = false;
    insConfig.cbCompletion = cbCamEngine;
    insConfig.pUserCbCtx = (void *)pEngineTest;
    insConfig.hHal = pCtx->hHal;
    insConfig.hCamEngine = NULL;

    result = CamEngineInit(&insConfig);
    pEngineTest->hCamEngine = insConfig.hCamEngine;

    CamEngineRegisterBufferCb(pEngineTest->hCamEngine, cbBuffer, pEngineTest);

    // Initialize Reg by init reg value.
    CamEngineTestInitLsc(pEngineTest->hCamEngine, 0);
    CamEngineTestInitDpcc(pEngineTest->hCamEngine, 0);

    {
        somCtrlConfig_t somConfig;

        memset(&somConfig, 0, sizeof(somCtrlConfig_t));

        somConfig.MaxPendingCommands = 20;
        somConfig.MaxBuffers = 10;
        somConfig.somCbCompletion = cbSomCtrl;
        somConfig.pUserContext = (void *)pEngineTest;
        somConfig.HalHandle = pCtx->hHal;
        somConfig.somCtrlHandle = NULL;

        result = somCtrlInit(&somConfig);
        if ((result != RET_SUCCESS) || !somConfig.somCtrlHandle) {
            TRACE(ISP_ENGINE_TEST_ERROR, "Failed to initialize SomCtrll: %d.\n",
                  result);
            ret = -ENODEV;
            goto err;
        }

        pEngineTest->hSomCtrl = somConfig.somCtrlHandle;
    }

    *ppEngineTest = pEngineTest;
    pCtx->pEngineTest = pEngineTest;

    TRACE(ISP_ENGINE_TEST_INFO, "%s (exit)\n", __func__);

    return 0;

err:
    if (pEngineTest->hSomCtrl) {
        somCtrlShutDown(pEngineTest->hSomCtrl);
        pEngineTest->hSomCtrl = NULL;
    }

    if (pEngineTest->hCamEngine) {
        CamEngineShutDown(pEngineTest->hCamEngine);
        pEngineTest->hCamEngine = NULL;
    }

    if (pEngineTest) {
        osEventDestroy(&pEngineTest->eventEngineStarted);
        osEventDestroy(&pEngineTest->eventEngineStop);
        osEventDestroy(&pEngineTest->eventEngineCaptureDone);

        osEventDestroy(&pEngineTest->eventStreamStarted);
        osEventDestroy(&pEngineTest->eventDmaFinished);
        free(pEngineTest);
    }

    TRACE(ISP_ENGINE_TEST_INFO, "%s (exit)\n", __func__);

    return ret;
}

void EngineTestDestroy(TestCtx_t *pCtx, EngineTest_t *pEngineTest) {
    if (!pEngineTest) {
        return;
    }

    {
        somCtrlStop(pEngineTest->hSomCtrl);
        osEventWait(&pEngineTest->eventSomStop);

        osEventDestroy(&pEngineTest->eventSomFinished);
        osEventDestroy(&pEngineTest->eventSomStop);
        osEventDestroy(&pEngineTest->eventSomStarted);

        somCtrlShutDown(pEngineTest->hSomCtrl);
    }
    /* Shutdown enigne. */
    CamEngineShutDown(pEngineTest->hCamEngine);

    /* release test-context */
    osEventDestroy(&pEngineTest->eventEngineStarted);
    osEventDestroy(&pEngineTest->eventEngineStop);
    osEventDestroy(&pEngineTest->eventEngineCaptureDone);

    osEventDestroy(&pEngineTest->eventStreamStarted);
    osEventDestroy(&pEngineTest->eventDmaFinished);

    free(pEngineTest);

#ifdef HAL_CMODEL
    destroyCmodel();
#endif
    return;
}

void EngineTestInitialRegisterFile(HalHandle_t hHal,
                                   const char *iniRegFileName) {
    TRACE(ISP_ENGINE_TEST_INFO, "%s (Enter), Reg file name:%s\n", __func__,
          iniRegFileName);
    CamEngineTestRegisterInitial(hHal, iniRegFileName, 0);
    return;
}

int CamEngineTestRegisterInitial(HalHandle_t hHal, const char *registerFileName,
                                 int inibool) {
    if (!hHal) {
        TRACE(ISP_ENGINE_TEST_ERROR, "%s Input Null pointer!\n", __func__);
        return (-1);
    }

    if (NULL == registerFileName) {
        TRACE(ISP_ENGINE_TEST_ERROR,
              "%s error: initial register file is NULL! \n", __func__);
        return -1;
    }

    FILE *registerFile = NULL;
    registerFile = fopen(registerFileName, "rb");
    if (!registerFile) {
        TRACE(ISP_ENGINE_TEST_ERROR, "\n error: load register file error! \n",
              __func__);
        return -1;
    }

    char line[512] = {0};
    char str1[64] = {0};
    char str2[64] = {0};
    char str3[64] = {0};

    if (0 == inibool) {
        // forlsctable
        // init lsc table below

        // wdr table init below

        // fordpcctable init below
    }

    int k = 0;
    while (!feof(registerFile)) {
        k++;
        if (NULL == fgets(line, 512, registerFile)) {
        }
        int ll, num;
        ll = strlen(line);

        num = 0;
        for (int i = 0; i < ll; i++) {
            if (line[i] == ' ' || line[i] == '\t') {
                num++;
            }
        }

        sscanf(line, ("%[^ ] %[^ ] %[^\n]"), str1, str2, str3);

        int rw = atoi(str1);

        int reg_i = 0;
        ll = strlen(str2);
        for (int i = 0; str2[i] != '\0' || i < ll; i++) {
            if (str2[i] >= '0' && str2[i] <= '9')
                reg_i = reg_i * 16 + str2[i] - '0';
            else if (str2[i] <= 'f' && str2[i] >= 'a')
                reg_i = reg_i * 16 + str2[i] - 'a' + 10;
            else if (str2[i] <= 'F' && str2[i] >= 'A')
                reg_i = reg_i * 16 + str2[i] - 'A' + 10;
        }

        int reg_v = 0;
        ll = strlen(str3);
        for (int i = 0; str3[i] != '\0' || i < ll; i++) {
            if (str3[i] >= '0' && str3[i] <= '9')
                reg_v = reg_v * 16 + str3[i] - '0';
            else if (str3[i] <= 'f' && str3[i] >= 'a')
                reg_v = reg_v * 16 + str3[i] - 'a' + 10;
            else if (str3[i] <= 'F' && str3[i] >= 'A')
                reg_v = reg_v * 16 + str3[i] - 'A' + 10;
        }
        if (0 == inibool) {
            // HAL register write: reg_i <-- reg_v
            TRACE(ISP_ENGINE_TEST_ERROR,
                  "HAL_Reg_write, addr:0x%x, value:0x%x \n", reg_i, reg_v);
            HalWriteReg(hHal, reg_i, reg_v);

        } else {
            // if(!rw && register_array[rindex][0])
            if (!rw) {
                TRACE(ISP_ENGINE_TEST_ERROR,
                      "inibool = %d, HAL_Reg_write, addr:0x%x, value:0x%x \n",
                      inibool, reg_i, reg_v);
                HalWriteReg(hHal, reg_i, reg_v);
            }
        }
    }

    fclose(registerFile);
    return 0;
}

int CamEngineTestInitLsc(CamEngineHandle_t hCamEngine, int inibool) {
    int32_t ret = 0;
    uint32_t reg_addr = 0;
    uint32_t reg_val = 0;

    if (!hCamEngine) {
        TRACE(ISP_ENGINE_TEST_ERROR, "%s Input Null pointer!\n", __func__);
        return (-1);
    }
    if (!inibool) {
        TRACE(ISP_ENGINE_TEST_ERROR, "%s init bool is false!\n", __func__);
        return -1;
    }
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    HalHandle_t pHalCtx = NULL;
    CamIcGetHalCtx(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pHalCtx);

    TRACE(ISP_ENGINE_TEST_ERROR, "%s LSC Init table 0!\n", __func__);

    // set Table0
    reg_addr = 0x2200 + 0x64;
    reg_val = 0;
    HalWriteReg(pHalCtx, reg_addr, reg_val);

    ret = CamerIcIspLscSetLenseShadeCorrectionMatrix(
        pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, LscRDataTblInit,
        LscGRDataTblInit, LscGBDataTblInit, LscBDataTblInit);

    // set Table1
    TRACE(ISP_ENGINE_TEST_ERROR, "%s LSC Init table 1!\n", __func__);

    // set Table0
    reg_addr = 0x2200 + 0x64;
    reg_val = 1;
    HalWriteReg(pHalCtx, reg_addr, reg_val);

    ret = CamerIcIspLscSetLenseShadeCorrectionMatrix(
        pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, LscRDataTblInit,
        LscGRDataTblInit, LscGBDataTblInit, LscBDataTblInit);

    return ret;
}

int CamEngineTestInitDpcc(CamEngineHandle_t hCamEngine, int inibool) {

    int32_t ret = 0;
    if (!hCamEngine) {
        TRACE(ISP_ENGINE_TEST_ERROR, "%s Input Null pointer!\n", __func__);
        return (-1);
    }
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    HalHandle_t pHalCtx = NULL;
    CamIcGetHalCtx(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pHalCtx);

    TRACE(ISP_ENGINE_TEST_ERROR, "%s DPCC register init!\n", __func__);

    ret = CamerIcIspDpccSetStaticConfig(
        pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &DpccCfgInit);
    if (ret != RET_SUCCESS) {
        TRACE(ISP_ENGINE_TEST_ERROR,
              "%s: CamerIc Driver DPCC Configuration failed (%d)\n", __func__,
              ret);
        return (ret);
    }

    return ret;
}

#if defined(HAL_ALTERA)
int FpgaResetControl(HalHandle_t HalHandle, int is_reset) {
    int32_t ret = 0;

    if (!HalHandle) {
        TRACE(ISP_ENGINE_TEST_ERROR, "%s Input Null pointer!\n", __func__);
        return (-1);
    }

    TRACE(ISP_ENGINE_TEST_ERROR, "%s FPGA contrl, is_reset:%d!\n", __func__,
          is_reset);
#if 0
    if (is_reset == 0) {
        HalFpgaDereset(HalHandle);
    } else {
        HalFpgaReset(HalHandle);
    }
#endif
    return ret;
}
#endif
