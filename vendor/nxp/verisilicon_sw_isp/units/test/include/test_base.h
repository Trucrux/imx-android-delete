#ifndef __TEST_BASE_H__
#define __TEST_BASE_H__

#include <cam_engine/cam_engine_api.h>
#include <cameric_drv/cameric_drv_api.h>
#include <cam_engine/cam_engine_isp_api.h>
#include <cameric_ie_drv_api.h>
#include <cameric_isp_dpcc_drv_api.h>
#include <cam_calibdb.h>
#include <cam_calibdb/cam_calibdb_api.h>
#include <cam_engine.h>
#include <common/picture_buffer.h>
#include <som_ctrl/som_ctrl_api.h>
#include "cJSON.h"
#include "bm_golden.h"

#include <cameric_drv/cameric_isp_cac_drv_api.h>
#ifdef ISP_DEMOSAIC2
#include <cameric_drv/cameric_isp_dmsc2_drv_api.h>
#endif
#include <cam_engine/cam_engine_cproc_api.h>
#include <cameric_drv/cameric_isp_3dnr_drv_api.h>

#include <ic_dev.h>

#define TEST_ASSERT_EQUAL_INT_RETURN(expected, actual)                                        \
    if (expected == actual)                                                                   \
    {                                                                                         \
    }                                                                                         \
    else                                                                                      \
    {                                                                                         \
        printf("--->%s: %d, expected=%d, actual=%d\n", __FILE__, __LINE__, expected, actual); \
        return actual;                                                                        \
    }

#define MAX_LEN 256

#define     DEGAMMA_CURVE_SEG_LEN          16
#define     DEGAMMA_CURVE_RGB_LEN          17
#define     LSC_DATA_TABLE_LEN             289

#define CA_CURVE_DATA_TABLE_LEN  65
typedef struct TestSetting_s
{
    /* SomCtrl. */
    struct Som_s
    {
        int enable;
    } som;

    struct TestCfg_s
    {
        int    generateGolden;
        int    verifyGolden;
        char * goldenPath;
    }testCfg;
} TestSetting_t;

typedef enum TestType_e
{
    TEST_TYPE_INVALID = -1,
    TEST_TYPE_ENGINE,
    TEST_TYPE_BM
} TestType_t;

