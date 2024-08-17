/******************************************************************************\
|* Copyright 2010, Dream Chip Technologies GmbH. used with permission by      *|
|* VeriSilicon.                                                               *|
|* Copyright (c) <2021> by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")     *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

/* VeriSilicon 2021 */

/**
 * @file isi_iss.h
 *
 * @brief Interface description for image sensor specific implementation (iss).
 *
 *****************************************************************************/
/**
 * @page module_name_page Module Name
 * Describe here what this module does.
 *
 * For a detailed list of functions and implementation detail refer to:
 * - @ref module_name
 *
 * @defgroup isi_iss CamerIc Driver API
 * @{
 *
 */
#ifndef __ISI_ISS_H__
#define __ISI_ISS_H__

#ifdef ISI_LITE
#include "vsi_type.h"
#endif

#include "isi.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ISI_EXPO_PARAS_FIX_FRACBITS 10

#ifndef ISI_LITE
/*****************************************************************************/
/**
 *          IsiRegisterFlags_t
 *
 * @brief   Register permission enumeration type
 */
/*****************************************************************************/
typedef enum IsiRegisterFlags_e
{
    // basic features
    eTableEnd           = 0x00,                                                 /**< special flag for end of register table */
    eReadable           = 0x01,
    eWritable           = 0x02,
    eVolatile           = 0x04,                                                 /**< register can change even if not written by I2C */
    eDelay              = 0x08,                                                 /**< wait n ms */
    eReserved           = 0x10,
    eNoDefault          = 0x20,                                                 /**< no default value specified */
    eTwoBytes           = 0x40,                                                 /**< SMIA sensors use 8-, 16- and 32-bit registers */
    eFourBytes          = 0x80,                                                 /**< SMIA sensors use 8-, 16- and 32-bit registers */

    // combined features
    eReadOnly           = eReadable,
    eWriteOnly          = eWritable,
    eReadWrite          = eReadable | eWritable,
    eReadWriteDel       = eReadable | eWritable | eDelay,
    eReadWriteVolatile  = eReadable | eWritable | eVolatile,
    eReadWriteNoDef     = eReadable | eWritable | eNoDefault,
    eReadWriteVolNoDef  = eReadable | eWritable | eVolatile | eNoDefault,
    eReadVolNoDef       = eReadable | eVolatile | eNoDefault,
    eReadOnlyVolNoDef   = eReadOnly | eVolatile | eNoDefault,

    // additional SMIA features
    eReadOnly_16            = eReadOnly          | eTwoBytes,
    eReadWrite_16           = eReadWrite         | eTwoBytes,
    eReadWriteDel_16        = eReadWriteDel      | eTwoBytes,
    eReadWriteVolatile_16   = eReadWriteVolatile | eTwoBytes,
    eReadWriteNoDef_16      = eReadWriteNoDef    | eTwoBytes,
    eReadWriteVolNoDef_16   = eReadWriteVolNoDef | eTwoBytes,
    eReadOnlyVolNoDef_16    = eReadOnly_16 | eVolatile | eNoDefault,
    eReadOnly_32            = eReadOnly          | eFourBytes,
    eReadWrite_32           = eReadWrite         | eFourBytes,
    eReadWriteVolatile_32   = eReadWriteVolatile | eFourBytes,
    eReadWriteNoDef_32      = eReadWriteNoDef    | eFourBytes,
    eReadWriteVolNoDef_32   = eReadWriteVolNoDef | eFourBytes
} IsiRegisterFlags_t;



/*****************************************************************************/
/**
 *          IsiRegDescription_t
 *
 * @brief   Sensor register description struct
 */
/*****************************************************************************/
typedef struct IsiRegisterFlags_s
{
    uint32_t    Addr;
    uint32_t    DefaultValue;
    const char* pName;
    uint32_t    Flags;
} IsiRegDescription_t;

#endif
typedef RESULT (*IsiHalQuerySensor_t)                ( HalHandle_t HalHandle, IsiSensorModeInfoArray_t *pSensorMode );

