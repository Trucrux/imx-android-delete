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
#include <common/picture_buffer.h>
#include <libgen.h>
#include <unistd.h>

#include "bm_golden.h"
#include "file.h"
#include "test_base.h"
#include "bare_metal.h"

//#define VSE_MI
//#define FE_MODE

#define REG_WRITE 0
#define REG_READ 1

#define FRAME_NUM 4

#define PIC_BUFFER_ALIGN (0x400)
#ifdef BM_ISP_MIV2
//modify by xzw 2019/6/28
//update register manual
#define MRV_MI_BASE                             0x1300                    //0X1300                                            // 0x1400

/*DMA_INPUT*/
//SP2 Y_CB_CR
#define MI_SP2_DMA_Y_PIC_START_AD              (MRV_MI_BASE + 0x0298)     //0X1598                                              //(MRV_MI_BASE + 0x00C8)
#define MI_SP2_DMA_CB_PIC_START_AD             (MRV_MI_BASE + 0x02A8)     //0X15A8                                              //(MRV_MI_BASE + 0x00D8)
#define MI_SP2_DMA_CR_PIC_START_AD             (MRV_MI_BASE + 0x02AC)     //0X15AC                                              //(MRV_MI_BASE + 0x00E8)

//SP2 RAW
#define MI_SP2_DMA_RAW_PCI_START_AD            (MRV_MI_BASE + 0x02BC)     //0X15BC

//MCM RAW
#define MI_MCM_DMA_RAW_PIC_START_AD            (MRV_MI_BASE + 0x036C)     //0X166C

//FE DMA
#define ISP_REG_FE_DMA_START_AD                0x3D68

/*OUTPUT_BUFFER*/
//MP PATH mi_mp_y_size_init
#define MI_MP_Y_BASE_AD_INIT                   (MRV_MI_BASE + 0x0024)     //0X1324                                            //(MRV_MI_BASE + 0x0008)
#define MI_MP_Y_BASE_AD_INIT2                  (MRV_MI_BASE + 0x0058)     //0X1358                                            //(MRV_MI_BASE + 0x0130)
#define MI_MP_Y_BASE_AD                        (MRV_MI_BASE + 0x0070)     //0x1370  mi_mp_y_base_ad_shd                       //(MRV_MI_BASE + 0x0078)

#define MI_MP_CB_BASE_AD_INIT                  (MRV_MI_BASE + 0x0040)     //0X1340                                            //(MRV_MI_BASE + 0x001C)
#define MI_MP_CB_BASE_AD_INIT2                 (MRV_MI_BASE + 0x005C)     //0X135C                                            //(MRV_MI_BASE + 0x0134)
#define MI_MP_CB_BASE_AD                       (MRV_MI_BASE + 0x007C      //0x137C  mi_mp_cb_base_ad_shd                       //(MRV_MI_BASE + 0x0088)

#define MI_MP_CR_BASE_AD_INIT                  (MRV_MI_BASE + 0x004C)     //0X134C                                            //(MRV_MI_BASE + 0x002C)
#define MI_MP_CR_BASE_AD_INIT2                 (MRV_MI_BASE + 0x0060)     //0X1360                                            //(MRV_MI_BASE + 0x0138)
#define MI_MP_CR_BASE_AD                       (MRV_MI_BASE + 0x0088)     //0x1388  mi_mp_cr_base_ad_shd                      //(MRV_MI_BASE + 0x0094)

#define MI_MP_RAW_BASE_AD_INIT                 (MRV_MI_BASE + 0x0094)     //0x1394
#define MI_MP_RAW_BASE_AD_SHD                  (MRV_MI_BASE + 0x00B4)     //0x13B4

#define MI_MP_JDP_BASE_AD_INIT                 (MRV_MI_BASE + 0x00C0)     //0x13C0
#define MI_MP_JDP_BASE_AD_SHD                  (MRV_MI_BASE + 0x00E0)     //0x13E0


//SP1 PATH
#define MI_SP1_Y_BASE_AD_INIT                  (MRV_MI_BASE + 0x0140)     //0X1440   MI_SP1_Y_BASE_AD_INIT                    //(MRV_MI_BASE + 0x003C)
#define MI_SP1_Y_BASE_AD_INIT2                 (MRV_MI_BASE + 0x0174)     //0X1474   MI_SP1_Y_BASE_AD_INIT2                   //(MRV_MI_BASE + 0x013C)
#define MI_SP1_Y_BASE_AD                       (MRV_MI_BASE + 0x018C)     //0x148c   mi_sp1_y_base_ad_shd                     //(MRV_MI_BASE + 0x00A0)

#define MI_SP1_CB_BASE_AD_INIT                 (MRV_MI_BASE + 0x015C)     //0X145C   MI_SP1_CB_BASE_AD_INIT                   //(MRV_MI_BASE + 0x0050)
#define MI_SP1_CB_BASE_AD_INIT2                (MRV_MI_BASE + 0x0178)     //0X1478   MI_SP1_CB_BASE_AD_INIT2                  //(MRV_MI_BASE + 0x0140)
#define MI_SP1_CB_BASE_AD                      (MRV_MI_BASE + 0x0198)     //0X1498   mi_sp1_cb_base_ad_shd                    //(MRV_MI_BASE + 0x00B0)

#define MI_SP1_CR_BASE_AD_INIT                 (MRV_MI_BASE + 0x0168)     //0X1468   MI_SP1_CR_BASE_AD_INIT                   //(MRV_MI_BASE + 0x0060)
#define MI_SP1_CR_BASE_AD_INIT2                (MRV_MI_BASE + 0x017C)     //OX147C   MI_SP1_CR_BASE_AD_INIT2                  //(MRV_MI_BASE + 0x0144)
#define MI_SP1_CR_BASE_AD                      (MRV_MI_BASE + 0x14A4)     //0X14A4   mi_sp1_cr_base_ad_shd                    //(MRV_MI_BASE + 0x00BC)

//MCM
#define MI_MCM_RAW0_BASE_AD_INIT               (MRV_MI_BASE + 0x0314)     //0x1614
#define MI_MCM_RAW0_BASE_AD_SHD                (MRV_MI_BASE + 0x0334)     //0x1634

#define MI_MCM_RAW1_BASE_AD_INIT               (MRV_MI_BASE + 0x0340)     //0x1640
#define MI_MCM_RAW1_BASE_AD_SHD                (MRV_MI_BASE + 0x0360)     //0x1660


//SP2 PATH
#define MI_SP2_Y_BASE_AD_INIT                  (MRV_MI_BASE + 0x01F8)     //0X14F8   MI_SP2_Y_BASE_AD_INIT                    //(MRV_MI_BASE + 0x003C)
#define MI_SP2_Y_BASE_AD_INIT2                 (MRV_MI_BASE + 0x022C)     //0X152C   MI_SP2_Y_BASE_AD_INIT2                   //(MRV_MI_BASE + 0x013C)
#define MI_SP2_Y_BASE_AD                       (MRV_MI_BASE + 0x0244)     //0x1244   mi_sp2_y_base_ad_shd                     //(MRV_MI_BASE + 0x00A0)

#define MI_SP2_CB_BASE_AD_INIT                 (MRV_MI_BASE + 0x0214)     //0X1514   MI_SP1_CB_BASE_AD_INIT                   //(MRV_MI_BASE + 0x0050)
#define MI_SP2_CB_BASE_AD_INIT2                (MRV_MI_BASE + 0x0230)     //0X1530   MI_SP1_CB_BASE_AD_INIT2                  //(MRV_MI_BASE + 0x0140)
#define MI_SP2_CB_BASE_AD                      (MRV_MI_BASE + 0x0250)     //0X1550   mi_sp1_cb_base_ad_shd                    //(MRV_MI_BASE + 0x00B0)

