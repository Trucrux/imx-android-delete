/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "pin_mux.h"
#include "app_srtm.h"
#include "fsl_gpc.h"
#include "sai_low_power_audio.h"
#include "lpm.h"
#include "fsl_debug_console.h"
#include "clock_config.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_rdc.h"
#include "lpa_power_management.h"
#include "mmio.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define RDC_DISABLE_A53_ACCESS 0xFF
#define I2C_RELEASE_SDA_GPIO   GPIO5
#define I2C_RELEASE_SDA_PIN    19U
#define I2C_RELEASE_SCL_GPIO   GPIO5
#define I2C_RELEASE_SCL_PIN    18U
#define I2C_RELEASE_BUS_COUNT  100U
#define RDC_DISABLE_M7_ACCESS  0xF3
static LPM_POWER_STATUS_M7 m7_lpm_state = LPM_M7_STATE_RUN;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_I2C_ReleaseBus(void);
extern volatile app_srtm_state_t srtmState;

/*******************************************************************************
 * Code
 ******************************************************************************/
void BOARD_PeripheralRdcSetting(void)
{
    rdc_domain_assignment_t assignment = {0};
    rdc_periph_access_config_t periphConfig;

    assignment.domainId = BOARD_DOMAIN_ID;

    /* Only configure the RDC if the RDC peripheral write access is allowed. */
    if ((0x1U & RDC_GetPeriphAccessPolicy(RDC, kRDC_Periph_RDC, assignment.domainId)) != 0U)
    {
        RDC_SetMasterDomainAssignment(RDC, kRDC_Master_SDMA3_PERIPH, &assignment);
        RDC_SetMasterDomainAssignment(RDC, kRDC_Master_SDMA3_BURST, &assignment);
        RDC_SetMasterDomainAssignment(RDC, kRDC_Master_SDMA3_SPBA2, &assignment);

        RDC_GetDefaultPeriphAccessConfig(&periphConfig);
        /* Do not allow the A53 domain(domain0) to access the following peripherals */
        periphConfig.policy = RDC_DISABLE_A53_ACCESS;
        periphConfig.periph = kRDC_Periph_SAI5;
        RDC_SetPeriphAccessConfig(RDC, &periphConfig);
        periphConfig.periph = kRDC_Periph_UART4;
        RDC_SetPeriphAccessConfig(RDC, &periphConfig);
        periphConfig.periph = kRDC_Periph_GPT1;
        RDC_SetPeriphAccessConfig(RDC, &periphConfig);
        periphConfig.periph = kRDC_Periph_SDMA3;
        RDC_SetPeriphAccessConfig(RDC, &periphConfig);
        periphConfig.periph = kRDC_Periph_I2C3;
        RDC_SetPeriphAccessConfig(RDC, &periphConfig);
    }
}

static void i2c_release_bus_delay(void)
{
    uint32_t i = 0;
    for (i = 0; i < I2C_RELEASE_BUS_COUNT; i++)
    {
        __NOP();
    }
}

void BOARD_I2C_ReleaseBus(void)
{
    uint8_t i                    = 0;
    gpio_pin_config_t pin_config = {kGPIO_DigitalOutput, 1, kGPIO_NoIntmode};

    IOMUXC_SetPinMux(IOMUXC_I2C3_SCL_GPIO5_IO18, 0U);
    IOMUXC_SetPinConfig(IOMUXC_I2C3_SCL_GPIO5_IO18, IOMUXC_SW_PAD_CTL_PAD_DSE(3U) | IOMUXC_SW_PAD_CTL_PAD_FSEL_MASK |
                                                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK);
    IOMUXC_SetPinMux(IOMUXC_I2C3_SDA_GPIO5_IO19, 0U);
    IOMUXC_SetPinConfig(IOMUXC_I2C3_SDA_GPIO5_IO19, IOMUXC_SW_PAD_CTL_PAD_DSE(3U) | IOMUXC_SW_PAD_CTL_PAD_FSEL_MASK |
                                                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK);
    GPIO_PinInit(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, &pin_config);
    GPIO_PinInit(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, &pin_config);

    /* Drive SDA low first to simulate a start */
    GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 0U);
    i2c_release_bus_delay();

    /* Send 9 pulses on SCL and keep SDA high */
    for (i = 0; i < 9; i++)
    {
        GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 0U);
        i2c_release_bus_delay();

        GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 1U);
        i2c_release_bus_delay();

        GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 1U);
        i2c_release_bus_delay();
    }

    /* Send stop */
    GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 1U);
    i2c_release_bus_delay();

    GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 1U);
    i2c_release_bus_delay();
}

/*
 * Give readable string of current M7 lpm state
 */
const char *LPM_MCORE_GetPowerStatusString(void)
{
    switch (m7_lpm_state)
    {
        case LPM_M7_STATE_RUN:
            return "RUN";
        case LPM_M7_STATE_WAIT:
            return "WAIT";
        case LPM_M7_STATE_STOP:
            return "STOP";
        default:
            return "UNKNOWN";
    }
}