typedef RESULT (*IsiSensorSetPowerIss_t)             ( IsiSensorHandle_t handle, bool_t on );
typedef RESULT (*IsiCreateSensorIss_t)               ( IsiSensorInstanceConfig_t *pConfig );
typedef RESULT (*IsiReleaseSensorIss_t)              ( IsiSensorHandle_t handle );
typedef RESULT (*IsiRegisterReadIss_t)               ( IsiSensorHandle_t handle, const uint32_t address, uint32_t *pValue );
typedef RESULT (*IsiRegisterWriteIss_t)              ( IsiSensorHandle_t handle, const uint32_t address, const uint32_t value );
typedef RESULT (*IsiGetSensorModeIss_t)              ( IsiSensorHandle_t handle, IsiSensorMode_t *pMode );
typedef RESULT (*IsiSetSensorModeIss_t)              ( IsiSensorHandle_t handle, IsiSensorMode_t *pMode );
typedef RESULT (*IsiQuerySensorIss_t)                ( IsiSensorHandle_t handle, IsiSensorModeInfoArray_t *pSensorMode );
typedef RESULT (*IsiGetCapsIss_t)                    ( IsiSensorHandle_t handle, IsiSensorCaps_t *pIsiSensorCaps );
typedef RESULT (*IsiSetupSensorIss_t)                ( IsiSensorHandle_t handle, const IsiSensorCaps_t *pIsiSensorCaps );
typedef RESULT (*IsiCheckSensorConnectionIss_t)      ( IsiSensorHandle_t handle );
typedef RESULT (*IsiGetSensorRevisionIss_t)          ( IsiSensorHandle_t handle, uint32_t *pValue );
typedef RESULT (*IsiSensorSetStreamingIss_t)         ( IsiSensorHandle_t handle, bool_t on );

/* AEC */
typedef RESULT (*IsiGetAeInfoIss_t)                  ( IsiSensorHandle_t handle, IsiSensorAeInfo_t *pAeInfo );
typedef RESULT (*IsiSetHdrRatioIss_t)                ( IsiSensorHandle_t handle, uint8_t hdrRatioNum, uint32_t HdrRatio[] );
typedef RESULT (*IsiGetIntegrationTimeIss_t)         ( IsiSensorHandle_t handle, IsiSensorIntTime_t *pIntegrationTime );
typedef RESULT (*IsiSetIntegrationTimeIss_t)         ( IsiSensorHandle_t handle, IsiSensorIntTime_t *pIntegrationTime );
typedef RESULT (*IsiGetGainIss_t)                    ( IsiSensorHandle_t handle, IsiSensorGain_t *pGain );
typedef RESULT (*IsiSetGainIss_t)                    ( IsiSensorHandle_t handle, IsiSensorGain_t *pGain );

/* FPS */
typedef RESULT (*IsiGetSensorFpsIss_t)               ( IsiSensorHandle_t handle, uint32_t *pFps );
typedef RESULT (*IsiSetSensorFpsIss_t)               ( IsiSensorHandle_t handle, uint32_t fps );
typedef RESULT (*IsiSetSensorAfpsLimitsIss_t)         ( IsiSensorHandle_t handle, uint32_t minAfps );

typedef RESULT (*IsiGetSensorIspStatusIss_t)         ( IsiSensorHandle_t handle, IsiSensorIspStatus_t *pSensorIspStatus );
typedef RESULT (*IsiGetAeStartExposureIss_t)         ( IsiSensorHandle_t handle, uint64_t *pExposure );
typedef RESULT (*IsiSetAeStartExposureIss_t)         ( IsiSensorHandle_t handle, uint64_t exposure );

#ifndef ISI_LITE

typedef RESULT (*IsiSensorSetBlcIss_t)               ( IsiSensorHandle_t handle, IsiSensorBlc_t *pBlc );
typedef RESULT (*IsiSensorSetWBIss_t)                ( IsiSensorHandle_t handle, IsiSensorWB_t *pWb );
typedef RESULT (*IsiSensorGetExpandCurveIss_t)       ( IsiSensorHandle_t handle, IsiSensorExpandCurve_t *pExpandCurve );
typedef RESULT (*IsiSensorGetCompressCurveIss_t)     ( IsiSensorHandle_t handle, IsiSensorCompressCurve_t *pCompressCurve );
typedef RESULT (*IsiActivateTestPatternIss_t)        ( IsiSensorHandle_t handle, IsiSensorTpgMode_e  tpgMode );

/* AF */
typedef RESULT (*IsiFocusSetupIss_t)                   ( IsiSensorHandle_t handle );
typedef RESULT (*IsiFocusReleaseIss_t)                 ( IsiSensorHandle_t handle );
typedef RESULT (*IsiFocusSetIss_t)                     ( IsiSensorHandle_t handle, IsiFocusPos_t *pPos );
typedef RESULT (*IsiFocusGetIss_t)                     ( IsiSensorHandle_t handle, IsiFocusPos_t *pPos );
typedef RESULT (*IsiFocusGetCalibrateIss_t)            ( IsiSensorHandle_t handle, IsiFoucsCalibAttr_t *pFocusCalib );