#define MI_SP2_CR_BASE_AD_INIT                 (MRV_MI_BASE + 0x0220)     //0X1520   MI_SP1_CR_BASE_AD_INIT                   //(MRV_MI_BASE + 0x0060)
#define MI_SP2_CR_BASE_AD_INIT2                (MRV_MI_BASE + 0x0234)     //OX1534   MI_SP1_CR_BASE_AD_INIT2                  //(MRV_MI_BASE + 0x0144)
#define MI_SP2_CR_BASE_AD                      (MRV_MI_BASE + 0x025C)     //0X155C   mi_sp1_cr_base_ad_shd                    //(MRV_MI_BASE + 0x00BC)

#define MI_SP2_RAW_BASE_AD_INIT                (MRV_MI_BASE + 0x0268)     //0x1568
#define MI_SP2_RAW_BASE_AD_SHD                 (MRV_MI_BASE + 0x028C)     //0x158C

#define MI_MP_Y_SIZE_INIT                      (MRV_MI_BASE + 0x0028)
#define MI_MP_Y_PIC_SIZE                       (MRV_MI_BASE + 0x003C)
#define MI_MP_CB_SIZE_INIT                     (MRV_MI_BASE + 0x0044)
#define MI_MP_CR_SIZE_INIT                     (MRV_MI_BASE + 0x0050)
#define MI_MP_RAW_SIZE_INIT                    (MRV_MI_BASE + 0x0098)
#define MI_MP_RAW_PIC_SIZE                     (MRV_MI_BASE + 0x00AC)
#define MI_MP_JDP_SIZE_INIT                    (MRV_MI_BASE + 0x00C4)
#define MI_MP_JDP_PIC_SIZE                     (MRV_MI_BASE + 0x00D8)

#define MI_SP1_Y_SIZE_INIT                     (MRV_MI_BASE + 0X0144)
#define MI_SP1_Y_PIC_SIZE                      (MRV_MI_BASE + 0x0158)
#define MI_SP1_CB_SIZE_INIT                    (MRV_MI_BASE + 0x0160)
#define MI_SP1_CR_SIZE_INIT                    (MRV_MI_BASE + 0x016C)

#define MI_SP2_Y_SIZE_INIT                     (MRV_MI_BASE + 0X01FC)
#define MI_SP2_Y_PIC_SIZE                      (MRV_MI_BASE + 0x0210)
#define MI_SP2_CB_SIZE_INIT                    (MRV_MI_BASE + 0x0218)
#define MI_SP2_CR_SIZE_INIT                    (MRV_MI_BASE + 0x0224)
#define MI_SP2_RAW_SIZE_INIT                   (MRV_MI_BASE + 0X026C)
#define MI_SP2_RAW_PIC_SIZE                    (MRV_MI_BASE + 0x0280)

#define MI_MP_CTRL                             (MRV_MI_BASE + 0x0010)
#define MI_SP1_CTRL                            (MRV_MI_BASE + 0x012c)
#define MI_SP2_CTRL                            (MRV_MI_BASE + 0x01e4)

//#define ISP_EXPV2_CTRL                         (0x26a0)

#define ISP_CTRL                               0x0400
#define ISP_CTRL_MASK_ISP_ENABLE               0x00000001
#define ISP_CTRL_MASK_CFG_UPD                  0x00000600

#define ISP_ACQ_NR_FRAMES                      0x0418


#define MCM_CTRL                               0x1200
#define ISP_EXPV2_CTRL                         0X26A0


#define MI_CTRL                                (MRV_MI_BASE + 0x0000)     //0X1300

#define MI_CTRL_MASK_MCM_START                 0x00008000
#define MI_CTRL_MASK_SP2_RAW_START             0x00001000

#define MRV_MI_CTRL_PATH_ENABLE_MASK            0x000000FFU               //


#define MI_MIS                                 (MRV_MI_BASE + 0x03D0)     //0X16D0

#define MRV_MI_MP_FRAME_END_MASK                0x00000007U               //0B0000_0111 :mp
#define MRV_MI_SP_FRAME_END_MASK                0x00000038U               //0B0011_1000 :sp

#define ISP_ICR                                 0x000005C8
#define MI_ICR                                  (MRV_MI_BASE + 0x03D8)     //0X16D8
#endif

#ifdef BM_VSE_MI
//modify by xzw 2019/7/5
//update vse register manual

/*DMA_INPUT*/
//M3 Y_VU_CR
#define VSE_MI3_DMA_Y_PIC_START_AD             0x000003BC
#define VSE_MI3_DMA_VU_PIC_START_AD            0x000003C0
#define VSE_MI3_DMA_CR_PIC_START_AD            0x000003C4


/*OUTPUT_BUFFER*/
//M0 PATH
#define VSE_MI0_Y_BASE_AD_INIT                 0x000001D4
#define VSE_MI0_Y_BASE_AD_INIT2                0x00000208
#define VSE_MI0_Y_BASE_AD_SHD                  0x00000220

#define VSE_MI0_CB_BASE_AD_INIT                0x000001F0
#define VSE_MI0_CB_BASE_AD_INIT2               0x0000020C
#define VSE_MI0_CB_BASE_AD_SHD                 0x0000022C

#define VSE_MI0_CR_BASE_AD_INIT                0x000001FC
#define VSE_MI0_CR_BASE_AD_INIT2               0x00000210
#define VSE_MI0_CR_BASE_AD_SHD                 0x00000238

//M1 PATH
#define VSE_MI1_Y_BASE_AD_INIT                 0x00000274
#define VSE_MI1_Y_BASE_AD_INIT2                0x000002A8
#define VSE_MI1_Y_BASE_AD_SHD                  0x000002C0

#define VSE_MI1_CB_BASE_AD_INIT                0x00000290
#define VSE_MI1_CB_BASE_AD_INIT2               0x000002AC
#define VSE_MI1_CB_BASE_AD_SHD                 0x000002CC

#define VSE_MI1_CR_BASE_AD_INIT                0x0000029C
#define VSE_MI1_CR_BASE_AD_INIT2               0x000002B0
#define VSE_MI1_CR_BASE_AD_SHD                 0x000002D8


//M2 PATH
#define VSE_MI2_Y_BASE_AD_INIT                 0x00000324
#define VSE_MI2_Y_BASE_AD_INIT2                0x00000358
#define VSE_MI2_Y_BASE_AD_SHD                  0x00000370

#define VSE_MI2_CB_BASE_AD_INIT                0x00000340
#define VSE_MI2_CB_BASE_AD_INIT2               0x0000035C
#define VSE_MI2_CB_BASE_AD_SHD                 0x0000037C

#define VSE_MI2_CR_BASE_AD_INIT                0x0000034C
#define VSE_MI2_CR_BASE_AD_INIT2               0x00000360
#define VSE_MI2_CR_BASE_AD_SHD                 0x00000388



#define VSE_CTRL                               0x00000004

#define VSE_CTRL_PATH_ENABLE_MASK              0x000000FFU

#define VSE_MI_MIS                             0x00000410

#define VSE_MI_M0_FRAME_END_MASK               0x00000001U
#define VSE_MI_M1_FRAME_END_MASK               0x00000002U
#define VSE_MI_M2_FRAME_END_MASK               0x00000004U

#endif

#define BUFFER_INIT_VALUE 0xAA

CREATE_TRACER(ISP_BM_TEST_INFO, "ISP-BM-TEST: ", INFO, 1);
CREATE_TRACER(ISP_BM_TEST_ERROR, "ISP-BM-TEST: ", ERROR, 1);