void LPM_MCORE_ChangeM7Clock(LPM_M7_CLOCK_SPEED target)
{
    /* Change CCM Root to change M7 clock*/
    switch (target)
    {
        case LPM_M7_LOW_FREQ:
            if (CLOCK_GetRootMux(kCLOCK_RootM7) != kCLOCK_M7RootmuxOsc24M)
            {
                CLOCK_SetRootMux(kCLOCK_RootM7, kCLOCK_M7RootmuxOsc24M);
                CLOCK_SetRootDivider(kCLOCK_RootM7, 1U, 1U);
            }
            break;
        case LPM_M7_HIGH_FREQ:
            if (CLOCK_GetRootMux(kCLOCK_RootM7) != kCLOCK_M7RootmuxSysPll1)
            {
                CLOCK_SetRootDivider(kCLOCK_RootM7, 2U, 1U);
                CLOCK_SetRootMux(kCLOCK_RootM7, kCLOCK_M7RootmuxSysPll1); /* switch cortex-m7 to SYSTEM PLL1 */
            }
            break;
        default:
            break;
    }
}

void LPM_MCORE_SetPowerStatus(GPC_Type *base, LPM_POWER_STATUS_M7 targetPowerMode)
{
    gpc_lpm_config_t config;
    config.enCpuClk              = false;
    config.enFastWakeUp          = false;
    config.enDsmMask             = false;
    config.enWfiMask             = false;
    config.enVirtualPGCPowerdown = true;
    config.enVirtualPGCPowerup   = true;
    switch (targetPowerMode)
    {
        case LPM_M7_STATE_RUN:
            GPC->LPCR_M7 = GPC->LPCR_M7 & (~GPC_LPCR_M7_LPM0_MASK);
            break;
        case LPM_M7_STATE_WAIT:
            GPC_EnterWaitMode(GPC, &config);
            break;
        case LPM_M7_STATE_STOP:
            GPC_EnterStopMode(GPC, &config);
            break;
        default:
            break;
    }

    m7_lpm_state = targetPowerMode;
}
void PreSleepProcessing(void)
{
    APP_SRTM_Suspend();
    DbgConsole_Deinit();

}

void PostSleepProcessing(void)
{
    APP_SRTM_Resume();
    DbgConsole_Init(BOARD_DEBUG_UART_INSTANCE, BOARD_DEBUG_UART_BAUDRATE, BOARD_DEBUG_UART_TYPE, BOARD_DEBUG_UART_CLK_FREQ);
}

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    uint32_t irqMask;
    uint64_t counter = 0;
    uint32_t timeoutTicks;
    uint32_t timeoutMilliSec = (uint64_t)1000 * xExpectedIdleTime / configTICK_RATE_HZ;

    irqMask = DisableGlobalIRQ();

    /* Only when no context switch is pending and no task is waiting for the scheduler
     * to be unsuspended then enter low power entry.
     */
    if (eTaskConfirmSleepModeStatus() != eAbortSleep)
    {
        timeoutTicks = LPM_EnterTicklessIdle(timeoutMilliSec, &counter);
        if (timeoutTicks)
        {
            if (APP_SRTM_ServiceIdle() && LPM_AllowSleep())
            {

                LPM_MCORE_ChangeM7Clock(LPM_M7_LOW_FREQ);
                LPM_MCORE_SetPowerStatus(BOARD_GPC_BASEADDR, LPM_M7_STATE_STOP);
                PreSleepProcessing();
                __DSB();
                __ISB();
                __WFI();
                PostSleepProcessing();
                LPM_MCORE_SetPowerStatus(BOARD_GPC_BASEADDR, LPM_M7_STATE_RUN);
            }
            else
            {
                __DSB();
                __ISB();
                __WFI();
            }
        }
        LPM_ExitTicklessIdle(timeoutTicks, counter);
    }

    EnableGlobalIRQ(irqMask);
}
void MainTask(void *pvParameters)
{
    /*
     * Wait For A53 Side Become Ready
     */
    PRINTF("********************************\r\n");
    PRINTF(" Wait the Linux kernel boot up to create the link between M core and A core.\r\n");
    PRINTF("\r\n");
    PRINTF("********************************\r\n");
    while (srtmState != APP_SRTM_StateLinkedUp)
        ;
    PRINTF("The rpmsg channel between M core and A core created!\r\n");
    PRINTF("********************************\r\n");
    PRINTF("\r\n");

    /* Configure GPC */
    GPC_Init(BOARD_GPC_BASEADDR, APP_PowerUpSlot, APP_PowerDnSlot);
    GPC_EnableIRQ(BOARD_GPC_BASEADDR, BOARD_MU_IRQ);
    GPC_EnableIRQ(BOARD_GPC_BASEADDR, SYSTICK_IRQn);
    GPC_EnableIRQ(BOARD_GPC_BASEADDR, SDMA3_IRQn);

    while (true)
    {
        /* Use App task logic to replace vTaskDelay */
        PRINTF("\r\nTask %s is working now.\r\n", (char *)pvParameters);
        vTaskDelay(portMAX_DELAY);
    }
}
/*!
 * @brief Main function
 */
