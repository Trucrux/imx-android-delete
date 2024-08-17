/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lpa_power_management.h"
#include "sai_low_power_audio.h"
#include "gpc_reg.h"
#define PMICI2CADD 0x4B
#define PMICI2CSUBADD 0x09 //Add buck5
#define PDVPUGPUDRAM 0x02 //bit1: sel (0: SM; 1: D[0]) bit0: EN
#define PUVPUGPUDRAM 0x03
#define PMICAUTO 0x0 //can be used as PUVPUGPUDRAM switching to auto
#define PGC_CPU_MAPPING 0x303A01D0


#define SRC_DDR1_RCR            (IMX_SRC_BASE + 0x1000)
#define SRC_DDR2_RCR            (IMX_SRC_BASE + 0x1004)

static struct dram_info dram_info;

void sema4_lock(uint8_t id)
{
    mmio_write_32(0x30384000+(16*22), 0x20); // Hs
    SEMA4_Lock(SEMA4_0, id, G_PROC_ID);
    mmio_write_32(0x30384000+(16*22), 0x0);
}

void sema4_unlock(uint8_t id)
{
    mmio_write_32(0x30384000+(16*22), 0x20); // Hs
    SEMA4_Unlock(SEMA4_0, id);
    mmio_write_32(0x30384000+(16*22), 0x0);
}

void sema4_init(void)
{
    mmio_write_32(0x30384000+(16*22), 0x20); // Hs
    SEMA4_Init(SEMA4_0);
    mmio_write_32(0x30384000+(16*22), 0x0);
}

static uint32_t fsp_init_reg[3][4] = {
        { DDRC_INIT3(0), DDRC_INIT4(0), DDRC_INIT6(0), DDRC_INIT7(0) },
        { DDRC_FREQ1_INIT3(0), DDRC_FREQ1_INIT4(0), DDRC_FREQ1_INIT6(0), DDRC_FREQ1_INIT7(0) },
        { DDRC_FREQ2_INIT3(0), DDRC_FREQ2_INIT4(0), DDRC_FREQ2_INIT6(0), DDRC_FREQ2_INIT7(0) },
};

static void get_mr_values(uint32_t (*mr_value)[8])
{
        uint32_t init_val;
        int i, fsp_index;

        for (fsp_index = 0; fsp_index < 3; fsp_index++) {
                for(i = 0; i < 4; i++) {
                        init_val = mmio_read_32(fsp_init_reg[fsp_index][i]);
                        mr_value[fsp_index][2*i] = init_val >> 16;
                        mr_value[fsp_index][2*i + 1] = init_val & 0xFFFF;
                }
        }
}

static void save_rank_setting(void)
{
        uint32_t i, offset;
        uint32_t pstate_num = dram_info.num_fsp;

        for(i = 0; i < pstate_num; i++) {
                offset = i ? (i + 1) * 0x1000 : 0;
                if (dram_info.dram_type == DDRC_LPDDR4) {
                        dram_info.rank_setting[i][0] = mmio_read_32(DDRC_DRAMTMG2(0) + offset);
                } else {
                        dram_info.rank_setting[i][0] = mmio_read_32(DDRC_DRAMTMG2(0) + offset);
                        dram_info.rank_setting[i][1] = mmio_read_32(DDRC_DRAMTMG9(0) + offset);
                }
#if !defined(PLAT_imx8mq)
                dram_info.rank_setting[i][2] = mmio_read_32(DDRC_RANKCTL(0) + offset);
#endif
        }
#if defined(PLAT_imx8mq)
        dram_info.rank_setting[0][2] = mmio_read_32(DDRC_RANKCTL(0));
#endif
}

static void rank_setting_update(void)
{
        uint32_t i, offset;
        uint32_t pstate_num = dram_info.num_fsp;

        for (i = 0; i < pstate_num; i++) {
                offset = i ? (i + 1) * 0x1000 : 0;
                if (dram_info.dram_type == DDRC_LPDDR4) {
                        mmio_write_32(DDRC_DRAMTMG2(0) + offset,
                                dram_info.rank_setting[i][0]);
                } else {
                        mmio_write_32(DDRC_DRAMTMG2(0) + offset,
                                dram_info.rank_setting[i][0]);
                        mmio_write_32(DDRC_DRAMTMG9(0) + offset,
                                dram_info.rank_setting[i][1]);
                }
#if !defined(PLAT_imx8mq)
                mmio_write_32(DDRC_RANKCTL(0) + offset,
                        dram_info.rank_setting[i][2]);
#endif
        }
#if defined(PLAT_imx8mq)
                mmio_write_32(DDRC_RANKCTL(0), dram_info.rank_setting[0][2]);
#endif
}