static int initBuffer(HalHandle_t, MediaBuffer_t *);
static int checkBuffer(HalHandle_t, MediaBuffer_t *);

static void BMCmdBufferDestroy(TestCtx_t *pCtx, BMCmdBuffer_t *pCmdBuffer)
{
    if (!pCtx || !pCmdBuffer)
    {
        return;
    }

    if (pCmdBuffer->registerOp)
    {
        free(pCmdBuffer->registerOp);
        pCmdBuffer->registerOp = NULL;
    }

    pCmdBuffer->cmdNum = 0;

    return;
}

static int BMMemPoolCreate(TestCtx_t *pCtx, BMMemPool_t *pMemPool)
{
    MediaBufPool_t *pBufPool = &pMemPool->bufPool;
    MediaBufPoolConfig_t *pBufPoolConfig = &pMemPool->bufPoolConfig;
    MediaBufPoolMemory_t *pBufPoolMemory = &pMemPool->bufPoolMemory;

    int ret = 0;

    if (!pCtx || !pMemPool)
    {
        return -EINVAL;
    }

    memset(pBufPoolConfig, 0, sizeof(MediaBufPoolConfig_t));
    memset(pBufPoolMemory, 0, sizeof(MediaBufPoolMemory_t));

    /* Create buffer pool. */
    //modify by xzw 2019/7/3
    //add some buffer for sp2 mcm path
    pBufPoolConfig->bufNum = 25;//9
    pBufPoolConfig->maxBufNum = 25;//9
    pBufPoolConfig->bufSize = (10 * 1024 * 1024);//(24*1024*1024)
    pBufPoolConfig->bufAlign = PIC_BUFFER_ALIGN;
    pBufPoolConfig->metaDataSizeMediaBuf = 0;
    pBufPoolConfig->metaDataSizeScmiBuf = sizeof(PicBufMetaData_t);
    pBufPoolConfig->flags = BUFPOOL_RINGBUFFER;

    ret = MediaBufPoolGetSize(pBufPoolConfig);
    assert(!ret);

    pBufPoolMemory->pMetaDataMemory = malloc(pBufPoolConfig->metaDataMemSize);
    pBufPoolMemory->pBufferMemory =
        HalAllocMemory(pCtx->hHal, pBufPoolConfig->bufMemSize);

#if 0
    ret = MediaBufPoolCreate(pBufPool, pBufPoolConfig, pBufPoolMemory);
#else
    ret = MediaBufPoolCreate(pBufPool, pBufPoolConfig, pMemPool->bufPoolMemory);
#endif

    return ret;
}

static void BMMemPoolDestroy(TestCtx_t *pCtx, BMMemPool_t *pMemPool)
{
    MediaBufPool_t *pBufPool = &pMemPool->bufPool;
    MediaBufPoolMemory_t *pBufPoolMemory = &pMemPool->bufPoolMemory;

    if (!pCtx || !pMemPool)
    {
        return;
    }

    /* Destroy buffer pool. */
    MediaBufPoolDestroy(pBufPool);
    HalFreeMemory(pCtx->hHal, pBufPoolMemory->pBufferMemory);
    free(pBufPoolMemory->pMetaDataMemory);

    pBufPoolMemory->pBufferMemory = ~0U;
    pBufPoolMemory->pMetaDataMemory = NULL;

    return;
}

int BMTestSoftReset(TestCtx_t *pCtx)
{
    if (!pCtx)
    {
        return -EINVAL;
    }

    TRACE(ISP_BM_TEST_INFO, "Soft reset... \n");

    /* VU440 board patch. */
    uint32_t data = 0U;

    /* reset */
    uint32_t reset_reg = 0U;

#if 1

#ifdef BM_ISP_MIV2
    reset_reg = 0x00308300;
#endif
#ifdef BM_VSE_MI
    reset_reg = 0x00308254;
#endif
    //vse
    data = HalReadReg(pCtx->hHal, reset_reg);

    HalWriteReg(pCtx->hHal, reset_reg, data&0xfeffffff);

    /* enable */
    data = HalReadReg(pCtx->hHal, reset_reg);
    // TRACE(ISP_BM_TEST_INFO, "Reg[0x00308300]: 0x%08X\n", data);

#ifdef BM_ISP_MIV2
         data |= 1 << 24;
#endif
#ifdef BM_VSE_MI
         data = 1 ;
#endif


    HalWriteReg(pCtx->hHal, reset_reg, data);
    data = HalReadReg(pCtx->hHal, reset_reg);
    // TRACE(ISP_BM_TEST_INFO, "Reg[0x00308300]: 0x%08X\n", data);
    //VSE ID = 'h02'
#ifdef BM_ISP_MIV2
        data = HalReadReg(pCtx->hHal, 0x00000008);
#endif
#ifdef BM_VSE_MI
        data = HalReadReg(pCtx->hHal, 0x00500000);
#endif

#endif
//isp core1 reset
#if 0
    reset_reg = 0x00308300;
    data = 0U;
    HalWriteReg(pCtx->hHal, reset_reg, data);
    /* enable */
    data = HalReadReg(pCtx->hHal, reset_reg);
    data |= 1 << 24;
    HalWriteReg(pCtx->hHal, reset_reg, data);
    data = HalReadReg(pCtx->hHal, reset_reg);
    data = HalReadReg(pCtx->hHal, 0x00100008);
#endif

    return 0;
}

int BMTestCreate(TestCtx_t *pCtx)
{
    int ret = 0;

    if (!pCtx)
    {
        return -EINVAL;
    }

    if (pCtx->pBmTest)
    {
        return -EALREADY;
    }

    pCtx->pBmTest = malloc(sizeof(BMTest_t));
    if (!pCtx->pBmTest)
    {
        TRACE(ISP_BM_TEST_ERROR, "Failed to allocate memory.\n");
        return -ENOMEM;
    }

    BMTest_t *pBmTest = pCtx->pBmTest;

    memset(pBmTest, 0, sizeof(BMTest_t));

    if (BMMemPoolCreate(pCtx, &pBmTest->memPool) < 0)
    {
        TRACE(ISP_BM_TEST_ERROR,
              "Failed to create the memory pool for bare metal test.\n");
        ret = -ENODEV;
        goto err;
    }

    for (int i = 0; i < BM_CHANNEL_MAX; i++)
    {
        pBmTest->pInputBuffer[i] =
            MediaBufPoolGetBuffer(&pBmTest->memPool.bufPool);
        if (!pBmTest->pInputBuffer[i])
        {
            TRACE(ISP_BM_TEST_ERROR, "Failed to get input buffer[%d]. \n", i);
            ret = -ENOMEM;
            goto err;
        }

        for (int j = 0; j < BM_PATH_MAX; j++)
        {
            pBmTest->pOutputBuffer[j][i] =
                MediaBufPoolGetBuffer(&pBmTest->memPool.bufPool);
            if (!pBmTest->pOutputBuffer[j][i])
            {
                TRACE(ISP_BM_TEST_ERROR, "Failed to get output buffer[%d]. \n",
                      i);
                ret = -ENOMEM;
                goto err;
            }

            pBmTest->goldenBaseAddress[j][i] = 0xFFFFFFFF;
        }
    }

    BMTestSoftReset(pCtx);

    return 0;

err:
    for (int i = 0; i < BM_CHANNEL_MAX; i++)
    {
        if (pBmTest->pInputBuffer[i])
        {
            MediaBufPoolFreeBuffer(&pBmTest->memPool.bufPool,
                                   pBmTest->pInputBuffer[i]);
            pBmTest->pInputBuffer[i] = NULL;
        }

        for (int j = 0; j < BM_PATH_MAX; j++)
        {
            if (pBmTest->pOutputBuffer[j][i])
            {
                MediaBufPoolFreeBuffer(&pBmTest->memPool.bufPool,
                                       pBmTest->pOutputBuffer[j][i]);
                pBmTest->pOutputBuffer[j][i] = NULL;
            }
        }
    }

    if (pBmTest->memPool.bufPoolMemory.pBufferMemory != ~0U)
    {
        BMMemPoolDestroy(pCtx, &pBmTest->memPool);
    }

    if (pBmTest)
    {
        free(pBmTest);
    }

    return ret;
}

