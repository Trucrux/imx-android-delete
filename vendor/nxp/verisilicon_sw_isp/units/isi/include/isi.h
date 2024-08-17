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
 * @defgroup isi Independent Sensor Interface
 * @{
 *
 */
#ifndef __ISI_H__
#define __ISI_H__

#include <ebase/types.h>
#include <hal/hal_api.h>

#include <isi/isi_common.h>
#include <cameric_drv/cameric_drv_common.h>
#include <cam_device/cam_device_sensor_defs.h>
#include <vvsensor.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************/
/**
 *          IsiSensorHandle_t
 *
 * @brief   Handle to a sensor instance
 *
 */
/*****************************************************************************/
typedef void *IsiSensorHandle_t; // should be of type 'struct IsiSensorContext_s *'

/*****************************************************************************/
/**
 * @brief
 */
/*****************************************************************************/
typedef struct IsiSensor_s IsiSensor_t;
#ifndef ISI_LITE
typedef sensor_blc_t            IsiSensorBlc_t;
typedef sensor_white_balance_t  IsiSensorWB_t;
typedef sensor_expand_curve_t   IsiSensorExpandCurve_t;
#endif
typedef vvcam_mode_info_array_t IsiSensorModeInfoArray_t;
typedef vvcam_mode_info_t       IsiSensorMode_t;

typedef struct IsiSensorCompressCurve_s {
	uint32_t x_bit;
	uint32_t y_bit;
	uint8_t compress_px[64];
	uint32_t compress_x_data[65];
	uint32_t compress_y_data[65];
} IsiSensorCompressCurve_t;

typedef struct IsiExpoAttr_s
{
    uint32_t            gain;
    uint32_t            integrationTime;
    uint8_t             hdrRatioNum;
    uint32_t           *pHdrRatioArr;
}IsiExpoAttr_t;

typedef struct IsiSensorIspStatus_s
{
    bool_t useSensorAWB;
    bool_t useSensorBLC;
}IsiSensorIspStatus_t;

typedef uint32_t IsiLinearInt_t; /*(us q10) */

typedef struct IsidualInt_s{
    uint32_t dualSIntTime;/* short (us q10) */
    uint32_t dualIntTime; /* normal (us q10) */
}IsidualInt_t;

typedef struct IsiTriInt_s{
    uint32_t triSIntTime;/* very short (us q10) */
    uint32_t triIntTime; /* normal (us q10) */
    uint32_t triLIntTime;/* long (us q10) */
}IsiTriInt_t;

typedef struct IsiQuadInt_s{
    uint32_t quadVSIntTime; /* very short (us q10) */
    uint32_t quadSIntSTime; /* short (us q10) */
    uint32_t quadIntTime;   /* normal (us q10) */
    uint32_t quadLIntTime;  /* long (us q10) */
}IsiQuadInt_t;

typedef union IsiSensorIntTime_union{
    IsiLinearInt_t  linearInt;
    IsidualInt_t    dualInt;
    IsiTriInt_t     triInt;
    IsiQuadInt_t    quadInt;
}IsiSensorIntTime_u;

typedef struct IsiSensorIntTime_s{
    IsiExpoFrmType_e    expoFrmType;
    IsiSensorIntTime_u  IntegrationTime;
}IsiSensorIntTime_t;

typedef uint32_t IsiLinearGain_t;

typedef struct IsidualGain_s{
    uint32_t dualSGain; /* short (q10) */
    uint32_t dualGain;  /* normal (q10) */
}IsidualGain_t;

typedef struct IsiTriGain_s{
    uint32_t triSGain; /* short (q10) */
    uint32_t triGain;  /* normal (q10) */
    uint32_t triLGain; /* long (q10) */
}IsiTriGain_t;

typedef struct IsiQuadGain_s{
    uint32_t quadVSGain;/* very short (q10) */
    uint32_t quadSGain; /* short (q10) */
    uint32_t quadGain;  /* normal (q10) */
    uint32_t quadLGain; /* long (q10) */
}IsiQuadGain_t;

typedef union IsiSensorGain_union{
    IsiLinearGain_t linearGainParas;
    IsidualGain_t   dualGainParas;
    IsiTriGain_t    triGainParas;
    IsiQuadGain_t   quadGainParas;
}IsiSensorGain_u;

typedef struct IsiSensorGain_s{
    IsiExpoFrmType_e    expoFrmType;
    IsiSensorGain_u     gain;
}IsiSensorGain_t;

typedef struct IsiSensorAeInfo_s{
    uint32_t    oneLineExpTime;/* (us q10) */
    IsiSensorIntTime_u maxIntTime;
    IsiSensorIntTime_u minIntTime;
    IsiSensorGain_u maxAGain;
    IsiSensorGain_u minAGain;
    IsiSensorGain_u maxDGain;
    IsiSensorGain_u minDGain;
    uint32_t    gainStep;   /**< q10 */
    uint32_t    currFps;    /**< q10 */
    uint32_t    maxFps;     /**< q10 */
    uint32_t    minFps;     /**< q10 */
    uint32_t    minAfps;    /**< q10 */
    uint32_t    hdrRatio[ISI_EXPO_FRAME_TYPE_MAX-1]; /**q10 ISI_EXPO_PARAS_FIX_FRACBITS*/
                                                     /**<ISI_EXPO_FRAME_TYPE_1FRAME: no ratio */
                                                     /**<ISI_EXPO_FRAME_TYPE_2FRAMES: hdrRatio[0]= Normal/Short*/
                                                     /**<ISI_EXPO_FRAME_TYPE_3FRAMES: hdrRatio[0]= Long/Normal hdrRatio[1]=Normal/Short */
    uint8_t     intUpdateDlyFrm; /* 0: immediately 1: 1 frame delay */
    uint8_t     gainUpdateDlyFrm;/* 0: immediately 1: 1 frame delay*/
}IsiSensorAeInfo_t;

typedef struct IsiFocusPos_s {
    IsiFocus_e mode;
    int32_t Pos;
}IsiFocusPos_t;

typedef struct IsiFoucsCalibAttr_s {
    int32_t minPos;
    int32_t maxPos;
    int32_t minStep;
}IsiFoucsCalibAttr_t;

/*****************************************************************************/
/**
 * @brief   This structure defines the sensor capabilities.
 */
/*****************************************************************************/
typedef struct IsiSensorCaps_s {
    uint32_t FieldSelection;            /**< sample fields */
    uint32_t YCSequence;
    uint32_t Conv422;
    uint32_t HPol;                      /**< horizontal polarity */
    uint32_t VPol;                      /**< vertical polarity */
    uint32_t Edge;                      /**< sample edge */
    uint32_t supportModeNum;
    uint32_t currentMode;
} IsiSensorCaps_t;

/*****************************************************************************/
/**
 *          IsiSensorConfig_t
 *
 * @brief   Sensor configuration struct
 */
/*****************************************************************************/
typedef IsiSensorCaps_t IsiSensorConfig_t ;

/*****************************************************************************/
/**
 *          IsiSensorInstanceConfig_t
 *
 * @brief   Config structure to create a new sensor instance
 *
 */
/*****************************************************************************/
typedef struct IsiSensorInstanceConfig_s
{
#ifdef ISI_LITE
    uint8_t             SensorId;
    uint32_t            SensorInitAddr;
    uint16_t            SensorInitSize;
#else
    HalHandle_t         HalHandle;          /**< Handle of HAL session to use. */
    uint32_t            HalDevID;           /**< HAL device ID of this sensor. */
    uint8_t             I2cAfBusNum;        /**< The I2C bus the ad module is connected to. */
    uint16_t            SlaveAfAddr;        /**< The I2C slave addr of the af module is configured to */
    uint32_t            SensorModeIndex;
    char                szSensorNodeName[32];
#endif
    uint8_t             I2cBusNum;          /**< The I2C bus the sensor is connected to. */
    uint16_t            SlaveAddr;          /**< The I2C slave addr the sensor is configured to. */
    IsiSensor_t         *pSensor;           /**< Sensor driver interface */
    IsiSensorHandle_t   hSensor;            /**< Sensor handle returned by IsiCreateSensorIss */
} IsiSensorInstanceConfig_t;

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
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSensorSetPowerIss
(
    IsiSensorHandle_t handle,
    bool_t on
);

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
);

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
);