#endif

/*****************************************************************************/
/**
 *          IsiSensor_t
 *
 * @brief
 *
 */
/*****************************************************************************/
struct IsiSensor_s
{
    const char                         *pszName;

    IsiSensorSetPowerIss_t              pIsiSensorSetPowerIss;
    IsiCreateSensorIss_t                pIsiCreateSensorIss;
    IsiReleaseSensorIss_t               pIsiReleaseSensorIss;
    IsiRegisterReadIss_t                pIsiRegisterReadIss;
    IsiRegisterWriteIss_t               pIsiRegisterWriteIss;
    IsiGetSensorModeIss_t               pIsiGetSensorModeIss;
    IsiSetSensorModeIss_t               pIsiSetSensorModeIss;
    IsiQuerySensorIss_t                 pIsiQuerySensorIss;
    IsiGetCapsIss_t                     pIsiGetCapsIss;
    IsiSetupSensorIss_t                 pIsiSetupSensorIss;
    IsiGetSensorRevisionIss_t           pIsiGetSensorRevisionIss;
    IsiCheckSensorConnectionIss_t       pIsiCheckSensorConnectionIss;
    IsiSensorSetStreamingIss_t          pIsiSensorSetStreamingIss;
    
    /* AEC functions */
    IsiGetAeInfoIss_t                   pIsiGetAeInfoIss;
    IsiSetHdrRatioIss_t                 pIsiSetHdrRatioIss;
    IsiGetIntegrationTimeIss_t          pIsiGetIntegrationTimeIss;
    IsiSetIntegrationTimeIss_t          pIsiSetIntegrationTimeIss;
    IsiGetGainIss_t                     pIsiGetGainIss;
    IsiSetGainIss_t                     pIsiSetGainIss;
    IsiGetSensorFpsIss_t                pIsiGetSensorFpsIss;
    IsiSetSensorFpsIss_t                pIsiSetSensorFpsIss;
    IsiSetSensorAfpsLimitsIss_t         pIsiSetSensorAfpsLimitsIss;
    IsiGetSensorIspStatusIss_t          pIsiGetSensorIspStatusIss;
    IsiGetAeStartExposureIss_t          pIsiGetAeStartExposureIss;
    IsiSetAeStartExposureIss_t          pIsiSetAeStartExposureIss;

#ifndef ISI_LITE
    IsiSensorSetBlcIss_t                pIsiSensorSetBlcIss;
    IsiSensorSetWBIss_t                 pIsiSensorSetWBIss;
    IsiSensorGetExpandCurveIss_t        pIsiSensorGetExpandCurveIss;
    IsiSensorGetCompressCurveIss_t      pIsiSensorGetCompressCurveIss;
    IsiActivateTestPatternIss_t         pIsiActivateTestPatternIss;

    /* AF functions */
    IsiFocusSetupIss_t                  pIsiFocusSetupIss;
    IsiFocusReleaseIss_t                pIsiFocusReleaseIss;
    IsiFocusSetIss_t                    pIsiFocusSetIss;
    IsiFocusGetIss_t                    pIsiFocusGetIss;
    IsiFocusGetCalibrateIss_t           pIsiGetFocusCalibrateIss;

#endif
};



/*****************************************************************************/
/**
 *          IsiGetSensorIss_t
 *
 * @brief   Only exported function of sensor specific code: fills in
 *          sensor decription struct
 *
 */
/*****************************************************************************/
typedef RESULT (*IsiGetSensorIss_t) ( IsiSensor_t *pIsiSensor );

#ifndef ISI_LITE
/*****************************************************************************/
/**
 *          IsiCamDrvConfig_t
 *
 * @brief   Camera sensor driver specific data
 *
 */
/*****************************************************************************/
typedef struct IsiCamDrvConfig_s
{
    uint32_t                     CameraDriverID;
    IsiHalQuerySensor_t          pIsiHalQuerySensor;
    IsiGetSensorIss_t            pfIsiGetSensorIss;
    IsiSensor_t                  IsiSensor;
} IsiCamDrvConfig_t;

#endif


#ifdef __cplusplus
}
#endif

#endif /* __ISI_ISS_H__ */

