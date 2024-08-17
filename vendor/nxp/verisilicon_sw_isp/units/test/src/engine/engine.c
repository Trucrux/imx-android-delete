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

#include "engine.h"
#include "cJSON.h"
#include "eng_parse.h"
#include "file.h"
#include "ic_util.h"
#include "test_base.h"

#include <assert.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <cam_engine/cam_engine_api.h>
#include <cam_engine/cam_engine_aaa_api.h>
#include <cam_engine/cam_engine_isp_api.h>
#include <cam_engine/cam_engine_jpe_api.h>
#include <cam_engine/cam_engine_mi_api.h>
#include <cam_engine/cam_engine_imgeffects_api.h>
#include <cam_engine/cam_engine_mi_api.h>
#include <cam_engine/cam_engine_simp_api.h>

#include <cameric_drv/cameric_isp_cac_drv_api.h>
#include <cameric_drv/cameric_isp_lsc_drv_api.h>
#include <cameric_drv/cameric_isp_dpf_drv_api.h>
#include <cameric_drv/cameric_isp_degamma_drv_api.h>
#include <cameric_drv/cameric_isp_wdr_drv_api.h>
#include <cameric_drv/cameric_isp_is_drv_api.h>
#include <cameric_drv/cameric_isp_cnr_drv_api.h>
#include <cameric_drv/cameric_isp_wdr2_drv_api.h>
#include <cameric_drv/cameric_isp_stitching_drv_api.h>
#include <cameric_drv/cameric_isp_ee_drv_api.h>
#include <cameric_drv/cameric_isp_afm_drv_api.h>
#include <cameric_drv/cameric_isp_2dnr_drv_api.h>
#include <cameric_drv/cameric_isp_vsm_drv_api.h>
#include <cameric_drv/cameric_isp_dmsc2_drv_api.h>
#include <cameric_drv/cameric_isp_green_equilibration_drv_api.h>

#include <cam_engine_drv.h>
#include <file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "isp_version.h"

#define MAX_LEN 256
#define TEST_RESULT "engine_test_result.json"

CREATE_TRACER(ENG_INF, "ENG-TEST: ", INFO, 1);
CREATE_TRACER(ENG_ERR, "ENG-TEST: ", ERROR, 1);

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

static CamerIcJpeConfig_t JpeConfig = {CAMERIC_JPE_MODE_LARGE_CONTINUOUS,
                                       CAMERIC_JPE_COMPRESSION_LEVEL_HIGH,
                                       CAMERIC_JPE_LUMINANCE_SCALE_DISABLE,
                                       CAMERIC_JPE_CHROMINANCE_SCALE_DISABLE,
                                       1920,
                                       1080};

static int fileCompare(const char *, const char *);
static void freeConfigResource(TestCase_t *);