/* Restore the ddrc configs */
static void dram_umctl2_init(struct dram_timing_info *timing)
{
        struct dram_cfg_param *ddrc_cfg = timing->ddrc_cfg;
        int i;

        for (i =0;  i < timing->ddrc_cfg_num; i++) {
                mmio_write_32(ddrc_cfg->reg, ddrc_cfg->val);
                ddrc_cfg++;
        }
        /* set the default fsp to P0 */
        mmio_write_32(DDRC_MSTR2(0), 0x0);
}

/* Restore the dram PHY config */
static void dram_phy_init(struct dram_timing_info *timing)
{
        struct dram_cfg_param *cfg = timing->ddrphy_cfg;
        int i;

        /* Restore the PHY init config */
        cfg = timing->ddrphy_cfg;
        for (i = 0; i < timing->ddrphy_cfg_num; i++) {
                dwc_ddrphy_apb_wr(cfg->reg, cfg->val);
                cfg++;
        }

        /* Restore the DDR PHY CSRs */
        cfg = timing->ddrphy_trained_csr;
        for (i = 0; i < timing->ddrphy_trained_csr_num; i++) {
                dwc_ddrphy_apb_wr(cfg->reg, cfg->val);
                cfg++;
        }

        /* Load the PIE image */
        cfg = timing->ddrphy_pie;
        for (i = 0; i < timing->ddrphy_pie_num; i++) {
                dwc_ddrphy_apb_wr(cfg->reg, cfg->val);
                cfg++;
        }
}

static void dram_info_init(unsigned long dram_timing_base)
{
        uint32_t ddrc_mstr, current_fsp;
        int i;

        /* Get the dram type & rank */
        ddrc_mstr = mmio_read_32(DDRC_MSTR(0));

        dram_info.dram_type = ddrc_mstr & DDR_TYPE_MASK;
        dram_info.num_rank = ((ddrc_mstr >> 24) & ACTIVE_RANK_MASK) == 0x3 ?
                DDRC_ACTIVE_TWO_RANK : DDRC_ACTIVE_ONE_RANK;

      /* Get current fsp info */
        current_fsp = mmio_read_32(DDRC_DFIMISC(0));
        current_fsp = (current_fsp >> 8) & 0xf;
        dram_info.boot_fsp = current_fsp;
        

#if defined(PLAT_imx8mq)
        imx8mq_dram_timing_copy((struct dram_timing_info *)dram_timing_base);

        dram_timing_base = (unsigned long) dram_timing_saved;
#endif

        get_mr_values(dram_info.mr_table);

        dram_info.timing_info = (struct dram_timing_info *)dram_timing_base;

        /* get the num of supported fsp */
        for (i = 0; i < 4; ++i)
                if (!dram_info.timing_info->fsp_table[i])
                        break;
        dram_info.num_fsp = i;

        /* save the DRAMTMG2/9 for rank to rank workaround */
        save_rank_setting();

        /* check if has bypass mode support */
        if (dram_info.timing_info->fsp_table[i-1] < 666)
                dram_info.bypass_mode = true;
        else
                dram_info.bypass_mode = false;

        /*
         * No need to do save for ddrc and phy config register,
         * we have done it in SPL stage and save in memory
         */
        PRINTF("%08p:%8d ", dram_info.timing_info->ddrc_cfg, dram_info.timing_info->ddrc_cfg_num); //FSY: need an access to the structure, why? cache?

}

void imx_set_sys_lpm(bool retention)
{
    uint32_t val;

    /* set system DSM mode SLPCR(0x14) */
    val = mmio_read_32(IMX_GPC_BASE + SLPCR);
    val &= ~(SLPCR_EN_DSM | SLPCR_VSTBY | SLPCR_SBYOS |
            SLPCR_BYPASS_PMIC_READY | SLPCR_RBC_EN |
            SLPCR_A53_FASTWUP_STOP_MODE);

    if (retention)
        val |= (SLPCR_EN_DSM | SLPCR_VSTBY | SLPCR_SBYOS |
                SLPCR_BYPASS_PMIC_READY );

    val |= SLPCR_A53_FASTWUP_STOP_MODE;

    mmio_write_32(IMX_GPC_BASE + SLPCR, val);
}

