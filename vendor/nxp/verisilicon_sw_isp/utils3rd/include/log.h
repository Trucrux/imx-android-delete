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

#ifndef _VSI_3RDLIBS_INCLUDE_LOG_H_
#define _VSI_3RDLIBS_INCLUDE_LOG_H_

#include <stdio.h>
#include <stdlib.h>

enum {
    ISP_LOG_LEVEL_NONE = 0, //No debug information is output.
    ISP_LOG_LEVEL_ERROR ,   //Logs all fatal errors.
    ISP_LOG_LEVEL_WARNING,  //Logs all warnings.
    ISP_LOG_LEVEL_FIXME,    //Logs all "fixme" messages. (Reserved)
    ISP_LOG_LEVEL_INFO,     //Logs all informational messages.
    ISP_LOG_LEVEL_DEBUG,    //Logs all debug messages.
    ISP_LOG_LEVEL_LOG,      //Logs all log messages.     (Reserved)
    ISP_LOG_LEVEL_TRACE,    //Logs all trace messages.   (Reserved)
    ISP_LOG_LEVEL_VERBOSE,  //Logs all level messages.
};

inline int isp_log_level() {
    char* szLogLevel = getenv("ISP_LOG_LEVEL");
    if (szLogLevel)
        return atoi(szLogLevel);
    else
        return ISP_LOG_LEVEL_WARNING;
}

// c++11 workaround empty variadic macro
#define __ALOG_INT(format, ...) "[%s] " format "\033[0m%s", LOGTAG, __VA_ARGS__

#ifdef ANDROID

#include <android/log.h>
#define ISP_INFO "VSI_ISP"

#define ALOGV(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_VERBOSE) __android_log_print(ANDROID_LOG_INFO, ISP_INFO, "\033[1;30;37mVERBOSE : " __ALOG_INT(__VA_ARGS__, "\n"))  // white

#define ALOGD(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_DEBUG) __android_log_print(ANDROID_LOG_INFO, ISP_INFO, "\033[1;30;37mDEBUG  : " __ALOG_INT(__VA_ARGS__, "\n"))  // white

#define ALOGI(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_INFO) __android_log_print(ANDROID_LOG_INFO, ISP_INFO, "\033[1;30;32mINFO   : " __ALOG_INT(__VA_ARGS__, "\n"))  // green

#define ALOGW(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_WARNING) __android_log_print(ANDROID_LOG_INFO, ISP_INFO, "\033[1;30;33mWARN   : " __ALOG_INT(__VA_ARGS__, "\n"))  // yellow

#define ALOGE(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_ERROR) __android_log_print(ANDROID_LOG_INFO, ISP_INFO, "\033[1;30;31mERROR  : " __ALOG_INT(__VA_ARGS__, "\n"))  // red

#else

#define ALOGV(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_VERBOSE) printf("\033[1;30;37mVERBOSE : " __ALOG_INT(__VA_ARGS__, "\n"))  // white

#define ALOGD(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_DEBUG) printf("\033[1;30;37mDEBUG  : " __ALOG_INT(__VA_ARGS__, "\n"))  // white

#define ALOGI(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_INFO) printf("\033[1;30;32mINFO   : " __ALOG_INT(__VA_ARGS__, "\n"))  // green

#define ALOGW(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_WARNING) printf("\033[1;30;33mWARN   : " __ALOG_INT(__VA_ARGS__, "\n"))  // yellow

#define ALOGE(...)\
    if (isp_log_level() >= ISP_LOG_LEVEL_ERROR) printf("\033[1;30;31mERROR  : " __ALOG_INT(__VA_ARGS__, "\n"))  // red

#endif

//#define TRACE_IN  ALOGI("enter %s", __PRETTY_FUNCTION__)
//#define TRACE_OUT ALOGI("leave %s", __PRETTY_FUNCTION__)

#endif  // _VSI_3RDLIBS_INCLUDE_LOG_H_