void BMTestDestroy(TestCtx_t *pCtx, BMTest_t *pBmTest)
{
    if (!pCtx || !pBmTest)
    {
        TRACE(ISP_BM_TEST_ERROR, "Invalid argument.\n");
        return;
    }

    /* Destroy command buffer. */
    BMCmdBufferDestroy(pCtx, &pBmTest->cmdBuffer);

    for (int i = 0; i < BM_CHANNEL_MAX; i++)
    {
        if (pBmTest->pInputBuffer[i])
        {
            MediaBufPoolFreeBuffer(&pBmTest->memPool.bufPool,
                                   pBmTest->pInputBuffer[i]);
            pBmTest->pInputBuffer[i] = NULL;
        }

        for (int j = 0; j < BM_PATH_MAX; j++)
        {
            if (pBmTest->pOutputBuffer[j][i])
            {
                MediaBufPoolFreeBuffer(&pBmTest->memPool.bufPool,
                                       pBmTest->pOutputBuffer[j][i]);
                pBmTest->pOutputBuffer[j][i] = NULL;
            }
        }
    }

    /* Destroy memory pool. */
    BMMemPoolDestroy(pCtx, &pBmTest->memPool);

    free(pBmTest);

    return;
}

int BMTestLoadInput(TestCtx_t *pCtx, BMTestConfigIO_t *pInput, int index,
                    cJSON *pReport)
{
    BMTest_t *pBmTest = pCtx->pBmTest;

    int ret = 0;

    if (!pCtx || !pBmTest || !pInput)
    {
        return -EINVAL;
    }

    char *pFile = pInput->filename[index];

    // TRACE(ISP_BM_TEST_INFO, "Load input file, %s\n", basename(pFile));

    FILE *fp = fopen(pFile, "r");
    if (!fp)
    {
        TRACE(ISP_BM_TEST_ERROR, "Failed to open input file %s.\n", pFile);
        return -errno;
    }

    MediaBuffer_t *pMediaBuffer = pBmTest->pInputBuffer[index];

    unsigned int bytes = 0;
    unsigned int buffer_size = 0;

    ret = FLE_Lines(pFile, &bytes);
    if (ret < 0)
    {
        goto exit;
    }

    uint8_t *pMapBuffer = NULL;

    bool is_fe_cfg;
    if(strstr(pFile, "_fe_cfg_")){
        is_fe_cfg = true;
        buffer_size = bytes*4 ;
    }
    else{
        is_fe_cfg = false;
        buffer_size = bytes ;
    }

    RESULT result =
        HalMapMemory(pCtx->hHal, pMediaBuffer->buf.base_address, buffer_size,
                     HAL_MAPMEM_WRITEONLY, (void *)&pMapBuffer);
    assert(result == RET_SUCCESS);
    (void)result; //avoid compiler's complaint

    uint8_t *pData = pMapBuffer;

    uint32_t addr, data;

    for (uint32_t i = 0; i < bytes; i++)
    {
        if(is_fe_cfg){
            if (fscanf(fp, "%32x", &data) <= 0){
                break;
            }
            pData[i*4+0] = data;
            pData[i*4+1] = (data >>8);
            pData[i*4+2] = (data >>16);
            pData[i*4+3] = (data >>24);
        }

        else{
            if (fscanf(fp, "%08x %8x", &addr, &data) <= 0)
            {
                break;
            }
            pData[i] = data;
        }

    }

    result = HalUnMapMemory(pCtx->hHal, pMapBuffer);
    assert(result == RET_SUCCESS);

    cJSON_AddNumberToObject(pReport, basename(pFile), bytes);

exit:
    if (fp)
    {
        fclose(fp);
    }

    return ret;
}

    bool isp_sel = 0;

