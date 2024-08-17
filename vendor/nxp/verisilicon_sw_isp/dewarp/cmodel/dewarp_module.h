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

#ifndef DEVELOPER_DEWARP_CMODEL_DEWARP_MODULE_H_
#define DEVELOPER_DEWARP_CMODEL_DEWARP_MODULE_H_

#include "register_interface.h"

extern "C" {
int DewarpModuleProcess(unsigned char* srcMemory1, unsigned char* srcMemory2, unsigned char* mapMemory1,
        unsigned char* mapMemory2, unsigned char* dstMemory1, unsigned char* dstMemory2,
        regVarStruct* registerVariable);

}
#endif  // DEVELOPER_DEWARP_CMODEL_DEWARP_MODULE_H_

