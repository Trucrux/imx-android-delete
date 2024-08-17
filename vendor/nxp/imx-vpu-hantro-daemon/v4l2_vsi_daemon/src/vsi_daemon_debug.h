/****************************************************************************
*
*    Copyright 2019 - 2020 VeriSilicon Inc. All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#ifndef VSI_DEC_SW_DEBUG_H_
#define VSI_DEC_SW_DEBUG_H_
#include <stdio.h>
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#include "time.h"

#define HANTRO_LEVEL_NONE 0
#define HANTRO_LEVEL_ERROR 1
#define HANTRO_LEVEL_WARNING 2
#define HANTRO_LEVEL_FIXME 3
#define HANTRO_LEVEL_INFO 4
#define HANTRO_LEVEL_DEBUG 5
#define HANTRO_LEVEL_LOG 6
#define HANTRO_LEVEL_TRACE_REG 7
#define HANTRO_LEVEL_MEMDUMP 9
#define HANTRO_LEVEL_COUNT 10

extern int hantro_log_level;
extern char hantro_level[HANTRO_LEVEL_COUNT][20];
extern FILE *vsidaemonstdlog;

#ifdef ANDROID_DEBUG
void LogOutput(int level, const char *fmt, ...);
#define HANTRO_LOG(level, fmt, args...) \
    if (level < hantro_log_level) { \
        LogOutput(level, fmt, ##args); \
    }

#else
#define HANTRO_LOG(level, fmt, args...)                     \
  if (level < hantro_log_level) {                           \
    printf(__FILE__ ":%d:%s() %s " fmt, __LINE__, __func__, \
           hantro_level[level], ##args);                    \
    if (vsidaemonstdlog) fflush(vsidaemonstdlog);           \
  }
#endif
//#define SW_PERFORMANCE

#ifdef SW_PERFORMANCE
#define V4L2_START_SW_PERFORMANCE gettimeofday(&dec_start_time, NULL);
#else
#define V4L2_START_SW_PERFORMANCE
#endif

#ifdef SW_PERFORMANCE
#define V4L2_END_SW_PERFORMANCE                                       \
  gettimeofday(&dec_end_time, NULL);                                  \
  dec_cpu_time +=                                                     \
      ((double)1000000 * dec_end_time.tv_sec + dec_end_time.tv_usec - \
       1000000 * dec_start_time.tv_sec - dec_start_time.tv_usec) /    \
      1000000;

#else
#define V4L2_END_SW_PERFORMANCE
#endif

#ifdef SW_PERFORMANCE
#define V4L2_FINALIZE_SW_PERFORMANCE \
  printf("SW_PERFORMANCE %0.5f s\n", dec_cpu_time);
#else
#define V4L2_FINALIZE_SW_PERFORMANCE
#endif

#ifdef SW_PERFORMANCE
#define V4L2_FINALIZE_SW_PERFORMANCE_PP \
  printf("SW_PERFORMANCE_PP %0.5f\n", dec_cpu_time);
#else
#define V4L2_FINALIZE_SW_PERFORMANCE_PP
#endif

#endif