typedef struct EngineTestConfig_s
{
    CamEngineConfig_t engineConfig;
    unsigned int verifyCurFrame;
    unsigned int orientation;
    unsigned int enableCAC;
    unsigned int disableCAC;
    unsigned int enableDpcc;
    unsigned int disableDpcc;
    unsigned int enableGamma;       // degamma
    unsigned int disableGamma;
    unsigned int enableLSC;
    unsigned int disableLSC;
    unsigned int enableMGC;
    unsigned int disableMGC;
    unsigned int enableWDR;
    unsigned int disableWDR;
    unsigned int enableJpeg;
    unsigned int frameNum;

    unsigned int imagesNumber;
    char *images;

    char *outputFileNameNoSuffix;
    char *goldenFileNameWithSuffix;

    CamerIcIspCacConfig_t CacConfig;
    CamEngineWdrCurve_t WdrCurve;

    /* BLS */
    unsigned int enableBLS;
    unsigned int disableBLS;
    unsigned int BLSSubMode;
    unsigned int BLSLevel[4];

    /* AWB */
    unsigned int enableAWB;
    unsigned int disableAWB;
    unsigned int AWBMeasureMode;
    CamerIcGains_t AWBGain;

    /* EXP */
    unsigned int enableEXP;
    unsigned int disableEXP;
    unsigned int EXPMeasureMode;

    /* FLT */
    unsigned int enableFLT;
    unsigned int disableFLT;
    unsigned int FLTDenoiseLevel;
    unsigned int FLTShrpenLevel;
    unsigned int FLTchrVMode;
    unsigned int FLTchrHMode;

    /* GammaOut */
    unsigned int enableGammaOut;
    unsigned int disableGammaOut;
    CamerIcIspGammaCurve_t gammaOutCurve;

    /* HIST */
    unsigned int enableHIST;
    unsigned int disableHIST;
    CamerIcIspHistMode_t HISTMode;
	CamerIcIspHistWindow_t histWindow;
    unsigned char histWeightTbl[CAMERIC_ISP_HIST_GRID_ITEMS];

    /* IS */
    unsigned int enableIS;
    unsigned int disableIS;
    unsigned int IsConfigTbl[4];
#ifdef ISP_RAWIS
    /* RawIS */
    unsigned int enableRawIS;
    unsigned int disableRawIS;
    unsigned int rawIsConfigTbl[4];
#endif
    /* SIMP */
    unsigned int enableSIMP;
    unsigned int disableSIMP;

	/* IE */
    CamerIcIeConfig_t ieConfig;
	/* Degamma */
	unsigned char   DegammaCurve_Seg[ DEGAMMA_CURVE_SEG_LEN ];
	unsigned short  DegammaCurve_R[ DEGAMMA_CURVE_RGB_LEN ];
	unsigned short  DegammaCurve_G[ DEGAMMA_CURVE_RGB_LEN ];
	unsigned short  DegammaCurve_B[ DEGAMMA_CURVE_RGB_LEN ];

	/* LSC */
	unsigned short  LscRDataTbl[ LSC_DATA_TABLE_LEN ];    /**< correction values of R color part */
    unsigned short  LscGRDataTbl[ LSC_DATA_TABLE_LEN ];   /**< correction values of G (red lines) color part */
    unsigned short  LscGBDataTbl[ LSC_DATA_TABLE_LEN ];   /**< correction values of G (blue lines) color part  */
    unsigned short  LscBDataTbl[ LSC_DATA_TABLE_LEN ];    /**< correction values of B color part  */

    CamerIcIspLscSectorConfig_t     LscSectorConfig;

#ifdef ISP_CA
	/* CA  color adjust*/

    bool  enableCa;
    unsigned char caMode;

    unsigned short  CaLutXTbl[ CA_CURVE_DATA_TABLE_LEN ];
    unsigned short  CaLutLumaTbl[ CA_CURVE_DATA_TABLE_LEN ];
    unsigned short  CaLutChromaTbl[ CA_CURVE_DATA_TABLE_LEN ];
    unsigned short  CaLutShiftTbl[ CA_CURVE_DATA_TABLE_LEN ];
#endif
    struct isp_digital_gain_cxt digiGain;

        /* DPF */
	struct isp_dpf_context dpfConfig;

	/* CPROC */
    unsigned int enableCPROC;
    unsigned int disableCPROC;
    CamEngineCprocConfig_t CprocConfig;

    /* CNR */
    unsigned int enableCNR;
    unsigned int disableCNR;
    unsigned int cnrLineWidth;
    unsigned int cnrThreshold1;
    unsigned int cnrThreshold2;

	/* CSM */
    unsigned int enableCSM;
    CamerIcColorConversionRange_t csm_color_range_y;
	CamerIcColorConversionRange_t csm_color_range_c;

    /* Xtalk */
    unsigned int enableXtalk;  // CCM
    CamEngineCcMatrix_t ccMatrix;
    CamEngineCcOffset_t ccOffset;

    /* AEV2 */
    unsigned int enableAev2;
    struct {
        unsigned int vOffset;
        unsigned int hOffset;
        unsigned int width;
        unsigned int height;

        uint8_t rWeight;
        uint8_t grWeight;
        uint8_t gbWeight;
        uint8_t bWeight;

    }AEV2Config;

#ifdef ISP_DVP_PINMAPPING
	uint8_t							dvp_input_pin_mapping_en;
	uint8_t							dvp_input_pin_mapping;
#endif

#ifdef ISP_DPF_RAW
	CamerIcIspDpfRaw_t				dpf_raw_mode;
#endif

#ifdef ISP_MI_FIFO_DEPTH_NANO
	unsigned int                    mi_mp_fifo_depth_en;
	CamerIcMiMpOutputFifoDepth_t	mi_mp_fifo_depth;
#endif

#ifdef ISP_MI_ALIGN_NANO
	CamerIcMiMpOutputLsbAlign_t 	mi_mp_lsb_align;
#endif

#ifdef ISP_MI_BYTESWAP
	CamerIcMiMpOutputSwapByte_t 	mi_mp_byte_swap;
#endif

#ifdef ISP_MI_HANDSHAKE_NANO
	CamerIcMiMpHandshake_t			mi_mp_handshake;
#endif

#ifdef ISP_GCMONO
	CamerIcIspGcMonoContext_t		gcmono_cxt;
#endif
#ifdef ISP_RGBGC
	CamerIcIspRgbGammaContext_t		rgbgamma_cxt;
#endif

#ifdef ISP8000NANO_BASE
    unsigned int                    enableFieldSelection;
	CamerIcIspFieldSelection_t    	field_selection;
#endif

    /* WDR2 */
    unsigned int    enableWDR2;
    unsigned int    disableWDR2;
    float           WDR2Strength;

    /* WDR3 */
    unsigned int    enableWDR3;
    unsigned int    disableWDR3;
    int32_t         Wdr3Strength;
    int32_t         Wdr3MaxGain;
    int32_t         Wdr3GlobalStrength;

    /* Compand */
    unsigned int    enableCmpd;
    unsigned int    disableCmpd;
    bool_t          blsEnable;
    bool_t          cmpdBlsEnable;
    bool_t          compressEnable;
    bool_t          expandEnable;
    unsigned int    ExpandTbl[64];
    unsigned int    CompressTbl[64];
    int             BlsPara[ 4 ];

    /* TPG */
    unsigned int enableTpg;
    unsigned int disableTpg;
    CamerIcIspTpgConfig_t tpgConfig;

    /* STITCHING */
    unsigned int    enableStitching;
    unsigned int    disableStitching;
    unsigned int    StitchingPara;

    unsigned int    enableAfm;
    unsigned int    disableAfm;
    unsigned int    AfmWindowId;
    unsigned int    AfmThreshold;
    unsigned char    DemosaicMode;
    unsigned char    DemosaicThreshold;
    unsigned char    DemosaicDenoiseStength;
    bool_t enableVsm;
    unsigned int vsmWinHoffset;
    unsigned int vsmWinVoffset;
    unsigned int vsmWinHeight;
    unsigned int vsmWinWidth;
#ifdef ISP_GREENEQUILIBRATE
    bool_t enableGrnEqlr;
    unsigned int grnEqlrTh;
    unsigned int grnEqlrHcntDmy;

#endif

#ifdef ISP_DEC
    bool_t   enableDec;
#endif
	/* EE */
    CamEngineEeConfig_t EeConfig;

    bool_t    enable2DNR;
    bool_t    disable2DNR;

    uint8_t   denoise2DStren;
    uint8_t   denoise2DPregmStren;
    uint16_t  denoiseSigmaYTbl[60];

    /* 3DNR */
	unsigned int  denoise3dEnable;
	unsigned int  denoise3dStrength;
        CamericIsp3DnrCompress_t compress;
        bool_t   enableDual;

    CamerIcDpccStaticConfig_t dpccConfig;
#ifdef ISP_DEMOSAIC2
    CamericIspDemosaic_t dmsc2;
     CamericIspDmscFlt_t flt;
#endif

} EngineTestConfig_t;

