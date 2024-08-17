/******************************************************************************\
|* Copyright 2010, Dream Chip Technologies GmbH. used with permission by      *|
|* VeriSilicon.                                                               *|
|* Copyright (c) <2020> by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")     *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

/* VeriSilicon 2020 */

/**
 * @file isisup.c
 *
 * @brief
 *   ADD_DESCRIPTION_HERE
 *
 *****************************************************************************/
#include <ebase/types.h>
#include <ebase/trace.h>
#include <ebase/builtins.h>

#include <common/return_codes.h>

#include "isi.h"
#include "isi_iss.h"
#include "isi_priv.h"



/******************************************************************************
 * local macro definitions
 *****************************************************************************/
USE_TRACER( ISI_INFO  );
USE_TRACER( ISI_WARN  );
USE_TRACER( ISI_ERROR );

/*****************************************************************************/
/**
 *          IsiWriteRegister
 *
 * @brief   writes a given number of bytes to the image sensor device by
 *          calling the corresponding sensor-function
 *
 * @param   handle              Handle to image sensor device
 * @param   RegAddress          register address
 * @param   RegValue            value to write
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NOTSUPP
 *
 *****************************************************************************/
RESULT IsiWriteRegister
(
    IsiSensorHandle_t   handle,
    const uint32_t      RegAddress,
    const uint32_t      RegValue
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s (enter)\n", __func__);

    if ( pSensorCtx->pSensor->pIsiRegisterWriteIss == NULL )
    {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiRegisterWriteIss( handle, RegAddress, RegValue );

    TRACE( ISI_INFO, "%s (exit)\n", __func__);

    return ( result );
}



/*****************************************************************************/
/**
 *          IsiReadRegister
 *
 * @brief   reads a given number of bytes from the image sensor device
 *
 * @param   handle              Handle to image sensor device
 * @param   RegAddress          register address
 * @param   RegValue            value to write
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 * @retval  RET_NOTSUPP
 *
 *****************************************************************************/
RESULT IsiReadRegister
(
    IsiSensorHandle_t   handle,
    const uint32_t      RegAddress,
    uint32_t            *pRegValue
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s (enter)\n", __func__);

    if ( pSensorCtx->pSensor->pIsiRegisterReadIss == NULL )
    {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiRegisterReadIss( handle, RegAddress, pRegValue );

    TRACE( ISI_INFO, "%s (exit)\n", __func__);

    return ( result );
}


