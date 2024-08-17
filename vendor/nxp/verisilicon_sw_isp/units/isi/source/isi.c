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
#include <sys/ioctl.h>

#include "isi.h"
#include "isi_iss.h"
#include "isi_priv.h"

/******************************************************************************
 * local macro definitions
 *****************************************************************************/
CREATE_TRACER( ISI_INFO , "ISI: ", INFO,    0);
CREATE_TRACER( ISI_WARN , "ISI: ", WARNING, 1);
CREATE_TRACER( ISI_ERROR, "ISI: ", ERROR,   1);

/*****************************************************************************/
/**
 *          IsiSensorSetPowerIss
 *
 * @brief   Performs the power-up/power-down sequence of the camera, if possible.
 *
 * @param   handle      Sensor instance handle
 * @param   on          new power state (BOOL_TRUE=on, BOOL_FALSE=off)
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NOTSUPP
 *
 *****************************************************************************/
RESULT IsiSensorSetPowerIss
(
    IsiSensorHandle_t   handle,
    bool_t              on
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ((pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL)) {
        return ( RET_WRONG_HANDLE );
    }

    if (pSensorCtx->pSensor->pIsiSensorSetPowerIss == NULL) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSensorSetPowerIss(pSensorCtx, on);

    TRACE(ISI_INFO, "%s: (exit)\n", __func__);

    return (result);
}

/*****************************************************************************/
/**
 *          IsiCreateSensorIss
 *
 * @brief   Creates a new camera instance.
 *
 * @param   pConfig     configuration of the new sensor instance
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_NULL_POINTER
 * @retval  RET_OUTOFMEM
 *
 *****************************************************************************/