int BMTestLoadCommands(TestCtx_t *pCtx, char *pCmdFile, cJSON *pReport)
{
    FILE *fp = NULL;
    uint32_t cmdNum = 0, i;
    uint32_t regDir, regAddr, regValue;
    BMRegisterOp_t *registerOp = NULL;

    int ret = 0;

    if (!pCtx || !pCmdFile)
    {
        return -EINVAL;
    }

    BMTest_t *pBmTest = pCtx->pBmTest;

    // TRACE(ISP_BM_TEST_INFO, "Load register file, %s\n", pCmdFile);


    bool istpg   = 0;
    if (strstr(pCmdFile, "_isp1_"))
        isp_sel = 1;
    else
        isp_sel = 0;

    bool sensor_sel = 0;
    if (strstr(pCmdFile, "_sensor1_"))
        sensor_sel = 1;

    if (strstr(pCmdFile, "_tpg_"))
       istpg = 1;

    fp = fopen(pCmdFile, "r");
    if (!fp)
    {
        TRACE(ISP_BM_TEST_ERROR, "Failed to open cmd file %s.\n", pCmdFile);
        return -errno;
    }

    while (fscanf(fp, "%u %08x %08x", &regDir, &regAddr, &regValue) > 0)
    {
        cmdNum++;
    }

    cJSON *pItem = cJSON_CreateObject();
    cJSON_AddItemToObject(pReport, basename(pCmdFile), pItem);

    cJSON_AddNumberToObject(pItem, "count", cmdNum);

    if (cmdNum == 0)
    {
        TRACE(ISP_BM_TEST_ERROR, "The command buffer file is incorrect.\n");
        ret = -EINVAL;

        goto exit;
    }

    registerOp = pBmTest->cmdBuffer.registerOp;

    if (cmdNum > pBmTest->cmdBuffer.cmdNum)
    {
        if (registerOp)
        {
            free(registerOp);

            pBmTest->cmdBuffer.registerOp = NULL;
            pBmTest->cmdBuffer.cmdNum = 0;
        }

        TRACE(ISP_BM_TEST_INFO, "Allocate %d BMRegisterOP_t memory \n", cmdNum);

        registerOp = malloc(sizeof(BMRegisterOp_t) * cmdNum);
        if (!registerOp)
        {
            TRACE(ISP_BM_TEST_ERROR,
                  "Failed to allocate memory for register operations.\n");
            ret = -ENOMEM;

            goto exit;
        }

        pBmTest->cmdBuffer.registerOp = registerOp;
    }

    pBmTest->cmdBuffer.cmdNum = cmdNum;

    fseek(fp, 0, SEEK_SET);

    for (i = 0; i < cmdNum; i++)
    {
        if (fscanf(fp, "%u %08x %08x", &regDir, &regAddr, &regValue) <= 0) {
            break;
        }
        if (regDir == REG_WRITE)
        {
            registerOp[i].type = BM_REG_WRITE;
        }
        else
        {
            registerOp[i].type = BM_REG_READ;
        }
#ifdef BM_VSE_MI
        //modify by xzw 2019/7/5
        //modify by vse register
        registerOp[i].addr = 0x500000+ regAddr;
#else
        if(isp_sel)
            registerOp[i].addr =0x100000 + regAddr ;
        else
            registerOp[i].addr = regAddr;
#endif
        /* Patch the input and output. */
    #ifdef BM_ISP_MIV2
        switch (regAddr)
        {

        case MI_MP_Y_SIZE_INIT   :
        case MI_MP_Y_PIC_SIZE    :
        case MI_MP_CB_SIZE_INIT  :
        case MI_MP_CR_SIZE_INIT  :
        case MI_MP_RAW_SIZE_INIT :
        case MI_MP_RAW_PIC_SIZE  :
        case MI_MP_JDP_SIZE_INIT :
        case MI_MP_JDP_PIC_SIZE  :

        case MI_SP1_Y_SIZE_INIT  :
        case MI_SP1_Y_PIC_SIZE   :
        case MI_SP1_CB_SIZE_INIT :
        case MI_SP1_CR_SIZE_INIT :

        case MI_SP2_Y_SIZE_INIT  :
        case MI_SP2_Y_PIC_SIZE   :
        case MI_SP2_CB_SIZE_INIT :
        case MI_SP2_CR_SIZE_INIT :
        case MI_SP2_RAW_SIZE_INIT:
        case MI_SP2_RAW_PIC_SIZE :
            if(istpg)
              regValue = regValue * FRAME_NUM;
            break;

        case MI_MP_CTRL:
        case MI_SP1_CTRL:
        case MI_SP2_CTRL:
            if(istpg)
               regValue = regValue&0xfffffdf;
            break;
        //case ISP_CTRL:
        //    break;

        case MI_CTRL:
            //face++ no sp2 raw
            //regValue = regValue &0XFFFEFFDF;

            //if(istpg)
            //  regValue = regValue &0XFFFEFFDF;
            break;

        case MCM_CTRL:
            //only tpg should use this operation
            if(istpg)
              regValue = 0x0;
            break;

        case ISP_ACQ_NR_FRAMES:
            if(istpg)
               regValue = FRAME_NUM;
            break;


        //only jdp should use this opertion
        case ISP_EXPV2_CTRL:
            regValue = regValue &0XFFFFFFFD;
            break;

#if 0
        case 0x1314:
             if(((regValue&0x200) == 0x200)
              ||((regValue&0x3c)  == 0x28)
              ||((regValue&0x32)  == 0x00)
              ||((regValue&0x32)  == 0x10))
              {
               ret = -ENFILE;
               goto exit;
              }
             break;
#endif


#if 0
        case 0x1430:
        case 0x14e8:
             regValue = 0x0;
             if((regValue&0x18) == 0x10)
              {
               ret = -ENFILE;
               goto exit;
              }
            break;
#endif



        /*DMA INPUT*/
         case ISP_REG_FE_DMA_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_Y]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "ISP_REG_FE_DMA_START_AD = 0x%X \n",
                  regValue);
            break;

        case MI_SP2_DMA_Y_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_Y]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_DMA_Y_PIC_START_AD = 0x%X \n",
                  regValue);
            break;

        case MI_SP2_DMA_CB_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_CB]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_DMA_CB_PIC_START_AD = 0x%X \n",
                  regValue);
            break;

        case MI_SP2_DMA_CR_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_CR]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_DMA_CR_PIC_START_AD = 0x%X \n",
                  regValue);
            break;
        case MI_SP2_DMA_RAW_PCI_START_AD:
            if (sensor_sel){
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_RAW0]->buf.base_address;
            }
            else{
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_RAW0]->buf.base_address;
            }
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_DMA_RAW_PIC_START_AD = 0x%X \n",
                  regValue);
            break;

        case MI_MCM_DMA_RAW_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_RAW1]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MCM_DMA_RAW_PIC_START_AD = 0x%X \n",
            regValue);
            break;



        /*Output*/
        //mp
        case MI_MP_Y_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_Y]
                           ->buf.base_address;

            //regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_Y]
            //               ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_Y_BASE_AD_INIT = 0x%X \n", regValue);
            break;

        case MI_MP_CB_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_CB_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case MI_MP_CR_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_CR_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;
        case MI_MP_RAW_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_RAW0]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_RAW_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case MI_MP_JDP_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_RAW1]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_JDP_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case MI_MP_Y_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_Y_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_MP_CB_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_CB_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_MP_CR_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MP_CR_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        //SP1
        case MI_SP1_Y_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP1_Y_BASE_AD_INIT = 0x%X \n", regValue);
            break;

        case MI_SP1_CB_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP1_CB_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case MI_SP1_CR_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP1_CR_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case MI_SP1_Y_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP1_Y_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_SP1_CB_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP1_CB_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_SP1_CR_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP1_CR_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        //MCM
        case MI_MCM_RAW0_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MCM][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MCM_RAW0_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case MI_MCM_RAW1_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MCM][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_MCM_RAW1_BASE_AD_INIT = 0x%X \n",
                   regValue);
            break;


        //SP2
        case MI_SP2_Y_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_Y_BASE_AD_INIT = 0x%X \n", regValue);
            break;

        case MI_SP2_CB_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_CB_BASE_AD_INIT = 0x%X \n",
                   regValue);
            break;

        case MI_SP2_CR_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_CR_BASE_AD_INIT = 0x%X \n",
                   regValue);
            break;

        case MI_SP2_Y_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_Y_BASE_AD_INIT2 = 0x%X \n",
                   regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_SP2_CB_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CB]
                             ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_CB_BASE_AD_INIT2 = 0x%X \n",
                regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_SP2_CR_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CR]
                ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_CR_BASE_AD_INIT2 = 0x%X \n",
                regValue);
            pBmTest->isPingPangBuffer = true;
            break;

        case MI_SP2_RAW_BASE_AD_INIT:
            if (sensor_sel){
              regValue = pBmTest->pInputBuffer[BM_CHANNEL_RAW0]->buf.base_address;
            }
            else{
              regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_RAW0]->buf.base_address;
            }
            //regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_RAW0]
            //    ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "MI_SP2_RAW_BASE_AD_INIT = 0x%X \n",
                regValue);
            break;

        default:
            break;
        }
#endif
#ifdef BM_VSE_MI
        switch (regAddr)
        {
        /*DMA INPUT*/
        case VSE_MI3_DMA_Y_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_Y]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI3_DMA_Y_PIC_START_AD = 0x%X \n",
                  regValue);
            break;

        case VSE_MI3_DMA_VU_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_CB]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI3_DMA_VU_PIC_START_AD = 0x%X \n",
                  regValue);
            break;

        case VSE_MI3_DMA_CR_PIC_START_AD:
            regValue = pBmTest->pInputBuffer[BM_CHANNEL_CR]->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI3_DMA_CR_PIC_START_AD = 0x%X \n",
                  regValue);
            break;

        /*Output*/
        //M0
        case VSE_MI0_Y_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_Y_BASE_AD_INIT = 0x%X \n", regValue);
            break;

        case VSE_MI0_CB_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_CB_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case VSE_MI0_CR_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_CR_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case VSE_MI0_Y_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_Y_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        case VSE_MI0_CB_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_CB_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        case VSE_MI0_CR_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_CR_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        //M1
        case VSE_MI1_Y_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_Y_BASE_AD_INIT = 0x%X \n", regValue);
            break;

        case VSE_MI1_CB_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI0_CB_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case VSE_MI1_CR_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI1_CR_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case VSE_MI1_Y_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_Y]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI1_Y_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        case VSE_MI1_CB_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI1_CB_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        case VSE_MI1_CR_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CR]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI1_CR_BASE_AD_INIT2 = 0x%X \n",
                  regValue);
            //pBmTest->isPingPangBuffer = true;
            break;


        //M2
        case VSE_MI2_Y_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_Y]
                          ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI2_Y_BASE_AD_INIT = 0x%X \n", regValue);
            break;

        case VSE_MI2_CB_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CB]
                           ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI2_CB_BASE_AD_INIT = 0x%X \n",
                  regValue);
            break;

        case VSE_MI2_CR_BASE_AD_INIT:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CR]
                ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI2_CR_BASE_AD_INIT = 0x%X \n",
                regValue);
            break;

        case VSE_MI2_Y_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_Y]
                ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI2_Y_BASE_AD_INIT2 = 0x%X \n",
                regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        case VSE_MI2_CB_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CB]
                ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI2_CB_BASE_AD_INIT2 = 0x%X \n",
                regValue);
            //pBmTest->isPingPangBuffer = true;
            break;

        case VSE_MI2_CR_BASE_AD_INIT2:
            regValue = pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CR]
                ->buf.base_address;
            TRACE(ISP_BM_TEST_INFO, "VSE_MI2_CR_BASE_AD_INIT2 = 0x%X \n",
                regValue);
            //pBmTest->isPingPangBuffer = true;
            break;


        default:
            break;
        }
