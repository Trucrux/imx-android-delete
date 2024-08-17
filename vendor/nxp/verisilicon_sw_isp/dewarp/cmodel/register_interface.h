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

#ifndef DEVELOPER_DEWARP_CMODEL_REGISTER_INTERFACE_H_
#define DEVELOPER_DEWARP_CMODEL_REGISTER_INTERFACE_H_

#include "dwe_regs.h"

typedef struct module_global_Register_Variable {
    uint64_t yOffsetCount1;
    uint64_t cOffsetCount1;
    uint64_t yOffsetCount2;
    uint64_t cOffsetCount2;
} mGlobalRegisterVariable;

typedef struct register_and_variable_struct {
    int moduleID;
    int dump_data_flag;
    int frame_num;
    int base_addr;
    mGlobalRegisterVariable* pMGRV;
    unsigned int register_array[REGISTER_NUM][2];
} regVarStruct, *regVarStructPoint;

extern "C" {
int dewarp_register_initional(regVarStruct* pRegVar);
int writeRegisterToCmodel(regVarStruct* pRegVar, unsigned int register_address, unsigned int register_value);
int readRegisterFromCmodel(regVarStruct* pRegVar, unsigned int register_address, unsigned int* register_value);
}

#endif  // DEVELOPER_DEWARP_CMODEL_REGISTER_INTERFACE_H_