void imx_clear_rbc_count(void)
{
    uint32_t val;

    val = mmio_read_32(IMX_GPC_BASE + SLPCR);
    val &= ~(0x3f << 24);
    mmio_write_32(IMX_GPC_BASE + SLPCR, val);
}

void dram_exit_retention_with_target(uint32_t target)
{
     /* assert all reset */
#if defined(PLAT_imx8mq)
    mmio_write_32(SRC_DDR2_RCR, 0x8F000003);
    mmio_write_32(SRC_DDR1_RCR, 0x8F00000F);
    mmio_write_32(SRC_DDR2_RCR, 0x8F000000);
#else
    mmio_write_32(SRC_DDR1_RCR, 0x8F00001F);
    mmio_write_32(SRC_DDR1_RCR, 0x8F00000F);
#endif

    switch(target){
        case 0:
            /* change the clock source of dram_apb_clk_root */
            mmio_write_32(0x3038a088, (0x7 << 24) | (0x7 << 16));
            mmio_write_32(0x3038a084, (0x4 << 24) | (0x3 << 16));
            break;
        case 1:
            /* change the clock source of dram_alt_clk_root to source 1 --400MHz */
            mmio_write_32(0x3038a008, (0x7 << 24) | (0x7 << 16));
            mmio_write_32(0x3038a004, (0x1 << 24) | (0x1 << 16));

            /* change the clock source of dram_apb_clk_root to source 3 --160MHz/2 */
            mmio_write_32(0x3038a088, (0x7 << 24) | (0x7 << 16));
            mmio_write_32(0x3038a084, (0x3 << 24) | (0x1 << 16));
            break;
        case 2:
            /* change the clock source of dram_alt_clk_root to source 2 --100MHz */
            mmio_write_32(0x3038a008, (0x7 << 24) | (0x7 << 16));
            mmio_write_32(0x3038a004, (0x2 << 24));

            /* change the clock source of dram_apb_clk_root to source 2 --40MHz/2 */
            mmio_write_32(0x3038a088, (0x7 << 24) | (0x7 << 16));
            mmio_write_32(0x3038a084, (0x2 << 24) | (0x1 << 16));
            break;

    }
    

    mmio_write_32(SRC_DDR1_RCR, 0x8F000006);

    /* ddrc re-init */
    dram_umctl2_init(dram_info.timing_info);
    /*
     * Skips the DRAM init routine and starts up in selfrefresh mode
     * Program INIT0.skip_dram_init = 2'b11
     */
    mmio_setbits_32(DDRC_INIT0(0), 0xc0000000);

    mmio_write_32(DDRC_MSTR2(0), target);
    /* Keeps the controller in self-refresh mode */
    mmio_write_32(DDRC_PWRCTL(0), 0xaa);
    mmio_write_32(DDRC_DBG1(0), 0x0);
    mmio_write_32(SRC_DDR1_RCR, 0x8F000004);
    mmio_write_32(SRC_DDR1_RCR, 0x8F000000);

    /* before write Dynamic reg, sw_done should be 0 */
    mmio_write_32(DDRC_SWCTL(0), 0x0);

#if !PLAT_imx8mn
    if (dram_info.dram_type == DDRC_LPDDR4){
        //PRINTF("LPDDR4 \n");
        mmio_write_32(DDRC_DDR_SS_GPR0, 0x01); /*LPDDR4 mode */
    }
#endif /* !PLAT_imx8mn */

    mmio_write_32(DDRC_DFIMISC(0), 0x0 | (target << 8));

    /* dram phy re-init */
    dram_phy_init(dram_info.timing_info);

    /* workaround for rank-to-rank issue */
    rank_setting_update();

    /* DWC_DDRPHYA_APBONLY0_MicroContMuxSel */
    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    while (dwc_ddrphy_apb_rd(0x20097))
        ;
    dwc_ddrphy_apb_wr(0xd0000, 0x1);

    /* before write Dynamic reg, sw_done should be 0 */
    mmio_write_32(DDRC_SWCTL(0), 0x0);


    mmio_write_32(DDRC_DFIMISC(0), 0x20 | (target << 8));
    /* wait DFISTAT.dfi_init_complete to 1 */
    while (!(mmio_read_32(DDRC_DFISTAT(0)) & 0x1))
        ;

    /* clear DFIMISC.dfi_init_start */
    mmio_write_32(DDRC_DFIMISC(0), 0x0 | (target << 8));
    /* set DFIMISC.dfi_init_complete_en */
    mmio_write_32(DDRC_DFIMISC(0), 0x1 | (target << 8));

    /* set SWCTL.sw_done to enable quasi-dynamic register programming */
    mmio_write_32(DDRC_SWCTL(0), 0x1);
    /* wait SWSTAT.sw_done_ack to 1 */
    while (!(mmio_read_32(DDRC_SWSTAT(0)) & 0x1))
        ;

    mmio_write_32(DDRC_PWRCTL(0), 0x88);
    /* wait STAT to normal state */
    while (0x1 != (mmio_read_32(DDRC_STAT(0)) & 0x7))
        ;

    mmio_write_32(DDRC_PCTRL_0(0), 0x1);
    /* dis_auto-refresh is set to 0 */
    mmio_write_32(DDRC_RFSHCTL3(0), 0x0);
    /* should check PhyInLP3 pub reg */
    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    if (!(dwc_ddrphy_apb_rd(0x90028) & 0x1)){
        //PRINTF("PHYInLP3 = 0\n");
    }
    dwc_ddrphy_apb_wr(0xd0000, 0x1);
}
        

