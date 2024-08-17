/*
 * Copyright 2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __LPA_POWER_MANAGEMENT_H__
#define __LPA_POWER_MANAGEMENT_H__

#include "fsl_debug_console.h"
#include "platform_def.h"
#include "ddrc.h"
#include "mmio.h"
#include "dram.h"
#include "fsl_sema4.h"

#define SEMA4_0 ((void*) 0x30AC0000)
// Value that must be used to take a semaphore
#define G_PROC_ID 1

#define ServiceFlagAddr SRC->GPR10

#define SRC_IPS_BASE_ADDR       IMX_SRC_BASE
#define SRC_DDRC_RCR_ADDR	(SRC_IPS_BASE_ADDR + 0x1000)

//#define GPC_PU_PWRHSK		(IMX_GPC_BASE + 0x01FC)
#define CCM_SRC_CTRL_OFFSET     (IMX_CCM_BASE + 0x800)
#define CCM_CCGR_OFFSET         (IMX_CCM_BASE + 0x4000)
#define CCM_SRC_CTRL(n)		(CCM_SRC_CTRL_OFFSET + 0x10 * n)
#define CCM_CCGR(n)		(CCM_CCGR_OFFSET + 0x10 * n)


#define IMX_CCM_CORE_BASE			(IMX_CCM_BASE + 0x8000)
#define IMX_CCM_BUS_BASE			(IMX_CCM_BASE + 0x8800)
#define IMX_CCM_IP_BASE				(IMX_CCM_BASE + 0xa000)
#define CCM_CORE_CLK_ROOT_GEN_TAGET(i)		(IMX_CCM_CORE_BASE + 0x80 * (i) + 0x00)
#define CCM_CORE_CLK_ROOT_GEN_TAGET_SET(i)	(IMX_CCM_CORE_BASE + 0x80 * (i) + 0x04)
#define CCM_CORE_CLK_ROOT_GEN_TAGET_CLR(i)	(IMX_CCM_CORE_BASE + 0x80 * (i) + 0x08)

#define CCM_BUS_CLK_ROOT_GEN_TAGET(i)		(IMX_CCM_BUS_BASE + 0x80 * (i) + 0x00)
#define CCM_BUS_CLK_ROOT_GEN_TAGET_SET(i)	(IMX_CCM_BUS_BASE + 0x80 * (i) + 0x04)
#define CCM_BUS_CLK_ROOT_GEN_TAGET_CLR(i)	(IMX_CCM_BUS_BASE + 0x80 * (i) + 0x08)

#define CCM_IP_CLK_ROOT_GEN_TAGET(i)		(IMX_CCM_IP_BASE + 0x80 * (i) + 0x00)
#define CCM_IP_CLK_ROOT_GEN_TAGET_SET(i)	(IMX_CCM_IP_BASE + 0x80 * (i) + 0x04)
#define CCM_IP_CLK_ROOT_GEN_TAGET_CLR(i)	(IMX_CCM_IP_BASE + 0x80 * (i) + 0x08)

#if 0
/* BSC */
#define LPCR_A53_BSC			0x0
#define LPCR_A53_BSC2			0x108
//#define LPCR_M4				0x8
#define SLPCR				0x14
#define SLPCR_EN_DSM			(1 << 31)
#define SLPCR_RBC_EN			(1 << 30)
#define SLPCR_A53_FASTWUP_STOP		(1 << 17)
#define SLPCR_A53_FASTWUP_WAIT		(1 << 16)
#define SLPCR_VSTBY			(1 << 2)
#define SLPCR_SBYOS			(1 << 1)
#define SLPCR_BYPASS_PMIC_READY		0x1

#define A53_LPM_WAIT			0x5
#define A53_LPM_STOP			0xa
#define A53_CLK_ON_LPM			(1 << 14)

#define MST_CPU_MAPPING			0x18

#define SRC_GPR1_OFFSET			0x74

#endif




void power_management_init(void);

void power_management_after_ddr_usage(void);

void power_management_before_ddr_usage(void);


void sema4_lock(uint8_t id);
void sema4_unlock(uint8_t id);

#endif
