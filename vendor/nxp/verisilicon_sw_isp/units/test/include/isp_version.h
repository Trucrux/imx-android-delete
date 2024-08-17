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

#ifdef ISP8000_BASE
#define CHIP "8000"
#elif defined ISP8000L_BASE
#define CHIP "8000l"
#elif defined ISP8000NANO_BASE
#define CHIP "nano"
#else
#define CHIP "base"
#endif

#define BM_LIST_FILE  ISP_VERSION"/case/bare_metal/"CHIP"/list.txt"

#ifdef HAL_CMODEL
#define SETTING_FILE_NAME ISP_VERSION"/case/global_settings_simulation.txt"
#else
#define SETTING_FILE_NAME ISP_VERSION"/case/global_settings_hardware.txt"
#endif

#define CASE_PREFIX ISP_VERSION"/case/"
#define ENG_LIST_FILE ISP_VERSION"/case/engine/list.txt"

#define ENGINE_DUMP_ISP_REGS_INIT_SETTINGS ISP_VERSION"/case/engine/dump_isp_regs_all.txt"
#define ENGINE_DUMP_ISP_REGS_DUAL_SETTINGS ISP_VERSION"/case/engine/dump_isp1_register_frame0_isp1.txt"

#define ENG_IMG_PATH ISP_VERSION"/case/engine/resource"