#endif
        registerOp[i].value = regValue;
    }

    if (pBmTest->isPingPangBuffer)
    {
        cJSON_AddTrueToObject(pItem, "is.ping.pang.buffer");
    }

exit:
    if (fp)
    {
        fclose(fp);
    }

    return ret;
}

int BMTestExecCommands(TestCtx_t *pCtx, BMTest_t *pBmTest)
{
    BMRegisterOp_t *registerOp;
    uint32_t i;

    int ret = 0;
    bool mcm_start = false;
    bool sp2_raw_start = false;

    if (!pCtx || !pBmTest)
    {
        return -EINVAL;
    }

    registerOp = pBmTest->cmdBuffer.registerOp;


    //HalWriteReg(pCtx->hHal, 0x00000014, 0xFFFFFFFF);
    //HalWriteReg(pCtx->hHal, 0x00000014, 0x0);
    uint32_t isp_core_offset = 0;
    if(isp_sel)
      isp_core_offset = 0x00100000;
    HalWriteReg(pCtx->hHal, 0x000016c0, 0xFFFFFFFF);

#if 0
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_MP_Y_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_Y]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_MP_CB_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CB]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_MP_CR_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_CR]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_MP_RAW_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_RAW0]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_MP_JDP_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_MAIN][BM_CHANNEL_RAW1]);

    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP1_Y_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_Y]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP1_CB_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CB]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP1_CR_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF1][BM_CHANNEL_CR]);

    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP2_Y_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_Y]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP2_CB_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CB]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP2_CR_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_CR]);
    HalWriteReg(pCtx->hHal, (isp_core_offset+MI_SP2_RAW_BASE_AD_INIT), pBmTest->pOutputBuffer[BM_PATH_SELF2][BM_CHANNEL_RAW0]);
#endif

#ifdef FE_MODE

    MediaBuffer_t *pMediaBuffer = pBmTest->pInputBuffer[0];
    uint8_t *pMapBuffer = NULL;
    RESULT result =
           HalMapMemory(pCtx->hHal, pMediaBuffer->buf.base_address, pBmTest->cmdBuffer.cmdNum*8,
                 HAL_MAPMEM_WRITEONLY, (void *)&pMapBuffer);
    assert(result == RET_SUCCESS);

    uint8_t *pData = pMapBuffer;

    for (i = 0; i < pBmTest->cmdBuffer.cmdNum; i++)
    {
        BMRegisterOp_t *pRegisterOp = registerOp + i;
        BMRegisterOpType_t type = pRegisterOp->type;

        if(pRegisterOp->addr == (isp_core_offset+MI_CTRL)){
            if(pRegisterOp->value & MI_CTRL_MASK_MCM_START){
              mcm_start = true;
            }
            if(pRegisterOp->value & MI_CTRL_MASK_SP2_RAW_START){
            sp2_raw_start = true;
            }
        }

        //pRegisterOp->addr, pRegisterOp->value
        pData[i*8+0] = (pRegisterOp->addr );
        pData[i*8+1] = (pRegisterOp->addr >>8);
        pData[i*8+2] = 0x01;
        pData[i*8+3] = 0x08;
        pData[i*8+4] = pRegisterOp->value;
        pData[i*8+5] = (pRegisterOp->value >>8);
        pData[i*8+6] = (pRegisterOp->value >>16);
        pData[i*8+7] = (pRegisterOp->value >>24);
    }
        pData[i*8+0] = 0x18;
        pData[i*8+1] = 0x00;
        pData[i*8+2] = 0x00;
        pData[i*8+3] = 0x00;
        pData[i*8+4] = 0x10;
        pData[i*8+5] = 0x00;
        pData[i*8+6] = 0x00;
        pData[i*8+7] = 0x00;

    result = HalUnMapMemory(pCtx->hHal, pMapBuffer);
    assert(result == RET_SUCCESS);

    HalWriteReg(pCtx->hHal, (isp_core_offset+0x3D60), 0x00000000);
    HalWriteReg(pCtx->hHal, (isp_core_offset+0x160C), 0x0DCABD1E);
    HalWriteReg(pCtx->hHal, (isp_core_offset+0x3D60), 0x00000001);
    HalWriteReg(pCtx->hHal, (isp_core_offset+0x3D6C), 0x00000001);
    HalWriteReg(pCtx->hHal, (isp_core_offset+0x3D68), 0x10000000);
    //HalWriteReg(pCtx->hHal, (isp_core_offset+0x3D64), (pBmTest->cmdBuffer.cmdNum + 1));
    HalWriteReg(pCtx->hHal, (isp_core_offset+0x3D64), 0x00010000|(pBmTest->cmdBuffer.cmdNum + 1));
    //HalWriteReg(pCtx->hHal, (isp_core_offset+0x0014), read_value);

