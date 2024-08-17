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

#ifndef VSI_ENC_VIDEO_H2_H
#define VSI_ENC_VIDEO_H2_H
#include "vsi_enc.h"

/**
 * @file vsi_enc_video_h1.h
 * @brief V4L2 Daemon video process header file.
 * @version 0.10- Initial version
 */

/**
 * @brief encoder_check_codec_format_h1(), before encode, confirm if hw support
 * this format.
 * @param v4l2_enc_inst* h, encoder instance.
 * @return int, 0 is supported, -1 is unsupported.
 */
int encoder_check_codec_format_h1(v4l2_enc_inst* h);

/**
 * @brief encoder_init_h1(), init H1 data structures.
 * @param v4l2_enc_inst* h, encoder instance.
 * @return .
 */
void encoder_init_h1(v4l2_enc_inst* h);

/**
 * @brief encoder_get_attr_h1(), get encoder attributes.
 * @param v4l2_enc_inst fmt: codec format.
 * @return
 */
void encoder_get_attr_h1(v4l2_daemon_codec_fmt fmt, enc_inst_attr* attr);

/**
 * @brief encoder_get_input_h1(), write message from V4l2 parameter to encoder
 * instance.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param int32_t if_config: if init gop config of encIn.
 * @return void.
 */
void encoder_get_input_h1(v4l2_enc_inst* h, v4l2_daemon_enc_params* enc_params,
                          int32_t if_config);

/**
 * @brief encoder_set_parameter_h1(), set encoder parameters.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param int32_t if_config: if configure the encoder.
 * @return int: api return value.
 */
int encoder_set_parameter_h1(v4l2_enc_inst* h,
                             v4l2_daemon_enc_params* enc_params,
                             int32_t if_config);

/**
 * @brief encoder_start_h1(), start encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @return int: api return value.
 */
int encoder_start_h1(v4l2_enc_inst* h, uint32_t* stream_size);

/**
 * @brief encoder_encode_h1(), encoder encode.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @param uint32_t* codingType: coding type.
 * @return int: api return value.
 */
int encoder_encode_h1(v4l2_enc_inst* h, uint32_t* stream_size,
                      uint32_t* codingType);

/**
 * @brief encoder_find_next_pic_h1(), find buffer and gop structure of next
 * picture.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param BUFFER** p_buffer: buffer to encode
 * @param uint32_t* list_num: buffer id in buffer list.
 * @return int: api return value.
 */
int encoder_find_next_pic_h1(v4l2_enc_inst* h, BUFFER** p_buffer,
                             uint32_t* list_num);

/**
 * @brief reset_enc_h1(), reset encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int: api return value.
 */
void reset_enc_h1(v4l2_enc_inst *h);

/**
* @brief encoder_end_h1(), stream end, write eos.
* @param v4l2_enc_inst* h: encoder instance.
* @param uint32_t* stream_size: srteam size.
* @return int: api return value.
*/
int encoder_end_h1(v4l2_enc_inst* h, uint32_t* stream_size);

/**
 * @brief encoder_close_h1(), close encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int: api return value.
 */
int encoder_close_h1(v4l2_enc_inst* h);

#endif