static int ENG_TestConfigRotation   (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigDPCC       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigDPF        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigDegamma    (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigLSC        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigMGC        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigWDR        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigBLS        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigAWB        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigAEC        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigGammaOut   (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigHIST       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigIS         (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#ifdef ISP_RAWIS
static int ENG_TestConfigRawIS         (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
static int ENG_TestConfigIE         (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigCNR        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigSI         (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigCPROC      (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
//static int ENG_TestConfigJpeg       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#ifdef ISP_WDR_V2
static int ENG_TestConfigWDR2       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
#ifdef ISP_WDR_V3
static int ENG_TestConfigWDR3       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
static int ENG_TestConfigCmpd       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigTpg        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestClearConfig      (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
//static int ENG_TestConfigSensor     (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigStitching  (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigAfm        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#ifdef ISP_EE
static int ENG_TestConfigEe         (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
static int ENG_TestConfigCSM        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#ifdef ISP_3DNR
static int ENG_TestConfig3DNR       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
static int ENG_TestConfigAEV2       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfig2DNR       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigDual(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigVsm(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#ifdef ISP_DEC
static int ENG_TestConfigDec        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif

#ifdef ISP_GCMONO
static int ENG_TestConfigGcmono     (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
#ifdef ISP_RGBGC
static int ENG_TestConfigRgbGc         (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif

#ifdef ISP_DEMOSAIC2
static int ENG_TestConfigDemosaic2 (EngineTest_t * pEngineTest,EngineTestConfig_t * pTestConfig);
static int ENG_TestConfigFlt(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);

#else
static int ENG_TestConfigDemosaic   (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
static int ENG_TestConfigFLT        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif
static int ENG_TestConfigCAC        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);

#ifdef ISP_CA
static int ENG_TestConfigCa        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif

#ifdef ISP_GREENEQUILIBRATE
static int ENG_TestConfigGreenEquilibration (EngineTest_t * pEngineTest,EngineTestConfig_t * pTestConfig);
#endif

#ifdef ISP8000NANO_BASE
static int ENG_TestConfigNano       (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);
#endif

static int ENG_TestConfigDitalGain(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);

static int ENG_countEnablePath(EngineTest_t *pEngineTest,  CamEngineConfig_t *pCamEngineConfig);
static int ENG_TestConfigModules(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig);

int ENG_ParseConfig(ENG_ParseConfigParam_t *pParam) {
    assert(pParam);

    return ENGPAS_Parse(pParam->pJObject, pParam->pTestCase);
}

int ENG_TestCase(ENG_TestCaseParam_t *pParam) {
    assert(pParam);

    int ret = 0;
    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    int total_frame;
    int current_frame = 0;
    //Todo: multi frame support
    int total_config;
    int current_config = 0;

    char imageFileName[MAX_LEN];
    char imageFileName1[MAX_LEN];
    char imageFileName2[MAX_LEN];

    char regDumpName[MAX_LEN];
    char bufDumpName[MAX_LEN];



    EngineTestConfig_t      *pTestConfig;
    EngineTest_t            *pEngineTest = pCtx->pEngineTest;
    CamEngineConfig_t       *pEngineConfig;

    cJSON *pReport          = NULL;

    cJSON *pCaseConfigItem  = NULL;
    cJSON *pFrameList       = NULL;
    cJSON *pFrameItem       = NULL;

    HalHandle_t hHal        = NULL;
    CamEngineHandle_t       hCamEngine;
    CamEngineContext_t     *pCamEngineCtx = NULL;
    CamerIcDrvHandle_t      hCamerIcDrv;
    TestCase_t             *pTestCase;

    PicBufMetaData_t        picBuf = {0};
    PicBufMetaData_t        picBufl = {0};
    PicBufMetaData_t        picBufs = {0};
    PicBufMetaData_t        picBufvs = {0};

    if (!pEngineTest) {
        EngineTestCreate(pCtx, &pEngineTest);
    }

    pTestCase       = pParam->pTestCase;
    total_config    = pTestCase->configCount;

    pReport         = pParam->pReport;
    assert(pReport);

configStart:

    TRACE(ENG_ERR, "Total case config:%d, current case config:%d\n", total_config, current_config + 1);

    pTestConfig     = &pTestCase->config[ current_config ].engine;
    pEngineConfig   = &pTestConfig->engineConfig;

    total_frame     = pTestConfig->frameNum;
    current_frame   = 0;

    pCaseConfigItem = cJSON_CreateObject();
    cJSON_AddItemToArray(pReport, pCaseConfigItem);

    cJSON_AddItemToObject(pCaseConfigItem, "CaseConfigCnt",
                          cJSON_CreateNumber(current_config));

    cJSON_AddItemToObject(pCaseConfigItem, "TotalFrames",
                          cJSON_CreateNumber(total_frame));

    pFrameList = cJSON_CreateArray();
    cJSON_AddItemToObject(pCaseConfigItem, "FrameList", pFrameList);

    hCamEngine = pEngineTest->hCamEngine;
    pCamEngineCtx = (CamEngineContext_t *)hCamEngine;
    hCamerIcDrv = pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc;

    RESULT result = CamIcGetHalCtx(hCamerIcDrv, &hHal);
    assert(!result);

testStart:

    TRACE(ENG_ERR, "Total Frame:%d, current frame:%d\n", total_frame, current_frame + 1);
    assert(current_frame < total_frame);

    //Update per frame report list
    pFrameItem = cJSON_CreateObject();
    cJSON_AddItemToArray(pFrameList, pFrameItem);

    cJSON_AddItemToObject(pFrameItem, "frame_cnt",
                          cJSON_CreateNumber(current_frame));

    /* Load the data from file. */
    if ( (pEngineConfig->type == CAM_ENGINE_CONFIG_IMAGE)
        || (pEngineConfig->type == CAM_ENGINE_CONFIG_TPG) )
    {

        snprintf(imageFileName, MAX_LEN, "%s/%s", ENG_IMG_PATH,
                 &pTestConfig->images[current_frame * MAX_LEN]);
        TRACE(ENG_INF, "Load raw file: %s.\n", imageFileName);

        cJSON_AddItemToObject(pFrameItem, "input_image",
                              cJSON_CreateString(imageFileName));

        if(pTestConfig->enableStitching)
        {
            snprintf(imageFileName1, MAX_LEN, "%s/%s", ENG_IMG_PATH,
                     &pTestConfig->images[(1) * MAX_LEN]);
            TRACE(ENG_INF, "Load raw file: %s.\n", imageFileName1);

            cJSON_AddItemToObject(pFrameItem, "input_image",
                                  cJSON_CreateString(imageFileName1));

            snprintf(imageFileName2, MAX_LEN, "%s/%s", ENG_IMG_PATH,
                 &pTestConfig->images[(2) * MAX_LEN]);
            TRACE(ENG_INF, "Load raw file: %s.\n", imageFileName2);

            cJSON_AddItemToObject(pFrameItem, "input_image",
                                  cJSON_CreateString(imageFileName2));
        }

            //dump register to file
            snprintf(regDumpName, MAX_LEN, "%s.regdump.txt",
                &pTestConfig->outputFileNameNoSuffix[current_frame * MAX_LEN]);
            TRACE(ENG_INF, "Reg dump file: %s.\n", regDumpName);

            cJSON_AddItemToObject(pFrameItem, "reg_dump_filename",
                                  cJSON_CreateString(regDumpName));

            result = HalDynRegDumpOpen(hHal, regDumpName);
            assert(!result);

        //dump buffer data to file
        snprintf(bufDumpName, MAX_LEN, "%s.bufdump.dat",
            &pTestConfig->outputFileNameNoSuffix[current_frame * MAX_LEN]);
        TRACE(ENG_INF, "buf dump file: %s.\n", bufDumpName);

        cJSON_AddItemToObject(pFrameItem, "buf_dump_filename",
                              cJSON_CreateString(bufDumpName));

        result = CamEngineMiMpDumpBufCfg(hCamEngine, bufDumpName);
        assert(!result);

        ret = icoUtil_LoadPGMRaw(imageFileName, &picBuf);
        if (ret < 0) {
            TRACE(ENG_ERR, "Failed to load PGM raw image.\n");
            return ret;
        }

        if(pTestConfig->enableStitching)
        {
            ret = icoUtil_LoadPGMRaw(imageFileName, &picBufl);
            if (ret < 0) {
                TRACE(ENG_ERR, "Failed to load PGM raw image.\n");
                return ret;
            }

            ret = icoUtil_LoadPGMRaw(imageFileName1, &picBufs);
            if (ret < 0) {
                TRACE(ENG_ERR, "Failed to load PGM raw image.\n");
                return ret;
            }
            ret = icoUtil_LoadPGMRaw(imageFileName2, &picBufvs);
            if (ret < 0) {
                TRACE(ENG_ERR, "Failed to load PGM raw image.\n");
                return ret;
            }
        }

        struct CamEngineConfigImage_s *pImage = &pEngineConfig->data.image;

        if (pTestConfig->enableXtalk) {
            pImage->pCcMatrix = &pTestConfig->ccMatrix;
            pImage->pCcOffset = &pTestConfig->ccOffset;
        }
        pImage->fieldSelection = CAMERIC_ISP_FIELD_SELECTION_BOTH;
        pImage->is_lsb_align = 1; //pgm file all lsb aligned.

        switch (picBuf.Type) {
        case PIC_BUF_TYPE_RAW8:
        case PIC_BUF_TYPE_RAW10:
        case PIC_BUF_TYPE_RAW12:
        case PIC_BUF_TYPE_RAW16:
            pImage->pBuffer = picBuf.Data.raw.pData;
            pImage->width   = picBuf.Data.raw.PicWidthPixel;
            pImage->height  = picBuf.Data.raw.PicHeightPixel;
            if (picBuf.Type == PIC_BUF_TYPE_RAW8) {
                pImage->inputSelection = CAMERIC_ISP_INPUT_8BIT_EX;
            } else if (picBuf.Type == PIC_BUF_TYPE_RAW16 || picBuf.Type == PIC_BUF_TYPE_RAW12) {
                pImage->inputSelection = CAMERIC_ISP_INPUT_12BIT;
            } else {
                pImage->inputSelection = CAMERIC_ISP_INPUT_10BIT_EX;
            }

#ifdef HAL_CMODEL
#if 1
            // dump test
            int dump_size =
                picBuf.Data.raw.PicWidthPixel * picBuf.Data.raw.PicHeightPixel;
            HalCmDumpBuf((char *)picBuf.Data.raw.pData, 1000, "load");
            HalCmDumpBuf((char *)(picBuf.Data.raw.pData + dump_size - 2000),
                         1000, "load");


#endif
#endif

            break;
        default:
            TRACE(ENG_ERR, "Not supported buffer type: %d.\n", picBuf.Type);
            assert(!"Not supported buffer type");
            break;
        }

        pImage->type = picBuf.Type;
        pImage->layout = picBuf.Layout;

        //For tpg source switch
        pImage->tpg_enabled = pTestConfig->enableTpg;

#ifdef HAL_CMODEL
        HalCmInputInfoSetup(pImage->width, pImage->height);
#endif

#ifdef ISP8000NANO_BASE

        result = ENG_TestConfigNano(pEngineTest, pTestConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(0, result);

#endif
    }

    CamEngineState_t camStatus ;

    camStatus = CamEngineGetState(hCamEngine);
    if ( eCamEngineStateInitialize == camStatus ){

        /* start the cam-engine instance  */
        result = CamEngineStart(hCamEngine, pEngineConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_PENDING, result);

        /* wait for test-event <= completion of start command */
        result = osEventWait(&pEngineTest->eventEngineStarted);
        TEST_ASSERT_EQUAL_INT_RETURN(OSLAYER_OK, result);

        /* Added by LYX for DMA test case */
        /* Init CalibDb */
        InitCalibDbForDma(&pEngineTest->hCamCalibDb);
        pCamEngineCtx->hCamCalibDb = pEngineTest->hCamCalibDb;

        /* Init DeGamma Curve */
        InitDegammaForDma(pCamEngineCtx, pTestConfig->enableGamma);
    } else if ( eCamEngineStateRunning == camStatus ) {
            result = CamEngineUpdateIspResolution(hCamEngine, pEngineConfig);
            TEST_ASSERT_EQUAL_INT_RETURN(0, result);
    }

    /* Enable Jpeg. */
    if (pTestConfig->enableJpeg) {
        result = CamEngineEnableJpe(hCamEngine, &JpeConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    result = ENG_TestConfigModules(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    /* Start SomCtrl. */
    osEventInit(&pEngineTest->eventSomStarted, 1, 0);
    osEventInit(&pEngineTest->eventSomStop, 1, 0);
    osEventInit(&pEngineTest->eventSomFinished, 1, 0);

    char countPath = ENG_countEnablePath(pEngineTest, pEngineConfig);

    somCtrlCmdParams_t params = {
        .Start.szBaseFileName = &pTestConfig->outputFileNameNoSuffix[current_frame * MAX_LEN],
        .Start.NumOfFrames = countPath,
        .Start.NumSkipFrames = 0,   //pTestConfig->frameNum - 1,
        .Start.AverageFrames = 0,
#ifdef EMULATOR
        .Start.ForceRGBOut = BOOL_FALSE,
#else
        .Start.ForceRGBOut = BOOL_TRUE,
#endif
        .Start.ExtendName = BOOL_FALSE,
    };

    result = somCtrlStart(pEngineTest->hSomCtrl, &params);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_PENDING, result);

    if (OSLAYER_OK != osEventWait(&pEngineTest->eventSomStarted)) {
        TRACE(ENG_ERR, "%s (start snapshot output timed out)\n", __func__);
    }

    if (pEngineConfig->type == CAM_ENGINE_CONFIG_IMAGE) {
        /* To avoid the first picture be loaded twice */
        if ( ( 0 != current_config ) || ( 0 != current_frame ) )
        {
            result = CamEnginePreloadImage( pCamEngineCtx, pEngineConfig, pEngineConfig->data.image.is_lsb_align);
        }
    }



    /* Start capturing. */
    result = CamEngineStartStreaming(hCamEngine, pTestConfig->frameNum);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_PENDING, result);

    // wait for irq process
    osSleep(100);

    /* wait for test-event <= completion of stream Stream command */
    TEST_ASSERT_EQUAL_INT_RETURN(OSLAYER_OK, osEventWait(&pEngineTest->eventStreamStarted));


    /* SI. Can be configured while streaming. */
    result = ENG_TestConfigSI(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);



#ifdef HAL_CMODEL
    // CamEngineContext_t * pCamEngine = (CamEngineContext_t
    // *)pEngineTest->hCamEngine; CamerIcDrvHandle_t hCamerIcDrv =
    // pCamEngine->chain[CHAIN_LEFT].hCamerIc;

    // input buffer process
    // HalHandle_t hHal = NULL;
    // CamIcGetHalCtx(hCamerIcDrv,  &hHal);

    if ( !pTestConfig->enableTpg ){
        HalCmBufTransfer(hHal, HAL_CMODEL_BUF_TRANS_DMA);
    }

#ifdef ISP_GCMONO
    if( pTestConfig->gcmono_cxt.enabled )
    {
        result = HalCmGcmonoBufFill(hHal, pTestConfig->gcmono_cxt.gcmonoLut, CAMERIC_ISP_GCMONO_LUT_NUM);
        assert(RET_SUCCESS == result);
    }
#endif

#endif

    // define all verions support TPG
    //Update TPG enable choice here for FPGA timming.
    osSleep(100);
    if ( pTestConfig->enableTpg ){
        result = CamEngineEnableTpg( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

#ifndef HAL_CMODEL
    //FPGA system, delay for the DMA ready interrupt switch for TPG cases.

    if (!pTestConfig->enableTpg)
    {
        osSleep(100);

        /// send DMA ready interrupt: MRV_MI_DMA_READY_MASK
        HalIrqCtx_t *pHalIrq = NULL;
        uint32_t halIntMis = 0;

        CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcCamIcMiIrq, &pHalIrq);

        TRACE(ENG_ERR, "%s Raising DMA event\n", __func__);
        halIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHAL_INT_MI_DMA_READY);
        HalEventGenerator(pHalIrq, eHalIsrSrcCamIcMiIrq, halIntMis);
        // wait for irq process
        /* wait for test-event <= completion of DMA ready command */
        TEST_ASSERT_EQUAL_INT_RETURN(OSLAYER_OK, osEventWait(&pEngineTest->eventDmaFinished));
    }
#endif

#ifdef HAL_CMODEL
    /// send DMA ready interrupt: MRV_MI_DMA_READY_MASK
    HalIrqCtx_t *pHalIrq = NULL;
    uint32_t halCmIntMis = 0;

    CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcCamIcMiIrq, &pHalIrq);

    HalCmodelBufConfig_t *pCmBufCfg = NULL;

    HalCmGetBufConfig(hHal, &pCmBufCfg);

    if (pCmBufCfg->cmDmaIntEnabled == 1) {
        TRACE(ENG_ERR, "%s Raising DMA event\n", __func__);
        halCmIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHAL_INT_MI_DMA_READY);
        HalEventGenerator(pHalIrq, eHalIsrSrcCamIcMiIrq, halCmIntMis);
        // wait for irq process
        /* wait for test-event <= completion of DMA ready command */
        TEST_ASSERT_EQUAL_INT_RETURN(OSLAYER_OK, osEventWait(&pEngineTest->eventDmaFinished));
    }

    osSleep(100);

#ifdef ISP_HDR_STITCH
    if(pTestConfig->enableStitching)
    {
        int offset = 0;
        int width = picBuf.Data.raw.PicWidthPixel;
        int height = picBuf.Data.raw.PicHeightPixel;
        int data_size = width * height * 2;

        HalCmodelBufConfig_t * pHalCmBufCfg = NULL;

        unsigned int data_8 = 0, data_8H = 0, data_12=0;

        for(offset = 0; offset < data_size; offset += 2)
        {
             data_8 = picBufl.Data.raw.pData[offset + 1];
             data_8H = picBufl.Data.raw.pData[offset];

             data_12 = (data_8H << 12) | (data_8 << 4);
             //new interface
             ISPWriteToCmBuf(HDR_MEMORY0, 0x3, offset, &data_12);
        }


        HalCmDumpBuf ((char *)picBufl.Data.raw.pData, 0x200, "CmHal");
        HalCmDumpBuf ((char *)(picBufl.Data.raw.pData + data_size - 0x200), 0x200, "CmHal");
       for(offset = 0; offset < data_size; offset += 2)
        {
             data_8 = picBufs.Data.raw.pData[offset + 1];
             data_8H = picBufs.Data.raw.pData[offset];
             data_12 = (data_8H << 12) | (data_8 << 4);

             //new interface
             ISPWriteToCmBuf(HDR_MEMORY1, 0x3, offset, &data_12);
        }
        HalCmDumpBuf ((char *)picBufs.Data.raw.pData, 0x200, "CmHal");
        HalCmDumpBuf ((char *)(picBufs.Data.raw.pData + data_size - 0x200), 0x200, "CmHal");

        for(offset = 0; offset < data_size; offset += 2)
        {
             data_8 = picBufvs.Data.raw.pData[offset + 1];
             data_8H = picBufvs.Data.raw.pData[offset];
             data_12 = (data_8H << 12) | (data_8 << 4);

             //new interface
             ISPWriteToCmBuf(HDR_MEMORY2, 0x3, offset, &data_12);
        }
        HalCmDumpBuf ((char *)picBufvs.Data.raw.pData, 0x200, "CmHal");
        HalCmDumpBuf ((char *)(picBufvs.Data.raw.pData + data_size - 0x200), 0x200, "CmHal");
    }
#endif

    HalCmISPpipeline(hHal);

    // output buffer process
    HalCmBufTransfer(hHal, HAL_CMODEL_BUF_TRANS_MP | HAL_CMODEL_BUF_TRANS_SP | HAL_CMODEL_BUF_TRANS_SP2_BP);

    // Add some actions for Cmodel process, and event update.
    // MRV_MI_MP_FRAME_END_MASK
    // MRV_MI_SP_FRAME_END_MASK

    CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcCamIcMiIrq, &pHalIrq);
    if (pCmBufCfg->cmMpIntEnabled == 1) {
        TRACE(ENG_ERR, "%s Raising MP event\n", __func__);
        halCmIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHAL_INT_MI_FRAME_END_MP);
        HalEventGenerator(pHalIrq, eHalIsrSrcCamIcMiIrq, halCmIntMis);
        // wait for irq process
        osSleep(100);
    }

    if (pCmBufCfg->cmSpIntEnabled == 1) {
        TRACE(ENG_ERR, "%s Raising SP event\n", __func__);
        halCmIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHAL_INT_MI_FRAME_END_SP);
        HalEventGenerator(pHalIrq, eHalIsrSrcCamIcMiIrq, halCmIntMis);
        // wait for irq process
        osSleep(100);
    }
    if (pCmBufCfg->cmSp2BpIntEnabled == 1) {
        TRACE(ENG_ERR, "%s Raising SP2 event\n", __func__);
        halCmIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHAL_INT_MI_FRAME_END_SP2_BP);
        HalEventGenerator(pHalIrq, eHalIsrSrcCamIcMiIrq, halCmIntMis);
        // wait for irq process
        osSleep(100);
    }
#endif

//ISP measurement dump
#ifdef HAL_CMODEL
    if (pTestConfig->enableHIST){
        //isp hsit signal trigger
        CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcIspIrq, &pHalIrq);
            halCmIntMis =
                CamIcGetInterruptMis(hCamerIcDrv, eHal_INT_ISP_HistM);
            HalEventGenerator(pHalIrq, eHalIsrSrcIspIrq, halCmIntMis);
        osSleep(100);

            TRACE(ENG_ERR,
                  "%s Hist enable, trigger the hist interrupt\n",
                  __func__);
   }

    if (pTestConfig->enableAWB){
        //isp hsit signal trigger
        CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcIspIrq, &pHalIrq);
            halCmIntMis =
                CamIcGetInterruptMis(hCamerIcDrv, eHal_INT_ISP_WbM);
            HalEventGenerator(pHalIrq, eHalIsrSrcIspIrq, halCmIntMis);
        osSleep(100);

            TRACE(ENG_ERR,
                  "%s WB enable, trigger the WB interrupt\n",
                  __func__);
    }

    if (pTestConfig->enableEXP){
        //isp hsit signal trigger
        CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcIspIrq, &pHalIrq);
            halCmIntMis =
                CamIcGetInterruptMis(hCamerIcDrv, eHal_INT_ISP_ExpM);
            HalEventGenerator(pHalIrq, eHalIsrSrcIspIrq, halCmIntMis);
        osSleep(100);

            TRACE(ENG_ERR,
                  "%s Exp enable, trigger the Exp interrupt\n",
                  __func__);
    }


    if (pTestConfig->enableAfm){
        //isp hsit signal trigger
        CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcIspIrq, &pHalIrq);
            halCmIntMis =
                CamIcGetInterruptMis(hCamerIcDrv, eHal_INT_ISP_AfmM);
            HalEventGenerator(pHalIrq, eHalIsrSrcIspIrq, halCmIntMis);
        osSleep(100);

            TRACE(ENG_ERR,
                  "%s Afm enable, trigger the Afm interrupt\n",
                  __func__);
    }

if (pTestConfig->enableVsm){
    //isp hsit signal trigger
    CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcIspIrq, &pHalIrq);
        halCmIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHal_INT_ISP_VsmM);
        HalEventGenerator(pHalIrq, eHalIsrSrcIspIrq, halCmIntMis);
    osSleep(100);

        TRACE(ENG_ERR,
              "%s Vsm enable, trigger the Vsm interrupt\n",
              __func__);
}

#endif


#ifdef HAL_CMODEL
        TRACE(ENG_ERR,
              "%s generate eHAL_INT_MI_FRAME_END_SP interrupt done\n",
              __func__);

    //isp off signal trigger
    CamIcGetIrqCtx(hCamerIcDrv, eHalIsrSrcIspIrq, &pHalIrq);
        halCmIntMis =
            CamIcGetInterruptMis(hCamerIcDrv, eHAL_INT_ISP_OFF);
        HalEventGenerator(pHalIrq, eHalIsrSrcIspIrq, halCmIntMis);

        osSleep(100);
#endif


#ifdef ISP_MI_HANDSHAKE_NANO
    CamerIcMiMpHandshake_t *pHandshake =
        &pEngineConfig->data.image.mi_mp_handshake;

    if (pHandshake->handshk_switch == CAMERIC_MI_MP_HANDSHK_SWITCH_ENABLE) {
        TRACE(ENG_ERR, "%s MI MP handshake enable, no som_output expected\n",
              __func__);
        osSleep(1000);
    } else {
#endif
        TRACE(ENG_ERR,
              "%s capture done, waitting for cb event\n",
              __func__);

        osEventWait(&pEngineTest->eventEngineCaptureDone);


        TRACE(ENG_ERR, "%s Waitting for SomFinish\n", __func__);
        osEventWait(&pEngineTest->eventSomFinished);

#ifdef ISP_MI_HANDSHAKE_NANO
    }
#endif


    TRACE(ENG_ERR, "%s Prepare for the CamEngine StopStream and StopEngine\n",
          __func__);


    /* Clear status after test. */
    if ( ( (current_config + 1) == total_config ) && ( (current_frame + 1) == total_frame ) ){
        ENG_TestClearConfig(pEngineTest, pTestConfig);
    }
#if 0
    if (pTestConfig->denoise3dEnable){
        result = CamEngineDisable3DNR(pEngineTest->hCamEngine);
        assert(!result);
    }
#endif
    //release aev2 to free memory that it allocated at enable time.
    if ( pTestConfig->enableAev2){
        result = CamEngineReleaseExpV2Drv(pCamEngineCtx);
        assert(!result);
    }
#if 0
         /* disable cac. */
            if (pTestConfig->enableCAC)
            {
                result = CamEngineSetupCacDrv(pCamEngineCtx, BOOL_FALSE);
                assert(!result);
            }

            /* stop the cam-engine Adpcc instance */
            if (pTestConfig->enableDpcc)
            {
                result = CamEngineAdpccStop(hCamEngine);
                assert(!result);
            }

            /* disable dpf. */
            if (pTestConfig->enableDPF)
            {
                result = CamEngineAdpfStop(hCamEngine);
                assert(!result);
            }

            /* disable degamma. */
            if (pTestConfig->enableGamma)
            {
                result = CamerIcIspDegammaDisable(hCamerIcDrv);
                assert(!result);
            }

            /* disable lsc. */
            if (pTestConfig->enableLSC)
            {
                result = CamEngineSetupLscDrv(pCamEngineCtx, BOOL_FALSE);
                assert(!result);
            }

            /* disable mgc. */
            if (pTestConfig->enableMGC)
            {
                //result = CamerIcIspMgcDisable(pEngineTest->handle);
                //assert(!result);
            }

            /* disable WDR. */
            if (pTestConfig->enableWDR)
            {
                result = CamEngineDisableWdr(hCamEngine);
                assert(!result);
            }

            /* disable Jpeg */
            if (pTestConfig->enableJpeg)
            {
                result = CamEngineDisableJpe(hCamEngine);
                assert(!result);
            }
#endif
#ifdef ISP_DEC
//#ifdef HAL_CMODEL

    result = CamEngineMiMpCmpDumpToFile(hCamEngine, pTestConfig->outputFileNameNoSuffix);
    assert(!result);
//#endif
#endif
#ifdef ISP_DEC
        char cmpCmd[MAX_LEN];
        if(pTestConfig->enableDec) {
#ifdef HAL_CMODEL

        snprintf(cmpCmd, MAX_LEN,  "./VSICompress -i %s -f NV12 -w %d -h %d -binIn -tm 128x1r -binOut -a 32 -o %s\n", pTestConfig->outputFileNameNoSuffix,
                 pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].width, pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].height,
                 pTestConfig->goldenFileNameWithSuffix);

        ret = system(cmpCmd);

#else
        snprintf(cmpCmd, MAX_LEN,  "cp %s  %s", pTestConfig->outputFileNameNoSuffix, pTestConfig->goldenFileNameWithSuffix);
        ret = system(cmpCmd);

#endif
       }
        TRACE(ENG_ERR,"%s:cmd line %s\n", __func__, cmpCmd);
        if(ret < 0)

        TRACE(ENG_ERR,"%s:call cmd failed: %s\n", __func__, strerror(errno));
#endif

    result = CamEngineMiMpDumpToFile(hCamEngine);
    assert(!result);
    //dump register file
    result = HalDynRegDumpClose(hHal);
    assert(!result);

#ifndef HAL_CMODEL
    if(pTestConfig->enableAev2) {
        char str[MAX_LEN];
        sprintf(str, "%s.aev2", pTestConfig->outputFileNameNoSuffix);
        result = rename("dump_isp0_sensorid0_mi_mp_out_data_jdp_frame0_fpga.txt", str);

    }
#else
    if(pTestConfig->enableAev2) {
        char str[MAX_LEN];
        sprintf(str, "%s.aev2", pTestConfig->outputFileNameNoSuffix);
        result = rename("dump_isp0_sensorid0_mi_mp_out_data_jdp_frame0.txt", str);
    }
#endif
    osSleep(1000);

    uint16_t mask = (pTestConfig->enableHIST << DUMP_HIST_SHIFT) | (pTestConfig->enableEXP << DUMP_EXPM_SHIFT) |
                     (pTestConfig->enableAWB << DUMP_WBM_SHIFT) | (pTestConfig->enableAfm << DUMP_AFM_SHIFT) |
                     (pTestConfig->enableVsm << DUMP_VSM_SHIFT);
    //Todo: checking if need dump 3am data to file
    result = CamerIcIspDumpMeasureData(hCamerIcDrv,
        &pTestConfig->goldenFileNameWithSuffix[current_frame * MAX_LEN], mask);
    assert(result == RET_SUCCESS);

#ifdef ISP_DVP_PINMAPPING
    if( (pTestConfig->dvp_input_pin_mapping_en != 0) || (pTestConfig->enableFieldSelection) )
    {
        result = CamerIcIspDumpAcqPropNanoData(hCamerIcDrv, &pTestConfig->goldenFileNameWithSuffix[current_frame * MAX_LEN]);
        assert(result == RET_SUCCESS);
    }
#endif
#ifdef ISP_MI_FIFO_DEPTH_NANO
    if(pTestConfig->mi_mp_fifo_depth_en !=0 )
    {
        result = CamEngineMiFifoDepthDumpBufToFile(hCamEngine, &pTestConfig->goldenFileNameWithSuffix[current_frame * MAX_LEN]);
        assert(result == RET_SUCCESS);
    }
#endif
#ifdef ISP_MI_HANDSHAKE_NANO
    //checking if need to dump handshake, and free buffer.
    if (pEngineConfig->type == CAM_ENGINE_CONFIG_IMAGE) {
        if (pHandshake->handshk_switch == CAMERIC_MI_MP_HANDSHK_SWITCH_ENABLE) {

            result = CamEngineMiHandshakeDumpBufToFile(hCamEngine, pHandshake,
                &pTestConfig->goldenFileNameWithSuffix[current_frame * MAX_LEN]);
            assert(result == RET_SUCCESS);
        }
    }
#endif

    //Checking the test purpose, to decide if store golden data or compare golden data.
    if(pCtx->settings.testCfg.generateGolden != 0)
    {
       ret = ENG_GoldenStore(pParam, current_config, current_frame, pFrameItem);
    }

    if( (pCtx->settings.testCfg.verifyGolden != 0) && (pTestConfig->verifyCurFrame != 0) )
    {
       ret = ENG_GoldenVerify(pParam, current_config, current_frame, pFrameItem);
    }


#ifdef ISP_MI_HANDSHAKE_NANO
    //checking if need to dump handshake, and free buffer.
    if (pEngineConfig->type == CAM_ENGINE_CONFIG_IMAGE) {
        if (pHandshake->handshk_switch == CAMERIC_MI_MP_HANDSHK_SWITCH_ENABLE) {
            result = CamEngineMiHandshakeBufferFree(hCamEngine, pHandshake);
            assert(result == RET_SUCCESS);
        }
    }
#endif

    TRACE(ENG_ERR, "%s StopStreaming..\n", __func__);

    if (pEngineConfig->type == CAM_ENGINE_CONFIG_IMAGE) {
        struct CamEngineConfigImage_s *pImage = &pEngineConfig->data.image;

        if (pImage->pBuffer) {
            free(pImage->pBuffer);
            pImage->pBuffer = NULL;
        }
    }

    //  Update TPG enable choice here for FPGA timming.
    if ( pTestConfig->enableTpg ){
        result = CamEngineDisableTpg( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    osSleep(1000);

    /* multi frame loop */
    current_frame++;
    if ( current_frame < total_frame ){
        TRACE(ENG_ERR, "%s:%d goto testStart! current_frame = %d, total = %d\n", __func__, __LINE__, current_frame, total_frame);
        goto testStart;
    }

    /* multi configuration loop */
    current_config++;
    if ( current_config < total_config ){
        TRACE(ENG_ERR, "%s:%d goto configStart! current_config = %d, total = %d\n", __func__, __LINE__, current_config, total_config);
        goto configStart;
    }

    /* stop the cam-engine instance */
    result = CamEngineStop(hCamEngine);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_PENDING, result);

    result = osEventWait(&pEngineTest->eventEngineStop);
    TEST_ASSERT_EQUAL_INT_RETURN(OSLAYER_OK, result);

    TRACE(ENG_ERR, "%s Cam Engine stop successfully\n", __func__);

    freeConfigResource(pTestCase);

    return ret;
}

//frameNum
//pEngineTestConfig->goldenFileNameWithSuffix[i * MAX_LEN],
int ENG_GoldenStore(ENG_TestCaseParam_t *pParam, int config_cnt, int frame_cnt, cJSON *pFrameItem) {
    assert(pParam);

    int ret_dump = 0;
    int ret = 0;
    int ret_dump_buf = 0;

    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    assert(pFrameItem);

    char goldenFullPath[MAX_LEN];
    char goldenFullName[MAX_LEN];
    char goldenStoreName[MAX_LEN];

    char regDumpName[MAX_LEN];
    char regDumpFullName[MAX_LEN];
    char regDumpStoreName[MAX_LEN];

    char bufDumpName[MAX_LEN];
    char bufDumpFullName[MAX_LEN];
    char bufDumpStoreName[MAX_LEN];

    char * genDumpName = NULL;
    char * genBufDumpName = NULL;
    char * genSrcName = NULL;

    EngineTestConfig_t *pTestConfig;

    TestCase_t *pTestCase = pParam->pTestCase;
    assert(pTestCase);

    pTestConfig = &pTestCase->config[ config_cnt ].engine;
    assert(pTestConfig);

    assert(frame_cnt < pTestConfig->frameNum);

    TRACE(ENG_ERR, "%s Test mode: generation golden data\n", __func__);

    if(access(pCtx->settings.testCfg.goldenPath, 0) != 0)
    {
        TRACE(ENG_ERR, "%s Golden case folder generation\n", __func__);
        //check and store the corresponding file to dest strcuture.
        ret = mkdir(pCtx->settings.testCfg.goldenPath, S_IRWXU|S_IRWXG|S_IRWXO);
        if(ret == 0)
        {
            TRACE(ENG_ERR, "mkdir folder %s successfully\n", pCtx->settings.testCfg.goldenPath);
        }else{
            TRACE(ENG_ERR, "mkdir folder %s fail.\n", pCtx->settings.testCfg.goldenPath);
        }
    }

    _snprintf(goldenFullPath, MAX_LEN, 2, pCtx->settings.testCfg.goldenPath,
            pTestCase->golden_sub_path);
    TRACE(ENG_ERR, "golden sub folder %s\n", goldenFullPath);

    if(access(goldenFullPath, 0) != 0)
    {
        //check and store the corresponding file to dest strcuture.
        ret = mkdir(goldenFullPath, S_IRWXU|S_IRWXG|S_IRWXO);
        if(ret == 0)
        {
            TRACE(ENG_ERR, "mkdir folder %s successfully\n", goldenFullPath);
        }else{
            TRACE(ENG_ERR, "mkdir folder %s fail %s.\n", goldenFullPath);
        }
    }

    //Store reg dump file
    genDumpName = &pTestConfig->outputFileNameNoSuffix[frame_cnt * MAX_LEN];
    snprintf(regDumpName, MAX_LEN, "%s.regdump.txt", genDumpName);

    _snprintf(regDumpFullName, MAX_LEN, 2, goldenFullPath, regDumpName);
    TRACE(ENG_ERR, "Reg dump store location %s\n", regDumpFullName);

    _snprintf(regDumpStoreName, MAX_LEN, 2, pTestCase->golden_sub_path, regDumpName);

    cJSON_AddItemToObject(pFrameItem, "stored_reg_dump",
                      cJSON_CreateString(regDumpStoreName));
    ret_dump = FLE_CopyFile(regDumpName, regDumpFullName);
    if(ret_dump != 0)
    {
    cJSON_AddItemToObject(pFrameItem, "regdump_result",
                  cJSON_CreateString("reg dump store failed"));
         TRACE(ENG_ERR, "reg dump store location %s Failed\n", regDumpName);
         ret_dump = -1;
    }else{
        cJSON_AddItemToObject(pFrameItem, "regdump_result",
                      cJSON_CreateString("reg dump Stored"));
    }

    //Store buf dump file
    genBufDumpName = &pTestConfig->outputFileNameNoSuffix[frame_cnt * MAX_LEN];
    snprintf(bufDumpName, MAX_LEN, "%s.bufdump.dat", genBufDumpName);

    _snprintf(bufDumpFullName, MAX_LEN, 2, goldenFullPath, bufDumpName);
    TRACE(ENG_ERR, "Buf dump store location %s\n", bufDumpFullName);

    _snprintf(bufDumpStoreName, MAX_LEN, 2, pTestCase->golden_sub_path, bufDumpName);

    cJSON_AddItemToObject(pFrameItem, "stored_buf_dump",
                      cJSON_CreateString(bufDumpStoreName));
    ret_dump_buf = FLE_CopyFile(bufDumpName, bufDumpFullName);
    if(ret_dump_buf != 0)
    {
    cJSON_AddItemToObject(pFrameItem, "bufdump_result",
                  cJSON_CreateString("buf dump store failed"));
         TRACE(ENG_ERR, "buf dump store location %s Failed\n", bufDumpName);
         ret_dump_buf = -1;
    }else{
        cJSON_AddItemToObject(pFrameItem, "bufdump_result",
                      cJSON_CreateString("buf dump Stored"));
    }

    //Store golden data
    genSrcName = &pTestConfig->goldenFileNameWithSuffix[frame_cnt * MAX_LEN];

    _snprintf(goldenFullName, MAX_LEN, 2, goldenFullPath, genSrcName);
    TRACE(ENG_ERR, "golden store location %s\n", goldenFullName);

    _snprintf(goldenStoreName, MAX_LEN, 2, pTestCase->golden_sub_path, genSrcName);

    cJSON_AddItemToObject(pFrameItem, "generate_golden_image",
                      cJSON_CreateString(goldenStoreName));

    ret = FLE_CopyFile(genSrcName, goldenFullName);
    if(ret != 0)
    {
    cJSON_AddItemToObject(pFrameItem, "ggresult",
                  cJSON_CreateString("Golden generate failed"));
         TRACE(ENG_ERR, "golden store location %s Failed\n", goldenFullName);
         ret = -1;
    }else{
        cJSON_AddItemToObject(pFrameItem, "ggresult",
                      cJSON_CreateString("Golden Stored"));
    }

    return ret|ret_dump;
}

int ENG_TestConfigModules(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig) {
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);
#ifndef HAL_CMODEL
    result = ENG_TestConfigDual(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif

    /* rotatetion method. */
    result = ENG_TestConfigRotation(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* dpcc. */
    result = ENG_TestConfigDPCC(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* dpf. */
    result = ENG_TestConfigDPF(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* degamma. */
    result = ENG_TestConfigDegamma(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* lsc. */
    result = ENG_TestConfigLSC(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

#ifdef ISP_GCMONO
    /* enable Gcmono, to be expanded. */
    result = ENG_TestConfigGcmono(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif

#ifdef ISP_RGBGC
    /* enable rgb gamma */
    result = ENG_TestConfigRgbGc(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif
#ifdef ISP_CA
    /* enable color adjust */
    result = ENG_TestConfigCa(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif
    /* MGC */
    result = ENG_TestConfigMGC(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* WDR. */
    result = ENG_TestConfigWDR(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* BLS */
    result = ENG_TestConfigBLS(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* AWB */
    result = ENG_TestConfigAWB(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* AEC / EXP */
    result = ENG_TestConfigAEC(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* AEV2 */
    result = ENG_TestConfigAEV2(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* Gamma out */
    result = ENG_TestConfigGammaOut(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* HIST */
    result = ENG_TestConfigHIST(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* IS */
    result = ENG_TestConfigIS(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#ifdef ISP_RAWIS
    /* IS */
    result = ENG_TestConfigRawIS(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif

    /* IE */
    result = ENG_TestConfigIE(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result || result == RET_NOTSUPP);

    /* CNR */
    result = ENG_TestConfigCNR(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* STITHING */
    result = ENG_TestConfigStitching(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* CSM */
    result = ENG_TestConfigCSM(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* DIGITAL*/
    result = ENG_TestConfigDitalGain(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    /* 2DNR */
    result = ENG_TestConfig2DNR(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#ifdef ISP_GREENEQUILIBRATE
    result = ENG_TestConfigGreenEquilibration(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif

#if ISP_DEC
#ifndef HAL_CMODEL
    result = ENG_TestConfigDec(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif
#endif
/* Ee */
#ifdef ISP_EE
    result = ENG_TestConfigEe(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif
    result = ENG_TestConfigAfm(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    result = ENG_TestConfigVsm(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result || result == RET_NOTSUPP);

#ifdef ISP_WDR_V2
    /* WDR2 */
    result = ENG_TestConfigWDR2(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif

    result = ENG_TestConfigCmpd(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#ifdef ISP_WDR_V3
    result = ENG_TestConfigWDR3(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif
    result = ENG_TestConfigTpg(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

#ifdef ISP_DEMOSAIC2
    result = ENG_TestConfigDemosaic2(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
    result = ENG_TestConfigFlt(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#else
    /* FLT */
    result = ENG_TestConfigFLT(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

    result = ENG_TestConfigDemosaic(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif
    /* cac. */
    result = ENG_TestConfigCAC(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);

#ifdef ISP_3DNR
    result = ENG_TestConfig3DNR(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
#endif

    /* CPROC. */
    result = ENG_TestConfigCPROC(pEngineTest, pTestConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(0, result);
    return result;
}
int ENG_GoldenVerify(ENG_TestCaseParam_t *pParam, int config_cnt, int frame_cnt, cJSON *pFrameItem) {

    assert(pParam);

    int ret = 0;

    TestCtx_t *pCtx = pParam->pCtx;
    assert(pCtx);

    assert(pFrameItem);

    char goldenFullPath[MAX_LEN];
    char goldenFullName[MAX_LEN];
    char goldenStoreName[MAX_LEN];
    char * genSrcName = NULL;

    EngineTestConfig_t *pTestConfig;

    TestCase_t *pTestCase = pParam->pTestCase;
    assert(pTestCase);

    pTestConfig = &pTestCase->config[ config_cnt ].engine;
    assert(pTestConfig);

    assert(frame_cnt < pTestConfig->frameNum);

    TRACE(ENG_ERR, "%s Test mode: Verify golden data\n", __func__);

    _snprintf(goldenFullPath, MAX_LEN, 2, pCtx->settings.testCfg.goldenPath,
            pTestCase->golden_sub_path);
    TRACE(ENG_ERR, "golden sub folder %s\n", goldenFullPath);


    genSrcName = &pTestConfig->goldenFileNameWithSuffix[frame_cnt * MAX_LEN];

    _snprintf(goldenFullName, MAX_LEN, 2, goldenFullPath, genSrcName);
    TRACE(ENG_ERR, "golden store location %s\n", goldenFullName);

    _snprintf(goldenStoreName, MAX_LEN, 2, pTestCase->golden_sub_path, genSrcName);

    cJSON_AddItemToObject(pFrameItem, "verify_golden_image",
                      cJSON_CreateString(goldenStoreName));

    if(access(goldenFullName, 0) != 0)
    {
        cJSON_AddItemToObject(pFrameItem, "vgresult",
              cJSON_CreateString("Golden data missing"));
        return -1;
    }

    ret = fileCompare(genSrcName, goldenFullName);
    if (ret != 0) {
        cJSON_AddItemToObject(pFrameItem, "vgresult",
              cJSON_CreateString("failed"));
        return -1;
    }

    cJSON_AddItemToObject(pFrameItem, "vgresult",
          cJSON_CreateString("pass"));
    return ret;
}

static int fileCompare(const char *pFilename, const char *pFilename2) {
    FILE *pF = fopen(pFilename, "r");
    if (!pF) {
        return -1;
    }

    FILE *pF2 = fopen(pFilename2, "r");
    if (!pF2) {
        fclose(pF);
        return -2;
    }

    int ret = FLE_BinaryCompare(pF, pF2);

    fclose(pF);
    fclose(pF2);

    return ret;
}

static void freeConfigResource(TestCase_t *pTestCase) {
    EngineTestConfig_t *pTestConfig;

    if (!pTestCase)
        return;

    pTestConfig = &pTestCase->config[0].engine;

    if (pTestConfig->images) {
        free(pTestConfig->images);
        pTestConfig->images = NULL;
    }
}

static int ENG_TestConfigRotation(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;


    switch (pTestConfig->orientation) {
    case 1:
        CamEngineOriginal(hCamEngine);
        break;

    case 2:
        CamEngineVerticalFlip(hCamEngine);
        break;

    case 3:
        CamEngineHorizontalFlip(hCamEngine);
        break;

    case 4:
        CamEngineRotateLeft(hCamEngine);
        break;

    case 5:
        CamEngineHorizontalFlip(hCamEngine);
        CamEngineVerticalFlip(hCamEngine);
        break;

    case 6:
        CamEngineRotateRight(hCamEngine);
        break;

    default:
        TRACE(ENG_ERR, "No such rotation method.\n");
        break;
    }

    return ret;
}



static int ENG_TestConfigDPCC(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableDpcc) {
        result = CamerIcIspDpccSetStaticConfig(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pTestConfig->dpccConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        result = CamEngineAdpccStart(hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableDpcc) {
        result = CamEngineAdpccStop(hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return ret;
}

static int ENG_TestConfigDPF(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    result = CamerIcIspDpfSetFullConfig(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pTestConfig->dpfConfig);
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    return ret;
}

static int ENG_TestConfigDegamma(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableGamma) {
        CamerIcIspDegammaCurve_t Curve;


        result = CamerIcIspDegammaDisable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        memcpy(Curve.segment, pTestConfig->DegammaCurve_Seg, 16*sizeof(uint8_t));
        memcpy(Curve.red,   pTestConfig->DegammaCurve_R, 17*sizeof(uint16_t));
        memcpy(Curve.green, pTestConfig->DegammaCurve_G, 17*sizeof(uint16_t));
        memcpy(Curve.blue,  pTestConfig->DegammaCurve_B, 17*sizeof(uint16_t));

        result = CamerIcIspDegammaSetCurve(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &Curve);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspDegammaEnable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableGamma){
        result = CamerIcIspDegammaDisable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return ret;
}

static int ENG_TestConfigLSC(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableLSC) {

        result = CamerIcIspLscSetLenseShadeSectorConfig( pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pTestConfig->LscSectorConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspLscSetLenseShadeCorrectionMatrix(
                    pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc,
                    pTestConfig->LscRDataTbl,
                    pTestConfig->LscGRDataTbl,
                    pTestConfig->LscGBDataTbl,
                    pTestConfig->LscBDataTbl);

        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineSetupLscDrv(pCamEngineCtx, BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableLSC){
        result = CamEngineSetupLscDrv(pCamEngineCtx, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return ret;
}


#ifdef ISP_DEMOSAIC2

static int ENG_TestConfigFlt(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig) {
    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->flt.enable) {

        result = CamEngineSetupFltDrv(pCamEngineCtx, &pTestConfig->flt, BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    } else {
        result = CamEngineSetupFltDrv(pCamEngineCtx, &pTestConfig->flt, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
}

/*integration aft and cac*/
static int ENG_TestConfigDemosaic2(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;
#if 0
    CamericIspDemosaic_t *pDmsc = malloc(sizeof(CamericIspDemosaic_t ));
    memset(pDmsc, 0, sizeof(CamericIspDemosaic_t));

    pDmsc->sharpen_size = 8;
    pDmsc->denoise_stren = pTestConfig->DemosaicDenoiseStength;
    pDmsc->demosaic_thr= pTestConfig->DemosaicThreshold;

    pDmsc->demoir_cxt.demoire_area_thr = 0x1f;
    pDmsc->demoir_cxt.demoire_sat_shrink = 0x08;
    pDmsc->demoir_cxt.demoire_r2 = 0x100;
    pDmsc->demoir_cxt.demoire_r1 = 0x00;
    pDmsc->demoir_cxt.demoire_t2_shift= 0x03;
    pDmsc->demoir_cxt.demoire_t1 = 0xff;
    pDmsc->demoir_cxt.demoire_edge_r2 = 100;
    pDmsc->demoir_cxt.demoire_edge_r1 = 196;
    pDmsc->demoir_cxt.demoire_edge_t2_shift = 3;
    pDmsc->demoir_cxt.demoire_edge_t1 = 40;

    pDmsc->skin_cxt.cbcr_mode = 3;
    pDmsc->skin_cxt.depurple_sat_shrink= 8;
    pDmsc->skin_cxt.depurple_thr = 0x8c;
#endif

    if (pTestConfig->dmsc2.enable) {

        result = CamEngineSetupDmsc2Drv(pCamEngineCtx, &pTestConfig->dmsc2, BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }else {
        result = CamEngineSetupDmsc2Drv(pCamEngineCtx, &pTestConfig->dmsc2, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    }

    //    free(pDmsc);
    return ret;
}

#else

static int ENG_TestConfigFLT(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableFLT){
        TRACE(ENG_INF, ":%d FLT enabled\n", __LINE__);
        result = CamEngineSetupFltDrv( pCamEngineCtx, BOOL_TRUE );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        result = CamerIcIspFltSetFilterParameter(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc,
                 pTestConfig->FLTDenoiseLevel,
                 pTestConfig->FLTShrpenLevel,
                 pTestConfig->FLTchrVMode,
                 pTestConfig->FLTchrHMode);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableFLT){
        TRACE(ENG_INF, ":%d FLT disabled\n", __LINE__);
        result = CamEngineSetupFltDrv( pCamEngineCtx, BOOL_FALSE );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigDemosaic(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;
    TRACE(ENG_INF, ":%d %s enter\n", __LINE__, __func__);

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->DemosaicMode){
        TRACE(ENG_INF, ":%d Demosaic enabled\n", __LINE__);

        result = CamEngineDemosaicConfig(pCamEngineCtx, pTestConfig->DemosaicMode, pTestConfig->DemosaicThreshold);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    }

    TRACE(ENG_INF, ":%d %s exit\n", __LINE__, __func__);

    return result;
}

#endif
static int ENG_TestConfigCAC(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    int ret = 0;
    RESULT result = RET_FAILURE;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    /* cac. */
    if (pTestConfig->enableCAC) {
        result = CamEngineSetupCacDrv(pCamEngineCtx, BOOL_TRUE );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspCacConfig(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pTestConfig->CacConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableCAC) {
        result = CamEngineSetupCacDrv(pCamEngineCtx, BOOL_FALSE );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return ret;
}

static int ENG_TestConfigMGC(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    //CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;

    if (pTestConfig->enableMGC) {
        // result = CamerIcIspMgcEnable(pEngineTest->handle);
        // assert(!result);
    }

    if (pTestConfig->disableMGC) {
        // result = CamerIcIspMgcDisable(pEngineTest->handle);
        // assert(!result);
    }

    return result;
}

static int ENG_TestConfigWDR(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    //CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;

    if (pTestConfig->enableWDR) {
        result = CamEngineEnableWdr(pEngineTest->hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineWdrSetCurve(pEngineTest->hCamEngine, pTestConfig->WdrCurve);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableWDR){
        result = CamEngineDisableWdr(pEngineTest->hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigBLS(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableBLS){
        TRACE(ENG_INF, ":%d BLS enabled\n", __LINE__);
        CamEngineBlackLevel_t Blvl;
        Blvl.Red    = pTestConfig->BLSLevel[ 0 ];
        Blvl.GreenR = pTestConfig->BLSLevel[ 1 ];
        Blvl.GreenB = pTestConfig->BLSLevel[ 2 ];
        Blvl.Blue    = pTestConfig->BLSLevel[ 3 ];
        result = CamEngineSetupBlsDrv(pCamEngineCtx, BOOL_TRUE, &Blvl);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableBLS){
        TRACE(ENG_INF, ":%d BLS disabled\n", __LINE__);
        result = CamEngineSetupBlsDrv(pCamEngineCtx, BOOL_FALSE, NULL);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigAWB(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableAWB){
        TRACE(ENG_INF, ":%d AWB enabled\n", __LINE__);

        CamerIcAwbMeasuringConfig_t MeasConfig;

        MeasConfig.RefCb_MaxB   = 0x80;
        MeasConfig.RefCr_MaxR   = 0x80;
        MeasConfig.MaxY         = 0xE9;
        MeasConfig.MaxCSum      = 0x10;
        MeasConfig.MinY_MaxG    = 0xC0;
        MeasConfig.MinC         = 0x10;

        result = CamerIcIspAwbSetMeasuringMode( pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, pTestConfig->AWBMeasureMode, &MeasConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspAwbSetGains( pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &pTestConfig->AWBGain);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspAwbEnable( pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableAWB){
        TRACE(ENG_INF, ":%d AWB disabled\n", __LINE__);
        result = CamEngineSetupAwbDrv(pCamEngineCtx, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigAEC(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableEXP){
        TRACE(ENG_INF, ":%d EXP enabled\n", __LINE__);

        result = CamEngineReleaseExpDrv( pCamEngineCtx );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineSetupExpDrv( pCamEngineCtx, BOOL_TRUE, pTestConfig->EXPMeasureMode);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableEXP){
        TRACE(ENG_INF, ":%d EXP disabled\n", __LINE__);
        result = CamEngineReleaseExpDrv( pCamEngineCtx );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineSetupExpDrv( pCamEngineCtx, BOOL_FALSE, pTestConfig->EXPMeasureMode);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigAEV2(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    CamEngineAev2Cfg_t *camAev2Cfg = NULL;

    if (pTestConfig->enableAev2){
        TRACE(ENG_INF, ":%d EXPV2 enabled\n", __LINE__);

        result = CamEngineReleaseExpV2Drv( pCamEngineCtx );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);


        camAev2Cfg = (CamEngineAev2Cfg_t *)malloc(sizeof(CamEngineAev2Cfg_t));

        memcpy(camAev2Cfg, &pTestConfig->AEV2Config, sizeof(CamEngineAev2Cfg_t));



        result = CamEngineSetupExpV2Drv( pCamEngineCtx, BOOL_TRUE, camAev2Cfg);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    } else {
        TRACE(ENG_INF, ":%d EXPV2 disabled\n", __LINE__);
        result = CamEngineReleaseExpV2Drv( pCamEngineCtx );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineSetupExpV2Drv( pCamEngineCtx, BOOL_FALSE, camAev2Cfg);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    if (camAev2Cfg)
     free(camAev2Cfg);
    return result;
}

static int ENG_TestConfigGammaOut(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableGammaOut){
        TRACE(ENG_INF, ":%d Gamma out enabled\n", __LINE__);
        result = CamerIcIspActivateGammaOut(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspSetGammOutCurve(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, CAMERIC_ISP_SEGMENTATION_MODE_LOGARITHMIC, &pTestConfig->gammaOutCurve);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableGammaOut){
        TRACE(ENG_INF, ":%d Gamma out disabled\n", __LINE__);
        result = CamerIcIspActivateGammaOut(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    return result;
}

static int ENG_TestConfigHIST(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableHIST){
        TRACE(ENG_INF, ":%d HIST enabled\n", __LINE__);
        result = CamerIcIspHistSetMeasuringMode(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, pTestConfig->HISTMode);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    if ( pTestConfig->histWindow.width  != 0 && pTestConfig->histWindow.height != 0)
    {
                result = CamerIcIspHistSetMeasuringWindow(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, pTestConfig->histWindow.offsetX, pTestConfig->histWindow.offsetY,          \
                                                          pTestConfig->histWindow.width, pTestConfig->histWindow.height);
                TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
        result = CamerIcIspHistSetGridWeights(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, pTestConfig->histWeightTbl);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        result = CamerIcIspHistEnable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableHIST){
        TRACE(ENG_INF, ":%d HIST disabled\n", __LINE__);
        result = CamerIcIspHistDisable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigIS(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableIS){
        TRACE(ENG_INF, ":%d IS enabled\n", __LINE__);
        CamerIcWindow_t outWin;
        outWin.hOffset = pTestConfig->IsConfigTbl[0];
        outWin.vOffset = pTestConfig->IsConfigTbl[1];
        outWin.width = pTestConfig->IsConfigTbl[2];
        outWin.height = pTestConfig->IsConfigTbl[3];

        result = CamerIcIspIsSetOutputWindow(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &outWin, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspIsSetMaxDisplacement(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, outWin.hOffset, outWin.vOffset);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspIsEnable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableIS)
    {
        TRACE(ENG_INF, ":%d IS disabled\n", __LINE__);
        result = CamerIcIspIsDisable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

#ifdef ISP_RAWIS
static int ENG_TestConfigRawIS(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableRawIS){
        TRACE(ENG_INF, ":%d RawIS enabled\n", __LINE__);
        CamerIcWindow_t outWin;
        outWin.hOffset = pTestConfig->rawIsConfigTbl[0];
        outWin.vOffset = pTestConfig->rawIsConfigTbl[1];
        outWin.width = pTestConfig->rawIsConfigTbl[2];
        outWin.height = pTestConfig->rawIsConfigTbl[3];

        result = CamerIcIspRawIsSetOutputWindow(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &outWin, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspRawIsSetMaxDisplacement(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, outWin.hOffset, outWin.vOffset);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspRawIsEnable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableRawIS)
    {
        TRACE(ENG_INF, ":%d RawIS disabled\n", __LINE__);
        CamerIcWindow_t outWin;
        outWin.hOffset = 0;
        outWin.vOffset = 0;
        //it is must config the rawis width and height for bp path output.
        outWin.width = pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP].width;
        outWin.height = pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP].height;
        result = CamerIcIspRawIsSetOutputWindow(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, &outWin, BOOL_FALSE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        result = CamerIcIspRawIsDisable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}
#endif
static int ENG_TestConfigIE(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    result = CamEngineDisableImageEffect( pEngineTest->hCamEngine );
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    if (0 == pTestConfig->ieConfig.mode){
        TRACE(ENG_INF, ":%d Image effect disalbed\n", __LINE__);
    }else{
        TRACE(ENG_INF, ":%d Image effect enalbed\n", __LINE__);
        result = CamEngineEnableImageEffect( pEngineTest->hCamEngine, &pTestConfig->ieConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        //result = CamEngineImageEffectSetColorSelection( pEngineTest->hCamEngine, CAMERIC_IE_COLOR_SELECTION_RGB, 30);
        //TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigCNR(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableCNR){
        TRACE(ENG_INF, ":%d CNR enabled\n", __LINE__);
        result = CamerIcIspCnrSetLineWidth(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, pTestConfig->cnrLineWidth);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        result = CamerIcIspCnrSetThresholds(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc, pTestConfig->cnrThreshold1, pTestConfig->cnrThreshold2);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspCnrEnable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableCNR)
    {
        TRACE(ENG_INF, ":%d CNR disabled\n", __LINE__);
        result = CamerIcIspCnrDisable(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigSI(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if (pTestConfig->enableSIMP){
        CamEngineSimpConfig_t SimpConfig;
        PicBufMetaData_t      PicBuffer;
        uint8_t *pData ;
        memset((uint8_t*)&PicBuffer, 0, sizeof(PicBufMetaData_t));

        pData = malloc( 20*20*3 );
        memset( pData, 0x80, 20*20*3 );

        PicBuffer.Type = PIC_BUF_TYPE_YCbCr422;     // Only YUV422 supported.
        PicBuffer.Align = 0x400;
        PicBuffer.Layout = PIC_BUF_LAYOUT_COMBINED;
        PicBuffer.Data.data.DataSize = 20*20*3;

        SimpConfig.Mode = CAM_ENGINE_SIMP_MODE_OVERLAY;
        SimpConfig.SimpModeConfig.Overlay.OffsetX = 0;
        SimpConfig.SimpModeConfig.Overlay.OffsetY = 0;
        SimpConfig.pPicBuffer = &PicBuffer;

        result = CamEngineEnableSimp( pEngineTest->hCamEngine, &SimpConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableSIMP){
        result = CamEngineDisableSimp( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigCPROC(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if (pTestConfig->enableCPROC ){
        TRACE(ENG_INF, ":%d CPROC enabled\n", __LINE__);
        result = CamEngineEnableCproc( pEngineTest->hCamEngine, &pTestConfig->CprocConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (pTestConfig->disableCPROC){
        TRACE(ENG_INF, ":%d CPROC disabled\n", __LINE__);
        result = CamEngineDisableCproc( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

#if 0
static int ENG_TestConfigJpeg(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;


    if (pTestConfig->enableJpeg) {
        result = CamEngineEnableJpe(hCamEngine, &JpeConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}
#endif

#ifdef ISP_WDR_V2
static int ENG_TestConfigWDR2(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if ( pTestConfig->enableWDR2 ){
        TRACE(ENG_INF, ":%d WDR2 enabled\n", __LINE__);
        result = CamEngineWdr2SetStrength( pEngineTest->hCamEngine, pTestConfig->WDR2Strength );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnableWdr2( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disableWDR2 ){
        TRACE(ENG_INF, ":%d WDR2 disabled\n", __LINE__);
        result = CamEngineDisableWdr2( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}
#endif

#ifdef ISP_WDR_V3
static int ENG_TestConfigWDR3(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if ( pTestConfig->enableWDR3 ){
        TRACE(ENG_INF, ":%d WDR3 enabled\n", __LINE__);

        result = CamEngineWdr3SetStrength(  pEngineTest->hCamEngine,
                                            pTestConfig->Wdr3Strength,
                                            pTestConfig->Wdr3MaxGain,
                                            pTestConfig->Wdr3GlobalStrength );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnableWdr3( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disableWDR3 ){
        TRACE(ENG_INF, ":%d WDR3 disabled\n", __LINE__);
        result = CamEngineDisableWdr3( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}
#endif

static int ENG_TestConfigCmpd(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);
    if ( pTestConfig->enableCmpd ){
        result =  CamEngineCmpdSetTbl(pEngineTest->hCamEngine,
                                      (const int*)pTestConfig->ExpandTbl,
                                         (const int*)pTestConfig->CompressTbl);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        result = CamEngineCmpdConfig(   pEngineTest->hCamEngine,
                                        pTestConfig->cmpdBlsEnable,
                                        pTestConfig->compressEnable,
                                        pTestConfig->expandEnable,
                                        pTestConfig->BlsPara);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnableCmpd( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disableCmpd ){
        result = CamEngineDisableCmpd( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    return result;
}


static int ENG_TestConfigTpg(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamerIcIspTpgConfig_t TpgConfig;

    memcpy(&TpgConfig, &pTestConfig->tpgConfig, sizeof(CamerIcIspTpgConfig_t));

    if ( pTestConfig->enableTpg ){
        result = CamEngineTpgConfig( pEngineTest->hCamEngine, &TpgConfig );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        //disable here firstly
        result = CamEngineDisableTpg( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disableTpg ){
        result = CamEngineDisableTpg( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    osSleep(1000);
    return result;
}

static int ENG_TestConfigStitching(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if ( pTestConfig->enableStitching ){
        TRACE(ENG_INF, ":%d Stitching enabled\n", __LINE__);
        result = CamEngineStitchingConfig( pEngineTest->hCamEngine, pTestConfig->StitchingPara );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnableStitching( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disableStitching ){
        TRACE(ENG_INF, ":%d Stitching disabled\n", __LINE__);
        result = CamEngineDisableStitching( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigCSM(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if ( pTestConfig->enableCSM ){
        TRACE(ENG_INF, ":%d CSM enabled\n", __LINE__);
        result = CamerIcIspSetColorConversionRange(pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc,
                                pTestConfig->csm_color_range_y,
                                pTestConfig->csm_color_range_c);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    return result;
}

static int ENG_TestConfigAfm(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if ( pTestConfig->enableAfm){
        TRACE(ENG_INF, ":%d Afm enabled\n", __LINE__);
        result = CamEngineAfmConfig( pEngineTest->hCamEngine, pTestConfig->AfmWindowId, pTestConfig->AfmThreshold);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnableAfm( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disableAfm){
        TRACE(ENG_INF, ":%d Afm disabled\n", __LINE__);
        result = CamEngineDisableAfm (pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

static int ENG_TestConfigVsm(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;
    CamerIcWindow_t vsmWinCfg;

    vsmWinCfg.hOffset = pTestConfig->vsmWinHoffset;
    vsmWinCfg.vOffset = pTestConfig->vsmWinVoffset;
    vsmWinCfg.width   = pTestConfig->vsmWinWidth;
    vsmWinCfg.height  = pTestConfig->vsmWinHeight;

    if ( pTestConfig->enableVsm){
        TRACE(ENG_INF, ":%d Vsm enabled\n", __LINE__);


        result = CamEngineSetupVsmDrv( (CamEngineContext_t *)hCamEngine, &vsmWinCfg,BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamerIcIspVsmEnable( pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    else{
        TRACE(ENG_INF, ":%d Vsm disabled\n", __LINE__);
        result = CamerIcIspVsmDisable( pCamEngineCtx->chain[CHAIN_LEFT].hCamerIc );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    }

    return result;
}

static int ENG_TestConfig2DNR(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);
    if ( pTestConfig->enable2DNR){
        TRACE(ENG_INF, ":%d 2DNR enabled\n", __LINE__);
        CamerIcIsp2DnrConfig_t cfg2DNR;

        cfg2DNR.enable=  pTestConfig->enable2DNR;
        cfg2DNR.pregmaStrength = pTestConfig->denoise2DPregmStren;
        cfg2DNR.strength= pTestConfig->denoise2DStren;
        memcpy(cfg2DNR.coeff, pTestConfig->denoiseSigmaYTbl, sizeof(pTestConfig->denoiseSigmaYTbl));

        result = CamEngine2DnrConfig( pEngineTest->hCamEngine, &cfg2DNR);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnable2Dnr(pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if ( pTestConfig->disable2DNR ){
        TRACE(ENG_INF, ":%d 2DNR disabled\n", __LINE__);
        result = CamEngineDisable2Dnr( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    return result;
}

#ifdef ISP8000NANO_BASE

static int ENG_TestConfigNano(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);
    //pEngineConfig = &pTestConfig->engineConfig
    //struct CamEngineConfigImage_s *pImage = &pEngineConfig->data.image;
    struct CamEngineConfigImage_s *pImage = &pTestConfig->engineConfig.data.image;

#ifdef ISP_DPF_RAW
    //pCamEngineCtx->dpf_mode = pTestConfig->dpf_mode;            // set dpf mode to CamEngineCtx ??
    // Nano DPF raw specific control
    pImage->dpf_mode = pTestConfig->dpf_raw_mode;
    TRACE(ENG_ERR, "Set DPF RAW out mode to %d.\n", pImage->dpf_mode);
#endif

#ifdef ISP_DVP_PINMAPPING
    pImage->dvpPinMapping = pTestConfig->dvp_input_pin_mapping;
    TRACE(ENG_ERR, "Set Pin Mapping  mode to %d.\n", pImage->dvpPinMapping);
#endif

#ifdef ISP_MI_FIFO_DEPTH_NANO
    pImage->mi_mp_fifo_depth = pTestConfig->mi_mp_fifo_depth;
    TRACE(ENG_ERR, "Set MI MP OUTPUT FIFO DEPTH to %d.\n", pImage->mi_mp_fifo_depth);
#endif

#ifdef ISP_MI_ALIGN_NANO
    // Nano MP output LSB alignment settings for test
    pImage->mi_mp_lsb_align = pTestConfig->mi_mp_lsb_align;
    TRACE(ENG_ERR, "Set MI MP OUTPUT LSB alignment to %d.\n", pImage->mi_mp_lsb_align);
#endif

#ifdef ISP_MI_BYTESWAP
    // Nano MP output byteswap settings for test
    pImage->mi_mp_byte_swap = pTestConfig->mi_mp_byte_swap;
    TRACE(ENG_ERR, "Set MI MP OUTPUT byte swap to %d.\n", pImage->mi_mp_byte_swap);

    // Nano MP output field selection settings for test
    pImage->fieldSelection = pTestConfig->field_selection;
    TRACE(ENG_ERR, "Set MI MP OUTPUT field selection to %d.\n", pImage->fieldSelection);
#endif

#ifdef ISP_MI_HANDSHAKE_NANO
    CamerIcMiMpHandshake_t *pHandshake = &pImage->mi_mp_handshake;

    pHandshake->handshk_switch             = pTestConfig->mi_mp_handshake.handshk_switch;
    pHandshake->storage_fmt             = pTestConfig->mi_mp_handshake.storage_fmt;
    pHandshake->data_fmt                 = pTestConfig->mi_mp_handshake.data_fmt;

    pHandshake->line_per_slice_minus1     = pTestConfig->mi_mp_handshake.line_per_slice_minus1;
    pHandshake->slice_per_buffer_minus1 = pTestConfig->mi_mp_handshake.slice_per_buffer_minus1;
    pHandshake->ack_count                 = pTestConfig->mi_mp_handshake.ack_count;

    pHandshake->mp_y_llength_pixel         = pTestConfig->mi_mp_handshake.mp_y_llength_pixel;
    pHandshake->mp_y_slice_offset_bytes = pTestConfig->mi_mp_handshake.mp_y_slice_offset_bytes;
    pHandshake->mp_c_slice_offset_bytes = pTestConfig->mi_mp_handshake.mp_c_slice_offset_bytes;

    if (pHandshake->handshk_switch == CAMERIC_MI_MP_HANDSHK_SWITCH_ENABLE) {

        result = CamEngineMiHandshakeBufferAlloc(hCamEngine, pHandshake);
        assert(result == RET_SUCCESS);
    } else {
        pHandshake->mp_y_base_ad_init = 0;
        pHandshake->mp_cb_base_ad_init = 0;
        pHandshake->mp_cr_base_ad_init = 0;
    }

    pHandshake->mp_y_pic_width_pixel     = pTestConfig->mi_mp_handshake.mp_y_pic_width_pixel;
    pHandshake->mp_y_pic_height_pixel     = pTestConfig->mi_mp_handshake.mp_y_pic_height_pixel;

    TRACE(ENG_ERR,
        "Set MI MP handshake cfg, switch:%d, storage:%d, data_fmt:%d, "
        "line_per_slice-1:%d, slice_per_buffer-1:%d, ack_count:%d\n",
        pHandshake->handshk_switch, pHandshake->storage_fmt,
        pHandshake->data_fmt, pHandshake->line_per_slice_minus1,
        pHandshake->slice_per_buffer_minus1, pHandshake->ack_count);
#endif

#ifdef ISP_GCMONO
    // Nano BP Gcmono settings for test
    pImage->gcmono_ctx.enabled =  pTestConfig->gcmono_cxt.enabled;
    pImage->gcmono_ctx.mode =  pTestConfig->gcmono_cxt.mode;
    memcpy(pImage->gcmono_ctx.gcmonoLut, pTestConfig->gcmono_cxt.gcmonoLut, sizeof(pTestConfig->gcmono_cxt.gcmonoLut));
    memcpy(pImage->gcmono_ctx.gcmonoPx, pTestConfig->gcmono_cxt.gcmonoPx, sizeof(pTestConfig->gcmono_cxt.gcmonoPx));
    memcpy(pImage->gcmono_ctx.gcmonoDataX, pTestConfig->gcmono_cxt.gcmonoDataX, sizeof(pTestConfig->gcmono_cxt.gcmonoDataX));
    memcpy(pImage->gcmono_ctx.gcmonoDataY, pTestConfig->gcmono_cxt.gcmonoDataY, sizeof(pTestConfig->gcmono_cxt.gcmonoDataY));
#endif

    return result;
}
#endif  //#ifdef ISP8000NANO_BASE
#ifdef ISP_GCMONO
static int ENG_TestConfigGcmono(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->gcmono_cxt.enabled) {
        TRACE(ENG_INF, ":%d mono GC enabled\n", __LINE__);
        CamerIcIspGcMonoContext_t gcmonoCtx;
        memset(&gcmonoCtx, 0, sizeof(gcmonoCtx));
        // Nano BP Gcmono settings for test
        gcmonoCtx.enabled =  pTestConfig->gcmono_cxt.enabled;
        gcmonoCtx.mode=  pTestConfig->gcmono_cxt.mode;
        memcpy(&gcmonoCtx.gcmonoLut, pTestConfig->gcmono_cxt.gcmonoLut, sizeof(pTestConfig->gcmono_cxt.gcmonoLut));
        memcpy(&gcmonoCtx.gcmonoPx, pTestConfig->gcmono_cxt.gcmonoPx, sizeof(pTestConfig->gcmono_cxt.gcmonoPx));
        memcpy(&gcmonoCtx.gcmonoDataX, pTestConfig->gcmono_cxt.gcmonoDataX, sizeof(pTestConfig->gcmono_cxt.gcmonoDataX));
        memcpy(&gcmonoCtx.gcmonoDataY, pTestConfig->gcmono_cxt.gcmonoDataY, sizeof(pTestConfig->gcmono_cxt.gcmonoDataY));
        result = CamEngineSetupGcmonoDrv(pCamEngineCtx,  &gcmonoCtx);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
      //  result = CamEngineSwitchGcmonoDrv(pCamEngineCtx, CAMERIC_ISP_GCMONO_SWITCH_ENABLE);
    }
    else{
        TRACE(ENG_INF, ":%d mono GC disabled\n", __LINE__);
        result = CamEngineSwitchGcmonoDrv(pCamEngineCtx, CAMERIC_ISP_GCMONO_SWITCH_DISABLE);
    }
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    return result;
}

#endif  //#ifdef ISP_GCMONO

#ifdef ISP_RGBGC
static int ENG_TestConfigRgbGc(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->rgbgamma_cxt.enabled) {
        TRACE(ENG_INF, ":%d rgb gamma out enabled\n", __LINE__);
        CamerIcIspRgbGammaContext_t rgbgammaCtx;
        memset(&rgbgammaCtx, 0, sizeof(rgbgammaCtx));
        memcpy(&rgbgammaCtx, &pTestConfig->rgbgamma_cxt.enabled, sizeof(CamerIcIspRgbGammaContext_t));

        result = CamEngineSetupRgbGammaDrv(pCamEngineCtx,  &rgbgammaCtx);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    else{
        TRACE(ENG_INF, ":%d rgb Gamma correction disabled\n", __LINE__);
        result = CamEngineReleaseRgbGammaDrv(pCamEngineCtx);
    }
    TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    return result;
}

#endif  //#ifdef ISP_GCMONO

static int ENG_TestClearConfig(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){
    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if (pTestConfig->enableCPROC){
        TRACE(ENG_INF, "%s:%d Clear CPROC status.\n", __FILE__, __LINE__);
        result = CamEngineDisableCproc( pEngineTest->hCamEngine );
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    return result;
}

#ifdef ISP_EE
static int ENG_TestConfigEe(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig){

    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);
    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->EeConfig.enabled){
        TRACE(ENG_INF, ":%d EE enabled\n", __LINE__);
        result = CamEngineEeConfig(pCamEngineCtx, &pTestConfig->EeConfig);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

        result = CamEngineEnableEe(pCamEngineCtx);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }

    if (!pTestConfig->EeConfig.enabled){
        TRACE(ENG_INF, ":%d Ee disabled\n", __LINE__);
        result = CamEngineDisableEe(pCamEngineCtx);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
    return result;
}
#endif

#ifdef ISP_CA
int ENG_TestConfigCa        (EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig)
{
    RESULT result = RET_SUCCESS;

     assert(pEngineTest);
     assert(pTestConfig);

     CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
     CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

     if (pTestConfig->enableCa){
         TRACE(ENG_INF, ":%dcolor adjust enabled\n", __LINE__);
         CamerIcIspCa_t *pClrAdj = (CamerIcIspCa_t *)malloc(sizeof(CamerIcIspCa_t));
         pClrAdj->caMode = pTestConfig->caMode;
         memcpy(&pClrAdj->CaLutXTbl, &pTestConfig->CaLutXTbl, sizeof(pClrAdj->CaLutXTbl));
         memcpy(&pClrAdj->CaLutChromaTbl, &pTestConfig->CaLutChromaTbl, sizeof(pClrAdj->CaLutChromaTbl));
         memcpy(&pClrAdj->CaLutLumaTbl, &pTestConfig->CaLutLumaTbl, sizeof(pClrAdj->CaLutLumaTbl));
         memcpy(&pClrAdj->CaLutShiftTbl, &pTestConfig->CaLutShiftTbl, sizeof(pClrAdj->CaLutShiftTbl));
         result = CamEngineSetupColorAdjusDrv(pCamEngineCtx, pClrAdj, BOOL_TRUE);
         TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
         free(pClrAdj);

     }

     return result;

}
#endif

#ifdef ISP_3DNR
static int ENG_TestConfig3DNR(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig)
{
    RESULT result = RET_SUCCESS;
#if 1
    assert(pEngineTest);
    assert(pTestConfig);

    if (pTestConfig->denoise3dEnable){
        //
        result = CamEngine3DnrSetStrength(pEngineTest->hCamEngine, pTestConfig->denoise3dStrength);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        //3dnr update
        result = CamEngine3DnrUpdate(pEngineTest->hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        //3dnr compute
//      result = CamEngine3DnrCompute(pEngineTest->hCamEngine);
//      TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
        //3dnr enable
#ifdef ISP_3Dnr_DDR_LESS
        result = CamEngine3DnrSetCompress(pEngineTest->hCamEngine, &pTestConfig->compress);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
#endif
                result = CamEngineEnable3Dnr(pEngineTest->hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    } else {
        //3dnr disable
        result = CamEngineDisable3Dnr(pEngineTest->hCamEngine);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);
    }
#endif
    return result;

}
#endif

static int ENG_TestConfigDual(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig)
{
    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);

    if (pTestConfig->enableDual){

        EngineTestInitialRegisterFile(pEngineTest->hCamEngine->hHal, ENGINE_DUMP_ISP_REGS_DUAL_SETTINGS);
    }

    return result;

}
static int ENG_countEnablePath(EngineTest_t *pEngineTest,  CamEngineConfig_t *pCamEngineConfig)
{
    char cnt = 0;
    assert(pEngineTest);
    assert(pCamEngineConfig);

    for (int i = 0 ; i <= CAM_ENGINE_PATH_SELF2_BP; i++) {
        if (pCamEngineConfig->pathConfig[i].mode != CAMERIC_MI_DATAMODE_DISABLED)
            cnt++;
    }
    return cnt;

}
static int ENG_TestConfigDitalGain(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig) {
    RESULT result = RET_SUCCESS;
    CamerIcIspDigitalGainConfig_t dGainCfg;
    TRACE(ENG_INF, ":%d %s enter\n", __LINE__, __func__);

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->digiGain.enable) {
        TRACE(ENG_INF, ":%d digital gain enabled\n", __LINE__);
        dGainCfg.enable = pTestConfig->digiGain.enable;
        dGainCfg.gainR  = pTestConfig->digiGain.gain_r;
        dGainCfg.gainB  = pTestConfig->digiGain.gain_b;
        dGainCfg.gainGr  = pTestConfig->digiGain.gain_gr;
        dGainCfg.gainGb  = pTestConfig->digiGain.gain_gb;
        result = CamEngineSetupDigitalGainDrv(pCamEngineCtx, &dGainCfg, BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    } else {
    }
    TRACE(ENG_INF, ":%d %s exit\n", __LINE__, __func__);

    return result;
}

#ifdef ISP_GREENEQUILIBRATE
int ENG_TestConfigGreenEquilibration (EngineTest_t * pEngineTest,EngineTestConfig_t * pTestConfig)
{
    RESULT result = RET_SUCCESS;
    TRACE(ENG_INF, ":%d %s enter\n", __LINE__, __func__);

    assert(pEngineTest);
    assert(pTestConfig);

    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;

    if (pTestConfig->enableGrnEqlr){
        TRACE(ENG_INF, ":%d green equilibration enabled\n", __LINE__);
        CamerIcIspGreenEqlr_t grnEqlrCfg;
        grnEqlrCfg.threshold = pTestConfig->grnEqlrTh;
        grnEqlrCfg.dummyLine = pTestConfig->grnEqlrHcntDmy;

        result = CamEngineSetupGreenEquilibrationDrv(pCamEngineCtx, &grnEqlrCfg, BOOL_TRUE);
        TEST_ASSERT_EQUAL_INT_RETURN(RET_SUCCESS, result);

    }
    TRACE(ENG_INF, ":%d %s exit\n", __LINE__, __func__);

    return result;
}
#endif

#ifdef ISP_DEC
#ifndef HAL_CMODEL
static int ENG_TestConfigDec(EngineTest_t *pEngineTest, EngineTestConfig_t *pTestConfig)
{
    RESULT result = RET_SUCCESS;

    assert(pEngineTest);
    assert(pTestConfig);
    CamEngineHandle_t hCamEngine = pEngineTest->hCamEngine;
    CamEngineContext_t *pCamEngineCtx = (CamEngineContext_t *)hCamEngine;


    if (pTestConfig->enableDec){

        CamEngineSetupDecDrv(pCamEngineCtx, BOOL_TRUE);
    }
    else
    {
        CamEngineSetupDecDrv(pCamEngineCtx, BOOL_FALSE);
    }
    return result;

}
#endif
#endif