int main(void)
{
    char *taskID = "A";
    uint32_t i = 0;

    /* M7 has its local cache and enabled by default,
     * need to set smart subsystems (0x28000000 ~ 0x3FFFFFFF)
     * non-cacheable before accessing this address region */
    BOARD_InitMemory();

    /* Board specific RDC settings */
    BOARD_RdcInit();
    BOARD_PeripheralRdcSetting();

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_I2C_ReleaseBus();
    BOARD_I2C_ConfigurePins();
    BOARD_InitDebugConsole();

    CLOCK_SetRootMux(kCLOCK_RootSai5, kCLOCK_SaiRootmuxOsc24M);    /* When boot set SAI5 derived from 24M */
    CLOCK_SetRootDivider(kCLOCK_RootSai5, 1U, 16U);                /* Only use this divider when audio PLL1/2 correctly enabled, no set this later*/
    CLOCK_SetRootMux(kCLOCK_RootI2c3, kCLOCK_I2cRootmuxOsc24M);    /* Set I2C source to 24MHZ */
    CLOCK_SetRootDivider(kCLOCK_RootI2c3, 1U, 1U);                 /* Set root clock to 24MHZ */
    CLOCK_SetRootMux(kCLOCK_RootGpt1, kCLOCK_GptRootmuxOsc24M);     /* Set GPT source to Osc24 MHZ */
    CLOCK_SetRootDivider(kCLOCK_RootGpt1, 1U, 1U);
    /* Enable the Audio clock on M7 side to make sure the AUDIOMIX domain clock on
     * after A core enters suspend */
    CLOCK_EnableClock(kCLOCK_Audio);
    /* SAI bit clock source */
    AUDIOMIX_AttachClk(AUDIOMIX, kAUDIOMIX_Attach_SAI5_MCLK1_To_SAI5_ROOT);

    /*
     * In order to wakeup M7 from LPM, all PLLCTRLs need to be set to "NeededRun"
     */
    for (i = 0; i != 39; i++)
    {
        CCM->PLL_CTRL[i].PLL_CTRL = kCLOCK_ClockNeededRun;
    }


    CCM->CCGR[4].CCGR  = kCLOCK_ClockNeededAll; /* DEBUG */
    CCM->CCGR[15].CCGR = kCLOCK_ClockNotNeeded; /* GPIO5 */
    CCM->CCGR[27].CCGR = kCLOCK_ClockNeededAll; /* IOMUX */
    CCM->CCGR[28].CCGR = kCLOCK_ClockNeededAll; /* IPMUX1 */
    CCM->CCGR[29].CCGR = kCLOCK_ClockNeededAll; /* IPMUX2 */
    CCM->CCGR[30].CCGR = kCLOCK_ClockNeededAll; /* IPMUX3 */
    CCM->CCGR[35].CCGR = kCLOCK_ClockNeededAll; /* OCRAM */
    CCM->CCGR[36].CCGR = kCLOCK_ClockNeededAll; /* OCRAM_S */
    CCM->CCGR[60].CCGR = kCLOCK_ClockNeededAll; /* SEC DEBUG */
    CCM->CCGR[64].CCGR = kCLOCK_ClockNeededAll;
    CCM->CCGR[65].CCGR = kCLOCK_ClockNeededAll;
    CCM->CCGR[66].CCGR = kCLOCK_ClockNeededAll;  /* S main  */
    CCM->CCGR[67].CCGR = kCLOCK_ClockNeededAll;
    CCM->CCGR[68].CCGR = kCLOCK_ClockNeededAll;
    CCM->CCGR[88].CCGR = kCLOCK_ClockNeededAll; /*NOC_WRAPPER*/
    CCM->CCGR[47].CCGR = kCLOCK_ClockNeededAll;  /* FlexSPI?! */


    PRINTF("\r\n####################  LOW POWER AUDIO TASK ####################\n\r\n");
    PRINTF("    Build Time: %s--%s \r\n", __DATE__, __TIME__);
    LPM_MCORE_ChangeM7Clock(LPM_M7_HIGH_FREQ);
    power_management_init();
    PRINTF("\r\nLower Power management INT ...\r\n");

    APP_SRTM_Init();

    if (xTaskCreate(MainTask, "Main Task", 256U, (void *)taskID, tskIDLE_PRIORITY + 1U, NULL) != pdPASS)
    {
        PRINTF("\r\nFailed to create MainTask task\r\n");
        while (1)
            ;
    }

    /* Start FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Application should never reach this point. */
    for (;;)
    {
    }
}
void vApplicationMallocFailedHook(void)
{
    PRINTF("Malloc Failed!!!\r\n");
}