void dram_enter_retention(void)
{
    /* enable the DRAM AHB ROOT */

    /* Wait DBGCAM to be empty */
    while (mmio_read_32(DDRC_DBGCAM(0)) != 0x36000000)
        ;
    /* Block AXI ports from taking anymore transactions */
    mmio_write_32(DDRC_PCTRL_0(0), 0x0);
    /* Wait until all AXI ports are idle */
    while (mmio_read_32(DDRC_PSTAT(0)) & 0x10001)
        ;
    /* Enter self refresh */
    mmio_write_32(DDRC_PWRCTL(0), 0xaa);

    /* LPDDR4 & DDR4/DDR3L need to check different status */
    if (dram_info.dram_type == DDRC_LPDDR4)
        while(0x223 != (mmio_read_32(DDRC_STAT(0)) & 0x33f));
    else
        while (0x23 != (mmio_read_32(DDRC_STAT(0)) & 0x3f)) ;


    mmio_write_32(DDRC_DFIMISC(0), 0x0);
    mmio_write_32(DDRC_SWCTL(0), 0x0);
    mmio_write_32(DDRC_DFIMISC(0), 0x1f00);
    mmio_write_32(DDRC_DFIMISC(0), 0x1f20);

    while (mmio_read_32(DDRC_DFISTAT(0)) & 0x1);

    mmio_write_32(DDRC_DFIMISC(0), 0x1f00);
    /* wait DFISTAT.dfi_init_complete to 1 */
    while (!(mmio_read_32(DDRC_DFISTAT(0)) & 0x1));

    mmio_write_32(DDRC_SWCTL(0), 0x1);

    /* should check PhyInLP3 pub reg */
    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    if (!(dwc_ddrphy_apb_rd(0x90028) & 0x1)){
        //PRINTF("PhyInLP3 = 1\n");
    }
    dwc_ddrphy_apb_wr(0xd0000, 0x1);

    /* pwrdnreqn_async adbm/adbs of ddr */
    mmio_clrbits_32(IMX_GPC_BASE + GPC_PU_PWRHSK, DDRMIX_ADB400_SYNC);
    while (mmio_read_32(IMX_GPC_BASE + GPC_PU_PWRHSK) & DDRMIX_ADB400_ACK);
    mmio_setbits_32(IMX_GPC_BASE + GPC_PU_PWRHSK, DDRMIX_ADB400_SYNC);

    /* remove PowerOk */
    mmio_write_32(SRC_DDR1_RCR, 0x8F000008);
}
        

#define GPC_IPS_BASE_ADDR IMX_GPC_BASE