/*****************************************************************************/
/**
 *          IsiCreateSensorIss
 *
 * @brief   This function creates a new sensor instance.
 *
 * @param   pConfig     configuration of the new sensor
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_NULL_POINTER
 * @retval  RET_OUTOFMEM
 *
 *****************************************************************************/
RESULT IsiCreateSensorIss
(
    IsiSensorInstanceConfig_t *pConfig
);

/*****************************************************************************/
/**
 *          IsiReleaseSensorIss
 *
 * @brief   The function destroys/releases a sensor instance.
 *
 * @param   handle      sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_NOTSUPP
 *
 *****************************************************************************/
RESULT IsiReleaseSensorIss
(
    IsiSensorHandle_t handle
);

/*****************************************************************************/
/**
 *          IsiGetSensorModeIss
 *
 * @brief   get current sensor mode info.
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
    IsiSensorHandle_t handle,
    IsiSensorMode_t *pMode
);

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
);

/*****************************************************************************/
/**
 *          IsiQuerySensorSupportIss
 *
 * @brief   query mode sensor support, 
 *
 * @param   handle              Sensor instance handle
 * @param   modeNum             support mode number which get from GetCaps   
 * @param   pIsiSensorMode      point to the sensor mode
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiQuerySensorIss
(
    IsiSensorHandle_t handle,
    IsiSensorModeInfoArray_t *pIsiSensorModeArr
);

/*****************************************************************************/
/**
 *          IsiGetCapsIss
 *
 * @brief   fills in the correct pointers for the sensor description struct
 *
 * @param   handle      Sensor instance handle
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
);

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
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiSetupSensorIss
(
    IsiSensorHandle_t   handle,
    IsiSensorCaps_t    *pSensorCaps
);

/*****************************************************************************/
/**
 *          IsiCheckSensorConnectionIss
 *
 * @brief   Checks the connection to the camera sensor, if possible.
 *
 * @param   handle      Sensor instance handle
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_NULL_POINTER
 *
 *****************************************************************************/