#else
    for (i = 0; i < pBmTest->cmdBuffer.cmdNum; i++)
    {
        BMRegisterOp_t *pRegisterOp = registerOp + i;

        BMRegisterOpType_t type = pRegisterOp->type;

        // TRACE(ISP_BM_TEST_INFO, "%3d/%d, 0x%08X %s 0x%08X \n", i,
        //       pBmTest->cmdBuffer.cmdNum, pRegisterOp->addr, type == BM_REG_READ ? ">>" : "<<",
        //       pRegisterOp->value);
        uint32_t read_value=0;
        if (type == BM_REG_WRITE)
        {
            if((pRegisterOp->addr == (isp_core_offset+ISP_CTRL))
             &&(pRegisterOp->value & ISP_CTRL_MASK_ISP_ENABLE)
             &&(!(pRegisterOp->value & ISP_CTRL_MASK_CFG_UPD))){

                //HalWriteReg(pCtx->hHal, (isp_core_offset+ISP_ACQ_NR_FRAMES), FRAME_NUM);
             }

             if((pRegisterOp->addr == (isp_core_offset+MI_CTRL))
              &&(pRegisterOp->value & MI_CTRL_MASK_MCM_START)){
                   mcm_start = true;
                   pRegisterOp->value &= 0xffff7fff;
               if(pRegisterOp->value & MI_CTRL_MASK_SP2_RAW_START){
                   sp2_raw_start = true;
               }
             }


            int set_mi_rst = 0;
#if 0
            if((pRegisterOp->addr == (isp_core_offset+0x1314))
             ||(pRegisterOp->addr == (isp_core_offset+0x1430))
             ||(pRegisterOp->addr == (isp_core_offset+0x14e8))){

               read_value = HalReadReg(pCtx->hHal, isp_core_offset+0x0014);
               HalWriteReg(pCtx->hHal, (isp_core_offset+0x0014), read_value|0x40);
               set_mi_rst = 1;
            }

#endif

#if 0
            if(pRegisterOp->addr == (isp_core_offset+0x700)){
                HalWriteReg(pCtx->hHal, (pRegisterOp+1)->addr, (pRegisterOp+1)->value);
                if(((pRegisterOp+1)->addr == (isp_core_offset+MI_CTRL))
                 &&((pRegisterOp+1)->value & MI_CTRL_MASK_MCM_START)){
                      mcm_start = true;
                      if((pRegisterOp+1)->value & MI_CTRL_MASK_SP2_RAW_START){
                         sp2_raw_start = true;
                       }
                }
                //HalWriteReg(pCtx->hHal, pRegisterOp->addr, ((pRegisterOp->value & 0x0000ffff)|(FRAME_NUM<<16)));
                HalWriteReg(pCtx->hHal, pRegisterOp->addr, ((pRegisterOp->value)));
                break;
            }
#endif

#if 0
              if((pRegisterOp->addr == (isp_core_offset+0x2214))
               ||(pRegisterOp->addr == (isp_core_offset+0x2218))
               ||(pRegisterOp->addr == (isp_core_offset+0x221c))


||(pRegisterOp->addr == (isp_core_offset+0x2220))){
                uint32_t sample_0_data = (pRegisterOp->value & 0xfff)<<1;
                uint32_t sample_1_data = ((pRegisterOp->value & 0xfff000)>>11);
                uint32_t sample_table_data = 0;

                sample_table_data = ((sample_1_data&0x3000)<<16)|((sample_0_data&0x3000)<<12)|((sample_1_data&0x0fff)<<12)|(sample_0_data&0x0fff);
                printf("0 %08x %08x\n",pRegisterOp->addr,sample_table_data);
                //HalWriteReg(pCtx->hHal, pRegisterOp->addr, sample_table_data);
                continue;
             }

#endif


            HalWriteReg(pCtx->hHal, pRegisterOp->addr, pRegisterOp->value);

            if (set_mi_rst){
               HalWriteReg(pCtx->hHal, (isp_core_offset+0x0014), read_value);
            }


           // if(!((pRegisterOp->addr >= 0x2900)
           //   &&(pRegisterOp->addr <= 0x2a00))){
           //    read_value = HalReadReg(pCtx->hHal, pRegisterOp->addr);
           //    if(read_value != pRegisterOp->value){
           //      TRACE(ISP_BM_TEST_ERROR, "Read register %x Error.\n",pRegisterOp->addr);
           //    }
           // }
        }
        else if (type == BM_REG_READ)
        {
            HalReadReg(pCtx->hHal, pRegisterOp->addr);
        }
        else
        {
            TRACE(ISP_BM_TEST_ERROR, "Invalid register operation.\n");
            return -1;
        }
    }
#endif

    TRACE(ISP_BM_TEST_INFO, "Refresh %d registers. \n", i);

    if (i <= 0)
    {
        return -2;
    }

    uint32_t val ;

#ifdef BM_ISP_MIV2

    sleep(1);
    val = HalReadReg(pCtx->hHal, (isp_core_offset+MI_CTRL));
    if(mcm_start){
        HalWriteReg(pCtx->hHal,(isp_core_offset+MI_CTRL),val|MI_CTRL_MASK_MCM_START);
    }
    val &= MRV_MI_CTRL_PATH_ENABLE_MASK;

       uint32_t value_int = 0;

       if (mcm_start){
         if(sp2_raw_start){
            value_int = value_int + 0x800000;
         }
         value_int = value_int + 0x1000000;
       }
       value_int = value_int + (val&0xff);
       int j =0;
       /* Wait for HW to complete with main path. */
       //while (isMp && !(HalReadReg(pCtx->hHal, MI_MIS) & MRV_MI_MP_FRAME_END_MASK))
       while (value_int != (HalReadReg(pCtx->hHal, (isp_core_offset+MI_MIS)) & 0x018000FF))
       {
           HalReadReg(pCtx->hHal, (isp_core_offset+0x05c4));
           if(j==10){
               j=0;
               break;
           }
           sleep(1);
           j++;
       }

       HalReadReg(pCtx->hHal, (isp_core_offset+0x05c4));
       HalReadReg(pCtx->hHal, (isp_core_offset+MI_MIS));

       //while (isSp && !(HalReadReg(pCtx->hHal, MI_MIS) & MRV_MI_SP_FRAME_END_MASK))
       //{
       //    if(j==10){
       //        j=0;
       //        break;
       //    }
       //    sleep(1);
       //    j++;
       //}
#if 0
       //CLEAR AXI ENABLE
       val = HalReadReg(pCtx->hHal, 0x131c) ;
       HalWriteReg(pCtx->hHal, 0x131c,val&0xFDFFFFFF);


       val = HalReadReg(pCtx->hHal, 0x1438) ;
       HalWriteReg(pCtx->hHal, 0x1438,val&0xFDFFFFFF);

       val = HalReadReg(pCtx->hHal, 0x14f0) ;
       HalWriteReg(pCtx->hHal, 0x14f0,val&0xF7FFFFFF);

       val = HalReadReg(pCtx->hHal, 0x160c) ;
       HalWriteReg(pCtx->hHal, 0x160c,val&0xF7FFFFFF);
#endif
       //clear  interrupt
       HalWriteReg(pCtx->hHal, (isp_core_offset+ISP_ICR),0xFFFFFFFF);
       HalWriteReg(pCtx->hHal, (isp_core_offset+ISP_ICR),0x00000000);
       HalWriteReg(pCtx->hHal, (isp_core_offset+MI_ICR), 0xFFFFFFFF);
       HalWriteReg(pCtx->hHal, (isp_core_offset+MI_ICR), 0x00000000);


#endif

#ifdef BM_VSE_MI
    //modify by xzw 2019/7/5
    //update by VSE register manual
    val = HalReadReg(pCtx->hHal, 0x500000+VSE_CTRL) & VSE_CTRL_PATH_ENABLE_MASK;

    bool isM0 = false;
    bool isM1 = false;
    bool isM2 = false;


   if (val&0x01)
      isM0 = true;
   if (val&0x02)
      isM1 = true;
   if (val&0x04)
      isM2 = true;
   int j=0;
   /* Wait for HW to complete with main path. */
    while (isM0 && !(HalReadReg(pCtx->hHal, 0x500000+VSE_MI_MIS) & VSE_MI_M0_FRAME_END_MASK))
       {
             HalReadReg(pCtx->hHal, 0x500004+VSE_MI_MIS) ;
             if(j==10){
                 j=0;
                 break;
             }
             sleep(1);
             j++;
       }

   while (isM1 && !(HalReadReg(pCtx->hHal, 0x500000+VSE_MI_MIS) & VSE_MI_M1_FRAME_END_MASK))
       {
             HalReadReg(pCtx->hHal, 0x500004+VSE_MI_MIS) ;
             if(j==10){
                 j=0;
                 break;
             }
             sleep(1);
             j++;
       }

  while (isM2 && !(HalReadReg(pCtx->hHal, 0x500000+VSE_MI_MIS) & VSE_MI_M2_FRAME_END_MASK))
       {

             HalReadReg(pCtx->hHal, 0x500004+VSE_MI_MIS) ;
             if(j==10){
                 j=0;
                 break;
             }
             sleep(1);
             j++;
       }

#endif






    return ret;
}

