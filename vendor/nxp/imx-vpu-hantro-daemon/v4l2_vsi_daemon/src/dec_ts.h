/****************************************************************************
*
*    Copyright (c) 2010-2012, Freescale Semiconductor, Inc. All rights reserved.
*    Copyright 2020 NXP
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

#ifndef _TIMESTAMP_H_
#define _TIMESTAMP_H_


/**
 * GST_CLOCK_TIME_NONE:
 *
 * Constant to define an undefined clock time.
 */

typedef long long TSM_TIMESTAMP;

typedef enum
{
  MODE_AI,
  MODE_FIFO,
} TSMGR_MODE;

#define TSM_TIMESTAMP_NONE ((long long)(-1))
#define TSM_KEY_NONE ((void *)0)

/**
 * GST_CLOCK_TIME_IS_VALID:
 * @time: clock time to validate
 *
 * Tests if a given #GstClockTime represents a valid defined time.
 */

#ifdef __cplusplus
#define EXTERN
#else
#define EXTERN extern
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * This function receive timestamp into timestamp manager.
 *
 * @param	handle		handle of timestamp manager.
 *
 * @param	timestamp	timestamp received
 *
 * @return	
 */
  EXTERN void TSManagerReceive (void *handle, TSM_TIMESTAMP timestamp);

  EXTERN void TSManagerReceive2 (void *handle, TSM_TIMESTAMP timestamp,
      int size);

  EXTERN void TSManagerFlush2 (void *handle, int size);

  EXTERN void TSManagerValid2 (void *handle, int size, void *key);

/*!
 * This function send the timestamp for next output frame.
 *
 * @param	handle		handle of timestamp manager.
 *
 * @return	timestamp for next output frame.
 */
  EXTERN TSM_TIMESTAMP TSManagerSend (void *handle);

  EXTERN TSM_TIMESTAMP TSManagerSend2 (void *handle, void *key);

  EXTERN TSM_TIMESTAMP TSManagerQuery2 (void *handle, void *key);

  EXTERN TSM_TIMESTAMP TSManagerQuery (void *handle);
/*!
 * This function resync timestamp handler when reset and seek
 *
 * @param	handle		handle of timestamp manager.
 *
 * @param	synctime    the postion time needed to set, if value invalid, position keeps original
 * 
 * @param	mode		playing mode (AI or FIFO)
 *
 * @return	
 */
  EXTERN void resyncTSManager (void *handle, TSM_TIMESTAMP synctime,
      TSMGR_MODE mode);
/*!
 * This function create and reset timestamp handler
 *
 * @param	ts_buf_size	 time stamp queue buffer size 
 * 
 * @return	
 */
  EXTERN void *createTSManager (int ts_buf_size);
/*!
 * This function destory timestamp handler
 *
 * @param	handle		handle of timestamp manager.
 * 
 * @return	
 */
  EXTERN void destroyTSManager (void *handle);
/*!
 * This function set  history buffer frame interval by fps_n and fps_d 
 *
 * @param	handle		handle of timestamp manager.
 * 
 * @param	framerate       the framerate to be set
 * 
 * @return	
 */
  EXTERN void setTSManagerFrameRate (void *handle, int fps_n, int fps_d);
//EXTERN void setTSManagerFrameRate(void * handle, float framerate);
/*!
 * This function set the current calculated Frame Interval
 *
 * @param	handle		handle of timestamp manager.
 * 
 * @return	
 */
  EXTERN TSM_TIMESTAMP getTSManagerFrameInterval (void *handle);
/*!
 * This function get  the current time stamp postion
 *
 * @param	handle		handle of timestamp manager.
 * 
 * @return	
 */
  EXTERN TSM_TIMESTAMP getTSManagerPosition (void *handle);
  EXTERN int getTSManagerPreBufferCnt (void *handle);

#ifdef __cplusplus
}
#endif

#endif /*_TIMESTAMP_H_ */
