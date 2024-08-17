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

#ifndef VSI_ENC_VIDEO_H
#define VSI_ENC_VIDEO_H
#include "buffer_list.h"
#include "command_defines.h"
#include "daemon_instance.h"

/**
 * @file vsi_enc.h
 * @brief V4L2 Daemon video process header file.
 * @version 0.10- Initial version
 */

typedef enum {
  ENC_NONE,      // none.
  ENC_PAIRING,   // make input and output address togother?!
  ENC_INIT,      // init inst and encode the 1st frame.
  ENC_ENCODING,  // working.
  ENC_DRAIN,     // 2pass/muti-core flush, b-frames encoding.
  ENC_STOPPED,
  ENC_RELEASED,
} v4l2_inst_state;

typedef struct {
  int32_t has_h264;
  int32_t has_hevc;
  int32_t has_jpeg;
  int32_t has_vp8;
  int32_t max_width_hevc;
  int32_t max_width_h264;
  int32_t max_width_jpeg;
  int32_t max_width_vp8;
} configures;

typedef struct { uint32_t param_size; } enc_inst_attr;

typedef struct _v4l2_enc_inst v4l2_enc_inst;

typedef struct {
  void (*init)(v4l2_enc_inst* h);
  int32_t (*check_codec_format)(v4l2_enc_inst* h);
  void (*get_input)(v4l2_enc_inst* h, v4l2_daemon_enc_params* enc_params,
                    int32_t if_config);
  int32_t (*set_parameter)(v4l2_enc_inst* h, v4l2_daemon_enc_params* enc_params,
                           int32_t if_config);
  int32_t (*start)(v4l2_enc_inst* h, uint32_t* size_header_plus_i);
  int32_t (*encode)(v4l2_enc_inst* h, uint32_t* size_header_plus_i,
                    uint32_t* codingType);
  int32_t (*find_next_pic)(v4l2_enc_inst* h, BUFFER** p_buffer,
                           uint32_t* list_num);
  int32_t (*end)(v4l2_enc_inst* h, uint32_t* size_header_plus_i);
  int32_t (*close)(v4l2_enc_inst* h);
  void (*get_attr)(v4l2_daemon_codec_fmt fmt, enc_inst_attr* attr);
  void (*reset_enc)(v4l2_enc_inst* h);
} daemon_enc_callback_t;

struct _v4l2_enc_inst {
  v4l2_daemon_inst* handler;
  unsigned long instance_id;

  const void* inst;
  v4l2_inst_state state;

  uint32_t input_frame_cnt;
  uint32_t output_frame_cnt;

  v4l2_daemon_codec_fmt codec_fmt;
  void* enc_params;

  v4l2_daemon_enc_params mediaconfig;

  daemon_enc_callback_t func;
  uint32_t cbr_last_bitrate;
  uint32_t cbr_bitrate_change;
  uint32_t next_pic_type;

  int32_t curr_inbuf_idx;

  BUFFERLIST* bufferlist_reorder;  // buffer(inout addr+parms) push in display
                                   // order, get in h order.
  BUFFERLIST* bufferlist_input;
  BUFFERLIST* bufferlist_output;

  int32_t already_streamon;
  int32_t already_command_stop;
  int32_t drain_input_num_total;
  int32_t drain_input_num_now;
  int32_t flushbuf;

  configures configure_t;

  uint32_t* saveOriOutBuf;
  int64_t total_time;
  int64_t total_frames;
};

/**
 * @brief h26x_encode_cmd_processor(), process the input command. After
 V4L2_DAEMON_VIDIOC_BUF_RDY received,
          the input and output buffers are put into 2 bufferlist, respectively.
 If both the sizes of input
          and output buffers are none 0, it will pair the input and output
 buffer, and add to the another
          bufferlist. If one of the input and output buffers is 0, it will not
 pair the input and output buffers,
          and return a H26X_ENC_PAIRING state.
 * @param v4l2_enc_inst* h: daemon instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: v4l2 message.
 * @return int32_t value:The encoder state.
 */
int32_t enc_cmd_processor(v4l2_enc_inst* h, struct vsi_v4l2_msg* v4l2_msg);

/**
 * @brief h26x_encode_state_processor(), process the input state returned from
 h26x_encode_cmd_processor().
          If the state is H26X_ENC_INIT, all the parameters is ready for the
 encoder and will be set to
          respective structure and the encoder instance will be created. Then
 the stream is going to be
          encoded. If error happens, it will send orphan message to driver. If
 the state is H26X_ENC_ENCODING,
          the encoder is encoding the stream. If the state is H26X_ENC_DRAIN,
 the encoder is going to handle
          the tail of the stream, maybe last GOP or lookahead tail. If the state
 is H26X_ENC_STOPPED,
          the encoder is going to stop, destroy the encoder instance and the
 worker thread will exit soon.
 * @param v4l2_enc_inst* h: daemon instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: v4l2 message.
 * @return int32_t value:The encoder state after process.
 */
v4l2_inst_state enc_state_processor(v4l2_enc_inst* h,
                                    struct vsi_v4l2_msg* v4l2_msg);

void check_if_cbr_bitrate_changed(v4l2_enc_inst* h,
                                  v4l2_daemon_enc_params* enc_params);


int32_t calculate_level(v4l2_daemon_codec_fmt format, int32_t width, int32_t height,
                        uint32_t frameRateNum, uint32_t frameRateDenom, uint32_t bitPerSecond, int32_t profile);
uint32_t getMaxCpbSize( v4l2_daemon_codec_fmt codecFormat , int32_t levelIdx );

#endif