void power_management_init(void)
{
    sema4_init();

    dram_info_init(SAVED_DRAM_TIMING_BASE);
    /* bit 0:M7, bit 1:NOC, bit 20:DDR why 0x3f0003 ?*/
    mmio_write_32(PGC_CPU_MAPPING, 0x200003 | mmio_read_32(PGC_CPU_MAPPING));  
    mmio_setbits_32(IMX_GPC_BASE + DDRMIX_PGC, 1);
}

void power_management_after_ddr_usage(void)
{
    uint8_t is_a53_active;

    /* Get info about A53 activity */
    is_a53_active =  mmio_read_32((uintptr_t) &ServiceFlagAddr) & 0xF;
    if (is_a53_active==0)
    {

        dram_enter_retention();

        mmio_clrbits_32(0x30388800,  (0x7 << 24) | (0x7 << 16));  
        mmio_clrbits_32(0x30388d00,  (0x7 << 24) | (0x7 << 16));  
        mmio_clrbits_32(0x30388d80,  (0x7 << 24) | (0x7 << 16));  

        LPM_MCORE_ChangeM7Clock(LPM_M7_LOW_FREQ);

        /* disable ddr clock */
        mmio_clrbits_32(0x3038A000, (0x1 << 28));
        mmio_clrbits_32(0x3038A080, (0x1 << 28));

        /* disable the SYSTEM PLL1, bypass first, then disable */
        /* can't disable SYSTEM PLL1 why ?, UART AHB, M7 all switched to 24M */
        mmio_setbits_32(0x30360094, (0x1 << 4));
        mmio_clrbits_32(0x30360094, (0x1 << 9));

        mmio_write_32(CCM_CCGR(97), 0x0); /* CCM_CCGR pll */
        mmio_write_32(CCM_CCGR(5),  0x0); /* CCM_CCGR dram1 */
        mmio_write_32(CCM_SRC_CTRL(15), 0x20);

       
    }
    sema4_unlock(0);

}

void power_management_before_ddr_usage(void)
{	
    uint8_t is_a53_active;

    sema4_lock(0);

    /* Get info about A53 activity */
    is_a53_active = mmio_read_32((uintptr_t) &ServiceFlagAddr) & 0xF;
    //FSY_TRACE("%d->%d ",nb_active_proc, nb_active_proc+1);
    if (is_a53_active==0)
    {

        uint8_t lpa_low_rate;
        lpa_low_rate = !(mmio_read_32((uintptr_t) &ServiceFlagAddr) & 0xF0000);

        

        mmio_write_32(CCM_CCGR(97), 0x10); /* CCM_CCGR pll */
        mmio_write_32(CCM_CCGR(5), 0x10);  /* CCM_CCGR dram1 */
        mmio_write_32(CCM_SRC_CTRL(15), 0x10);  /* ddr pll ? need this ? */

        /* enable the SYSTEM PLL1, enable first, then unbypass */
        mmio_setbits_32(0x30360094, (0x1 << 9));
        while(!(mmio_read_32(0x30360094) & ( (uint32_t) 1 << 31)));
        mmio_clrbits_32(0x30360094, (0x1 << 4));

        /* enable ddr clock */
        mmio_setbits_32(0x3038A000, (0x1 << 28));
        mmio_setbits_32(0x3038A080, (0x1 << 28));



        LPM_MCORE_ChangeM7Clock(LPM_M7_HIGH_FREQ);

        dram_exit_retention_with_target(1);


        mmio_clrbits_32(0x30388800,  (0x7 << 24) | (0x7 << 16));  
        mmio_clrbits_32(0x30388d00,  (0x7 << 24) | (0x7 << 16));  
        mmio_clrbits_32(0x30388d80,  (0x7 << 24) | (0x7 << 16));  

        if(!lpa_low_rate){
            mmio_setbits_32(0x30388800,  (0x2 << 24) | (1 << 16));  /* Audio AXI to SYSPLL1 */
            mmio_setbits_32(0x30388d00,  (0x1 << 24) | (1 << 16));  /* NOC to SYSPLL1 */
            mmio_setbits_32(0x30388d80,  (0x1 << 24) | (1 << 16));  /* NOC_IO to SYSPLL1 */
        }
    }
}

