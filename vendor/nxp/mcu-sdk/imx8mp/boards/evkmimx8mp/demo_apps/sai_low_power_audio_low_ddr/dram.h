/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __DRAM_H__
#define __DRAM_H__
/*
#include <utils_def.h>
#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <console.h>
#include <context.h>
#include <context_mgmt.h>
#include <debug.h>
#include <stdbool.h>
#include <mmio.h>
#include <platform.h>
#include <platform_def.h>
#include <plat_imx8.h>
#include <xlat_tables.h>
#include <soc.h>
#include <tzc380.h>
#include <imx_csu.h>
#include <imx_rdc.h>
#include <uart.h>
*/
#define BIT(x) (1 << (x))
#define DDRPHY_REG(x)	(0x3c000000 + 4*x)

#define DDRC_LPDDR4             BIT(5)
#define DDRC_DDR4               BIT(4)
#define DDRC_DDR3L              BIT(0)
#define DDR_TYPE_MASK           U(0x3f)

#define ACTIVE_RANK_MASK        U(0x3)
#define DDRC_ACTIVE_ONE_RANK    U(0x1)
#define DDRC_ACTIVE_TWO_RANK    U(0x2)

/* reg & config param */
struct dram_cfg_param {
	unsigned int reg;
	unsigned int val;
};

#if 0
struct dram_timing_info {
	/* umctl2 config */
	struct dram_cfg_param *ddrc_cfg;
	unsigned int pad1;
	unsigned int ddrc_cfg_num;
	unsigned int pad2;
	/* ddrphy config */
	struct dram_cfg_param *ddrphy_cfg;
	unsigned int pad3;
	unsigned int ddrphy_cfg_num;
	unsigned int pad4;
	/* ddr fsp train info */
	struct dram_fsp_msg *fsp_msg;
	unsigned int pad5;
	unsigned int fsp_msg_num;
	unsigned int pad6;
	/* ddr phy trained CSR */
	struct dram_cfg_param *ddrphy_trained_csr;
	unsigned int pad7;
	unsigned int ddrphy_trained_csr_num;
	unsigned int pad8;
	/* ddr phy PIE */
	struct dram_cfg_param *ddrphy_pie;
	unsigned int pad9;
	unsigned int ddrphy_pie_num;
	unsigned int pad10;
};

struct dram_info {
	int dram_type;
	int current_fsp;
	int boot_fsp;
	struct dram_timing_info *timing_info;
};
#endif

struct dram_timing_info {
        /* umctl2 config */
        struct dram_cfg_param *ddrc_cfg;
        unsigned int dummy1;
        unsigned int ddrc_cfg_num;
        unsigned int dummy11;
        /* ddrphy config */
        struct dram_cfg_param *ddrphy_cfg;
        unsigned int dummy2;
        unsigned int ddrphy_cfg_num;
        unsigned int dummy22;
        /* ddr fsp train info */
        struct dram_fsp_msg *fsp_msg;
        unsigned int dummy3;
        unsigned int fsp_msg_num;
        unsigned int dummy33;
        /* ddr phy trained CSR */
        struct dram_cfg_param *ddrphy_trained_csr;
        unsigned int dummy4;
        unsigned int ddrphy_trained_csr_num;
        unsigned int dummy44;
        /* ddr phy PIE */
        struct dram_cfg_param *ddrphy_pie;
        unsigned int dummy5;
        unsigned int ddrphy_pie_num;
        unsigned int dummy55;
        /* initialized fsp table */
        unsigned int fsp_table[4];
};

struct dram_info {
        int dram_type;
        unsigned int num_rank;
        uint32_t num_fsp;
        int current_fsp;
        int boot_fsp;
        bool bypass_mode;
        struct dram_timing_info *timing_info;
        /* mr, emr, emr2, emr3, mr11, mr12, mr22, mr14 */
        uint32_t mr_table[3][8];
        /* used for workaround for rank to rank issue */
        uint32_t rank_setting[3][3];
};






//void lpddr4_mr_write(uint32_t, uint32_t, uint32_t);
//uint32_t lpddr4_mr_read(uint32_t, uint32_t);

void lpddr4_enter_retention(void);
void lpddr4_exit_retention(void);
void dram_exit_retention_with_target(uint32_t target);


#endif /* __DRAM_H__ */