typedef enum
{
    BM_CHANNEL_Y,
    BM_CHANNEL_CB,
    BM_CHANNEL_CR,
	BM_CHANNEL_RAW0,
	BM_CHANNEL_RAW1,

    BM_CHANNEL_MAX
} BMChannel_t;

typedef struct BMTestConfigIO_s
{
    char filename[BM_CHANNEL_MAX][MAX_LEN];
} BMTestConfigIO_t;

typedef enum
{
    BM_PATH_MAIN,
    BM_PATH_SELF1,
    BM_PATH_SELF2,
    BM_PATH_MCM,
    BM_PATH_MAX
} BMPath_t;

typedef struct BMTestConfig_s
{
    BMTestConfigIO_t input;
    BMTestConfigIO_t output[BM_PATH_MAX];
    char registerFile[MAX_LEN];
} BMTestConfig_t;

#define CONFIG_MAX 120

typedef struct TestCase_s
{
    char path[MAX_LEN];
    char name[MAX_LEN];
    char golden_sub_path[MAX_LEN];
    char description[MAX_LEN];
    char version[MAX_LEN];
    char date[MAX_LEN];
    TestType_t type;
    union TestConfig_u {
        EngineTestConfig_t engine;
        BMTestConfig_t bm;
    } config[CONFIG_MAX];
    char configCount;
} TestCase_t;