RESULT IsiCreateSensorIss
(
    IsiSensorInstanceConfig_t   *pConfig
)
{
    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pConfig == NULL) || (pConfig->HalHandle == NULL) ||
        (pConfig->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    (void)HalAddRef(pConfig->HalHandle);

    if ( pConfig->pSensor->pIsiCreateSensorIss == NULL ) {
        (void)HalDelRef( pConfig->HalHandle );
        return ( RET_NOTSUPP );
    }

    result = pConfig->pSensor->pIsiCreateSensorIss( pConfig );
    if ( result != RET_SUCCESS ) {
        (void)HalDelRef( pConfig->HalHandle );
    }

    TRACE( ISI_INFO, "%s: (exit)\n", __func__ );

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiReleaseSensorIss
 *
 * @brief   Destroys a camera instance.
 *
 * @param   handle      Sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 *
 *****************************************************************************/
RESULT IsiReleaseSensorIss
(
    IsiSensorHandle_t   handle
)
{
    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__ );

    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;
    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    (void)HalDelRef( pSensorCtx->HalHandle );

    if ( pSensorCtx->pSensor->pIsiReleaseSensorIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiReleaseSensorIss( pSensorCtx );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetSensorModeIss
 *
 * @brief   get cuurent sensor mode info.
 *
 * @param   handle      Sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 *
 *****************************************************************************/
RESULT IsiGetSensorModeIss
(
    IsiSensorHandle_t   handle,
    IsiSensorMode_t     *pMode
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetSensorModeIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetSensorModeIss( pSensorCtx, pMode);

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetSensorModeIss
 *
 * @brief   set sensor mode by index.
 *
 * @param   handle      Sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 *
 *****************************************************************************/
RESULT IsiSetSensorModeIss
(
    IsiSensorHandle_t handle,
    IsiSensorMode_t *pMode
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetSensorModeIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetSensorModeIss( pSensorCtx, pMode);

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiQuerySensorIss
 *
 * @brief   query sensor info arry.
 *
 * @param   handle                  sensor instance handle
 * @param   pIsiSensorModeArr       sensor query mode arry
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiQuerySensorIss
(
    IsiSensorHandle_t handle,
    IsiSensorModeInfoArray_t *pIsiSensorModeArr
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiQuerySensorIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiQuerySensorIss( pSensorCtx, pIsiSensorModeArr );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetCapsIss
 *
 * @brief   fills in the correct pointers for the sensor description struct
 *
 * @param   handle      Sensor instance handle
 * @param   pCaps
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiGetCapsIss
(
    IsiSensorHandle_t   handle,
    IsiSensorCaps_t     *pSensorCaps
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    if (pSensorCtx->pSensor->pIsiGetCapsIss == NULL) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetCapsIss( pSensorCtx, pSensorCaps );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );

}

/*****************************************************************************/
/**
 *          IsiSetupSensorIss
 *
 * @brief   Setup of the image sensor considering the given configuration.
 *
 * @param   handle      Sensor instance handle
 * @param   pConfig     pointer to sensor configuration structure
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSetupSensorIss
(
    IsiSensorHandle_t   handle,
    IsiSensorCaps_t    *pSensorCaps
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetupSensorIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetupSensorIss( pSensorCtx, pSensorCaps );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiCheckSensorConnectionIss
 *
 * @brief   Performs the power-up sequence of the camera, if possible.
 *
 * @param   handle      Sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiCheckSensorConnectionIss
(
    IsiSensorHandle_t   handle
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( pSensorCtx == NULL ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiCheckSensorConnectionIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiCheckSensorConnectionIss( pSensorCtx );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetSensorRevisionIss
 *
 * @brief   reads the sensor revision register and returns this value
 *
 * @param   Handle      pointer to sensor description struct
 * @param   p_value     pointer to storage value
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiGetSensorRevisionIss
(
    IsiSensorHandle_t   handle,
    uint32_t            *pValue
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetSensorRevisionIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result =  pSensorCtx->pSensor->pIsiGetSensorRevisionIss( handle, pValue );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}
/*****************************************************************************/
/**
 *          IsiSensorSetStreamingIss
 *
 * @brief   Enables/disables streaming of sensor data, if possible.
 *
 * @param   handle      Sensor instance handle
 * @param   on          new streaming state (BOOL_TRUE=on, BOOL_FALSE=off)
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NOTSUPP
 * @retval  RET_WRONG_STATE
 *
 *****************************************************************************/
RESULT IsiSensorSetStreamingIss
(
    IsiSensorHandle_t   handle,
    bool_t              on
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSensorSetStreamingIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSensorSetStreamingIss( pSensorCtx, on );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetAeInfoIss
 *
 * @brief   
 *
 * @param   handle                  sensor instance handle
 * @param   *pAeInfo                Pointer to exposure information 
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/

RESULT IsiGetAeInfoIss
(
    IsiSensorHandle_t   handle,
    IsiSensorAeInfo_t  *pAeInfo
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetAeInfoIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetAeInfoIss( pSensorCtx, pAeInfo );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetHdrRatioIss
 *
 * @brief   This function reads the sensor revision register and returns it.
 *
 * @param   handle      sensor instance handle
 * @param   hdrRatioNum size of hdr ratio array newHdrRatio[]
 * @param   newHdrRatio hdr ratio array with q10
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 * @retval  RET_NOTSUPP
 *
 *****************************************************************************/
RESULT IsiSetHdrRatioIss
(
    IsiSensorHandle_t   handle,
    uint8_t             hdrRatioNum,
    uint32_t            HdrRatio[]
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetHdrRatioIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetHdrRatioIss( pSensorCtx, hdrRatioNum, HdrRatio );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetIntegrationTimeIss
 *
 * @brief   Reads integration time values from the image sensor module.
 *
 * @param   handle                  sensor instance handle
 * @param   pIntegrationTime        set integration time
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiGetIntegrationTimeIss
(
    IsiSensorHandle_t   handle,
    IsiSensorIntTime_t *pIntegrationTime
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetIntegrationTimeIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetIntegrationTimeIss( pSensorCtx, pIntegrationTime );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetIntegrationTimeIss
 *
 * @brief   Writes integration time values to the image sensor module.
 *
 * @param   handle                  sensor instance handle
 * @param   newIntegrationTime      integration time to be set
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSetIntegrationTimeIss
(
    IsiSensorHandle_t   handle,
    IsiSensorIntTime_t *pIntegrationTime
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetIntegrationTimeIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetIntegrationTimeIss( pSensorCtx, pIntegrationTime );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetGainIss
 *
 * @brief   Reads gain values from the image sensor module.
 *
 * @param   handle                  sensor instance handle
 * @param   pGain                   set gain
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiGetGainIss
(
    IsiSensorHandle_t   handle,
    IsiSensorGain_t    *pGain
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetGainIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetGainIss( pSensorCtx, pGain );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetGainIss
 *
 * @brief   Writes gain values to the image sensor module.
 *
 * @param   handle                  sensor instance handle
 * @param   newGain                 gain to be set
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSetGainIss
(
    IsiSensorHandle_t   handle,
    IsiSensorGain_t    *pGain
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetGainIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetGainIss( pSensorCtx, pGain );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetSensorFpsIss
 *
 * @brief   Get Sensor Fps Config.
 *
 * @param   handle                  sensor instance handle
 * @param   pFps                    current fps
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiGetSensorFpsIss
(
    IsiSensorHandle_t   handle,
    uint32_t            *pFps
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetSensorFpsIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetSensorFpsIss( pSensorCtx, pFps );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetSensorFpsIss
 *
 * @brief   set Sensor Fps Config.
 *
 * @param   handle                  sensor instance handle
 * @param   fps                     Setfps
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSetSensorFpsIss
(
    IsiSensorHandle_t   handle,
    uint32_t            Fps
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetSensorFpsIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetSensorFpsIss( pSensorCtx, Fps );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetSensorAfpsLimtsIss
 *
 * @brief   set auto fps limit..
 *
 * @param   handle                  sensor instance handle
 * @param   minAfps                 minimum framerate for auto fps control
 *                                  in fixed point ISI_EXPO_PARAS_FIX_FRACBITS
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSetSensorAfpsLimitsIss
(
    IsiSensorHandle_t   handle,
    uint32_t            minAfps
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetSensorAfpsLimitsIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetSensorAfpsLimitsIss( pSensorCtx, minAfps );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetSensorIspStatusIss
 *
 * @brief   
 *
 * @param   handle                  sensor instance handle
 * @param   *pSensorIspStatus       Pointer to the sensor isp status
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiGetSensorIspStatusIss
(
    IsiSensorHandle_t     handle,
    IsiSensorIspStatus_t *pSensorIspStatus
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetSensorIspStatusIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetSensorIspStatusIss( pSensorCtx, pSensorIspStatus );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiGetAeStartExposureIss
 *
 * @brief   get sensor AE start exposure
 *
 * @param   handle          sensor instance handle
 * @param   pExposure       point of exposure
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_FAILURE
 *
 *****************************************************************************/
RESULT IsiGetAeStartExposureIss
(
    IsiSensorHandle_t  handle,
    uint64_t *pExposure
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetAeStartExposureIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetAeStartExposureIss( pSensorCtx, pExposure );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSetAeStartExposureIss
 *
 * @brief   set sensor AE start exposure
 *
 * @param   handle          sensor instance handle
 * @param   exposure        exposure
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_FAILURE
 *
 *****************************************************************************/
RESULT IsiSetAeStartExposureIss
(
    IsiSensorHandle_t  handle,
    uint64_t Exposure
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSetAeStartExposureIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSetAeStartExposureIss( pSensorCtx, Exposure );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

#ifndef ISI_LITE
/*****************************************************************************/
/**
 *          IsiSensorSetBlc
 *
 * @brief   set sensor linear mode black level 
 *          
 *
 * @param   handle          sensor instance handle
 * @param   pBlc            blc params point
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_FAILURE
 *
 *****************************************************************************/
RESULT IsiSensorSetBlc
(
    IsiSensorHandle_t handle,
    IsiSensorBlc_t   *pBlc
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSensorSetBlcIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSensorSetBlcIss( pSensorCtx, pBlc );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSensorSetWB
 *
 * @brief   set sensor linear mode white balance 
 *          or hdr mode normal exp frame white balance
 *
 * @param   handle          sensor instance handle
 * @param   pWb             wb params point
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_FAILURE
 *
 *****************************************************************************/
RESULT IsiSensorSetWB
(
    IsiSensorHandle_t handle,
    IsiSensorWB_t    *pWb
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSensorSetWBIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSensorSetWBIss( pSensorCtx, pWb );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSensorGetExpandCure
 *
 * @brief   get sensor expand curve 
 *
 * @param   handle          sensor instance handle
 * @param   pexpand_curve   expand cure point
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_FAILURE
 *
 *****************************************************************************/
RESULT IsiSensorGetExpandCurve
(
    IsiSensorHandle_t       handle,
    IsiSensorExpandCurve_t *pExpandCurve
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSensorGetExpandCurveIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSensorGetExpandCurveIss( pSensorCtx, pExpandCurve );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiSensorGetCompressCure
 *
 * @brief   get sensor compress curve
 *
 * @param   handle          sensor instance handle
 * @param   pexpand_curve   compress cure point
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_FAILURE
 *
 *****************************************************************************/
RESULT IsiSensorGetCompressCure
(
    IsiSensorHandle_t       handle,
    IsiSensorCompressCurve_t *pCompressCurve
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiSensorGetCompressCurveIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiSensorGetCompressCurveIss( pSensorCtx, pCompressCurve );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiActivateTestPattern
 *
 * @brief   Activates or deactivates sensor's test-pattern
 *
 * @param   handle          sensor instance handle
 * @param   tpgMode         sensor's test-pattern mode
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiActivateTestPattern
(
    IsiSensorHandle_t   handle,
    IsiSensorTpgMode_e  tpgMode
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiActivateTestPatternIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiActivateTestPatternIss( pSensorCtx, tpgMode );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiFocusSetupIss
 *
 * @brief   Setup of the MotoDrive.
 *
 * @param   handle          sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiFocusSetupIss
(
    IsiSensorHandle_t handle
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiFocusSetupIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiFocusSetupIss( pSensorCtx );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiFocusReleaseIss
 *
 * @brief   Release of the MotoDrive.
 *
 * @param   handle          sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
*****************************************************************************/

RESULT IsiFocusReleaseIss
(
    IsiSensorHandle_t handle
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiFocusReleaseIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiFocusReleaseIss( pSensorCtx );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiFocusSetIss
 *
 * @brief   Drives the lens system to a certain focus point.
 *
 * @param   handle          sensor instance handle
 *
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/

RESULT IsiFocusSetIss
(
    IsiSensorHandle_t handle,
    IsiFocusPos_t *pPos
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiFocusSetIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiFocusSetIss( pSensorCtx, pPos );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiFocusGetIss
 *
 * @brief   Retrieves the currently applied focus attribution.
 *
 * @param   handle          sensor instance handle
 *          pFocusAttr      pointer to a variable to receive focus attribution
 *
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiFocusGetIss
(
    IsiSensorHandle_t handle,
    IsiFocusPos_t *pPos
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiFocusSetIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiFocusGetIss( pSensorCtx, pPos );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiFocusCalibrateIss
 *
 * @brief   Triggers a forced calibration of the focus hardware.
 *
 * @param   handle          sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiFocusCalibrateIss
(
    IsiSensorHandle_t handle,
    IsiFoucsCalibAttr_t *pFocusCalib
)
{
    IsiSensorContext_t *pSensorCtx = (IsiSensorContext_t *)handle;

    RESULT result = RET_SUCCESS;

    TRACE( ISI_INFO, "%s: (enter)\n", __func__);

    if ( (pSensorCtx == NULL) || (pSensorCtx->pSensor == NULL) ) {
        return ( RET_WRONG_HANDLE );
    }

    if ( pSensorCtx->pSensor->pIsiGetFocusCalibrateIss == NULL ) {
        return ( RET_NOTSUPP );
    }

    result = pSensorCtx->pSensor->pIsiGetFocusCalibrateIss( pSensorCtx, pFocusCalib );

    TRACE( ISI_INFO, "%s: (exit)\n", __func__);

    return ( result );
}

/*****************************************************************************/
/**
 *          IsiDumpAllRegisters
 *
 * @brief   Activates or deactivates sensor's test-pattern (normally a defined
 *          colorbar )
 *
 * @param   handle          sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiDumpAllRegisters
(
    IsiSensorHandle_t   handle,
    const uint8_t       *filename
)
{
   return RET_SUCCESS;
}
#endif
