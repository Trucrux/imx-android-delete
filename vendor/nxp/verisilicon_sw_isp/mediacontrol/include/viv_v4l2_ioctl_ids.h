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

#ifndef _VIV_V4L2_IOCTL_IDS_H_
#define _VIV_V4L2_IOCTL_IDS_H_

#include "cam_device_module_ids.h"
#include "ioctl_cmds.h"

enum {
    // mc
    VIV_V4L_MC_SET_LINKS     = ISPCORE_MODULE_END + 0x0000,
    VIV_V4L_MC_USE_COMPONENT = ISPCORE_MODULE_END + 0x0001,
    VIV_V4L_MC_CAPTURE       = ISPCORE_MODULE_END + 0x0002,

    // dwe
    VIV_V4L_DWE_GET_PARAM    = ISPCORE_MODULE_END + 0x1000,
    VIV_V4L_DWE_SET_PARAM    = ISPCORE_MODULE_END + 0x1001,
    VIV_V4L_DWE_GET_HFLIP    = ISPCORE_MODULE_END + 0x1002,
    VIV_V4L_DWE_SET_HFLIP    = ISPCORE_MODULE_END + 0x1003,
    VIV_V4L_DWE_GET_VFLIP    = ISPCORE_MODULE_END + 0x1004,
    VIV_V4L_DWE_SET_VFLIP    = ISPCORE_MODULE_END + 0x1005,
    VIV_V4L_DWE_GET_BYPASS   = ISPCORE_MODULE_END + 0x1006,
    VIV_V4L_DWE_SET_BYPASS   = ISPCORE_MODULE_END + 0x1007,
    VIV_V4L_DWE_GET_MODE     = ISPCORE_MODULE_END + 0x1008,
    VIV_V4L_DWE_SET_MODE     = ISPCORE_MODULE_END + 0x1009,
    VIV_V4L_DWE_GET_CROP     = ISPCORE_MODULE_END + 0x100a,
    VIV_V4L_DWE_SET_CROP     = ISPCORE_MODULE_END + 0x100b,
    VIV_V4L_DWE_GET_SCALE    = ISPCORE_MODULE_END + 0x100c,
    VIV_V4L_DWE_SET_SCALE    = ISPCORE_MODULE_END + 0x100d,
    VIV_V4L_DWE_GET_MATRIX   = ISPCORE_MODULE_END + 0x100e,
    VIV_V4L_DWE_SET_MATRIX   = ISPCORE_MODULE_END + 0x100f,
    VIV_V4L_DWE_GET_TYPE     = ISPCORE_MODULE_END + 0x1010,
    VIV_V4L_DWE_SET_TYPE     = ISPCORE_MODULE_END + 0x1011,
};


#endif  // _VIV_V4L2_IOCTL_IDS_H_