int BMTestSaveOutput(TestCtx_t *pCtx, BMTestConfig_t *pTestConfig,
                     BMG_AnalyzeResult_t *pAnalyzeResult, int pathIndex,
                     int index)
{

    int ret = 0;

    if (!pCtx || !pTestConfig || !pAnalyzeResult)
    {
        return -EINVAL;
    }

    BMTest_t *pBmTest = pCtx->pBmTest;
    assert(pBmTest);


    MediaBuffer_t *pMediaBuffer = pBmTest->pOutputBuffer[pathIndex][index];
    if (!pMediaBuffer)
    {
        return -ENOMEM;
    }

    uint32_t *pGoldenBaseAddress =
        &pBmTest->goldenBaseAddress[pathIndex][index];

    if (*pGoldenBaseAddress == 0xFFFFFFFF)
    {
        *pGoldenBaseAddress = pAnalyzeResult->startAddr;
    }

    BMTestConfigIO_t *pOutput = &pTestConfig->output[pathIndex];

    FILE *pGolden = fopen(pOutput->filename[index], "r");
    if (!pGolden)
    {
        return -ENFILE;
    }
    if(strstr(pOutput->filename[index], "sensorid1_mi_sp2_out_data_raw"))
        pMediaBuffer = pBmTest->pInputBuffer[BM_CHANNEL_RAW0];

    BMG_SeekBufferStart(pGolden, pAnalyzeResult);

     TRACE(ISP_BM_TEST_INFO, "Save output file, %s.out\n",
     basename(pOutput->filename[index]));

    char out[MAX_LEN] = {0};
    const char *filename =  pOutput->filename[index];
    snprintf(out, MAX_LEN, "%s.out", filename);
   // snprintf(buffer, MAX_LEN, "%s.buffer", pOutput->filename[index]);

    FILE *pOut = fopen(out, "w");
   // FILE *pBuffer = fopen(buffer, "w");
    if (!pOut)
    {
        ret = -ENOMEM;
        goto exit;
    }

   // if (!pBuffer)
   // {
   //     ret = -ENOMEM;
   //     goto exit;
   // }

    uint8_t *pMapBuffer = NULL;

    RESULT result = HalMapMemory(pCtx->hHal, pMediaBuffer->buf.base_address,
                                 pMediaBuffer->buf.size, HAL_MAPMEM_READONLY,
                                 (void *)&pMapBuffer);
    assert(result == RET_SUCCESS);
    (void)result; //avoid compiler's complaint

    unsigned int addr = 0;
    unsigned int value = 0;
    unsigned int offset = 0;

    int frame_id = 0 ;
    if(strstr(pOutput->filename[index],"_tpg_")){
      if(strstr(pOutput->filename[index],"_frame1"))
          frame_id = 1;
      else if(strstr(pOutput->filename[index],"_frame2"))
          frame_id = 2;
      else if(strstr(pOutput->filename[index],"_frame3"))
          frame_id = 3;
      else
          frame_id = 0;

      if(strstr(pOutput->filename[index],"_sensorid1_"))
          frame_id ++;
    }
    else
        frame_id = 0;


    pBmTest->isPingPangBuffer = false;
     uint32_t val ;
     if(pathIndex == 0)
     val = HalReadReg(pCtx->hHal, MI_MP_Y_SIZE_INIT)/FRAME_NUM;
     else if(pathIndex == 1)
     val = HalReadReg(pCtx->hHal, MI_SP1_Y_SIZE_INIT)/FRAME_NUM;
     else if(pathIndex == 2)
     val = HalReadReg(pCtx->hHal, MI_SP2_Y_SIZE_INIT)/FRAME_NUM;

     //if(index == 0)
     //   val  =  val>>4;
     //else
        val  = pAnalyzeResult->bufferBytes;

    for (uint32_t i = 0; i < (pAnalyzeResult->bufferBytes); i++)
    //for (uint32_t i = 0; i < 1920*1080*4; i++)
    {
        if (fscanf(pGolden, "%08x %08x", &addr, &value) <= 0) {
            break;
        }

        if (pBmTest->isPingPangBuffer)
        {
            offset = i;
        }
        else
        {
            //modify by xzw 2019/7/8
            //modify by error segmentation fault
            // offset = addr - *pGoldenBaseAddress;
            //offset = addr - *pGoldenBaseAddress + frame_id * (pAnalyzeResult->bufferBytes);
            offset = addr - *pGoldenBaseAddress + frame_id * val;
        }

        fprintf(pOut, "%08x %08x\n", offset, *(pMapBuffer + offset));
        //fprintf(pBuffer, "%08x %08x\n", i, *(pMapBuffer - 1280 + i));
    }

    result = HalUnMapMemory(pCtx->hHal, pMapBuffer);
    assert(result == RET_SUCCESS);

exit:
    pBmTest->goldenBaseAddress[pathIndex][index] = 0xFFFFFFFF;

    if (pOut)
    {
        fclose(pOut);
        pOut = NULL;
    }


   // if (pBuffer)
   // {
   //     fclose(pBuffer);
   //     pBuffer = NULL;
   // }

    if (pGolden)
    {
        fclose(pGolden);
        pGolden = NULL;
    }

    return ret;
}

int BMTestInitAllBuffers(TestCtx_t *pCtx)
{
    assert(pCtx);
    //BMTestSoftReset(pCtx);

    BMTest_t *pBmTest = pCtx->pBmTest;
    assert(pBmTest);

    for (int i = 0; i < BM_CHANNEL_MAX; i++)
    {
        initBuffer(pCtx->hHal, pBmTest->pInputBuffer[i]);

        for (int j = 0; j < BM_PATH_MAX; j++)
        {
            initBuffer(pCtx->hHal, pBmTest->pOutputBuffer[j][i]);
        }
    }

    return 0;
}

int BMTestCheckAllBuffers(TestCtx_t *pCtx)
{
    assert(pCtx);

    BMTest_t *pBmTest = pCtx->pBmTest;
    assert(pBmTest);

    for (int i = 0; i < BM_CHANNEL_MAX; i++)
    {
        checkBuffer(pCtx->hHal, pBmTest->pInputBuffer[i]);

        for (int j = 0; j < BM_PATH_MAX; j++)
        {
            checkBuffer(pCtx->hHal, pBmTest->pOutputBuffer[j][i]);
        }
    }

    return 0;
}

static int initBuffer(HalHandle_t hHal, MediaBuffer_t *pMediaBuffer)
{
    assert(hHal);
    assert(pMediaBuffer);

    uint8_t *pMapBuffer = NULL;

    RESULT result = HalMapMemory(hHal, pMediaBuffer->buf.base_address,
                                 pMediaBuffer->buf.size, HAL_MAPMEM_WRITEONLY,
                                 (void *)&pMapBuffer);
    assert(result == RET_SUCCESS);
    (void)result; //avoid compiler's complaint

    memset(pMapBuffer, BUFFER_INIT_VALUE, pMediaBuffer->buf.size);

    result = HalUnMapMemory(hHal, pMapBuffer);
    assert(result == RET_SUCCESS);

    return 0;
}

static int checkBuffer(HalHandle_t hHal, MediaBuffer_t *pMediaBuffer)
{
    assert(hHal);
    assert(pMediaBuffer);

    uint8_t *pMapBuffer = NULL;

    RESULT result = HalMapMemory(hHal, pMediaBuffer->buf.base_address,
                                 pMediaBuffer->buf.size, HAL_MAPMEM_WRITEONLY,
                                 (void *)&pMapBuffer);
    assert(result == RET_SUCCESS);
    (void)result; //avoid compiler's complaint

    for (uint32_t i = 0; i < pMediaBuffer->buf.size; i++) {
        assert(*(pMapBuffer + i) == BUFFER_INIT_VALUE);
    }

    result = HalUnMapMemory(hHal, pMapBuffer);
    assert(result == RET_SUCCESS);

    return 0;
}