RESULT IsiCheckSensorConnectionIss
(
    IsiSensorHandle_t   handle
);

/*****************************************************************************/
/**
 *          IsiGetSensorRevisionIss
 *
 * @brief   This function reads the sensor revision register and returns it.
 *
 * @param   handle      sensor instance handle
 * @param   pValue      pointer to value
 *
 * @return  Return the result of the function call.
 * @retval  RET_SUCCESS
 * @retval  RET_WRONG_HANDLE
 * @retval  RET_NULL_POINTER
 * @retval  RET_NOTSUPP
 *
 *****************************************************************************/
RESULT IsiGetSensorRevisionIss
(
    IsiSensorHandle_t   handle,
    uint32_t            *pValue
);

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
 * @retval  RET_NULL_POINTER
 * @retval  RET_WRONG_STATE
 *
 *****************************************************************************/
RESULT IsiSensorSetStreamingIss
(
    IsiSensorHandle_t   handle,
    bool_t              on
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
    uint64_t exposure
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

/*****************************************************************************/
/**
 *          IsiDumpAllRegisters
 *
 * @brief   dump registers to the specified file
 *
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
    const uint8_t      *fileName
);

#endif

#ifdef __cplusplus
}
#endif


/* @} isi */


#endif /* __ISI_H__ */