typedef struct EngineTest_s
{
    CamerIcDrvHandle_t handle;
    CamCalibDbHandle_t hCamCalibDb;
    CamEngineHandle_t hCamEngine;

    osEvent eventEngineStarted;
    osEvent eventEngineStop;
    osEvent eventEngineCaptureDone;

    somCtrlHandle_t hSomCtrl;
    osEvent eventSomStarted;
    osEvent eventSomStop;
    osEvent eventSomFinished;

    osEvent eventStreamStarted;
    osEvent eventDmaFinished;
} EngineTest_t;

typedef struct BMMemPool_s
{
    MediaBufPool_t bufPool;
    MediaBufPoolConfig_t bufPoolConfig;
    MediaBufPoolMemory_t bufPoolMemory;
} BMMemPool_t;

typedef enum BMRegisterOpType_e
{
    BM_REG_READ,
    BM_REG_WRITE
} BMRegisterOpType_t;

typedef struct BMRegisterOp_s
{
    BMRegisterOpType_t type;
    uint32_t addr;
    uint32_t value;
} BMRegisterOp_t;

typedef struct BMCmdBuffer_s
{
    uint32_t cmdNum;
    BMRegisterOp_t *registerOp;
} BMCmdBuffer_t;

typedef struct BMTest_s
{
    BMMemPool_t memPool;
    MediaBuffer_t *pInputBuffer[BM_CHANNEL_MAX];
    MediaBuffer_t *pOutputBuffer[BM_PATH_MAX][BM_CHANNEL_MAX];
    BMCmdBuffer_t cmdBuffer;
    uint32_t goldenBaseAddress[BM_PATH_MAX][BM_CHANNEL_MAX];
    bool isPingPangBuffer;
} BMTest_t;

/******************************************************************************
 * TestCtx_t
 *****************************************************************************/
typedef struct TestCtx_s
{
    HalHandle_t hHal;

    unsigned int testRepeatTimes;
    char *settingFileName;
    TestSetting_t settings;

    char *caseListFileName;

    /* Driver based test. */
    EngineTest_t *pEngineTest;

    /* Bare metal test. */
    BMTest_t *pBmTest;
} TestCtx_t;

void InitCalibDbForDma(
    CamCalibDbHandle_t *hCalibDb);

void InitDegammaForDma(
    CamEngineContext_t *pCamEngineCtx,
    bool_t init_gamma_enable);

int EngineTestCreate(
    TestCtx_t *pCtx,
    EngineTest_t **ppEngineTest);

void EngineTestDestroy(
    TestCtx_t *pCtx,
    EngineTest_t *pEngineTest);

void EngineTestInitialRegisterFile(
    HalHandle_t hHal,
    const char *iniRegFileName);

int CamEngineTestRegisterInitial(
    HalHandle_t hHal,
    const char *registerFileName,
    int inibool);

int CamEngineTestInitLsc(
    CamEngineHandle_t hCamEngine,
    int inibool);

int CamEngineTestInitDpcc(
    CamEngineHandle_t hCamEngine,
    int inibool);

#if defined(HAL_ALTERA)
int FpgaResetControl(
    HalHandle_t HalHandle,
    int is_reset);
#endif

int BMTestSoftReset(TestCtx_t *);

int BMTestCreate(
    TestCtx_t *pCtx);

void BMTestDestroy(
    TestCtx_t *pCtx,
    BMTest_t *pBmTest);

int BMTestLoadInput(
    TestCtx_t *pCtx,
    BMTestConfigIO_t *pInput,
    int index,
    cJSON *);

int BMTestLoadCommands(
    TestCtx_t *pCtx,
    char *pCmdFile,
    cJSON *);

int BMTestExecCommands(
    TestCtx_t *pCtx,
    BMTest_t *pBmTest);

int BMTestSaveOutput(
    TestCtx_t *,
    BMTestConfig_t *,
    BMG_AnalyzeResult_t *,
    int,
    int);

int BMTestInitAllBuffers(TestCtx_t *);

int BMTestCheckAllBuffers(TestCtx_t *);

#endif
