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

/**
 * @file vsi_enc_video.c
 * @brief V4L2 Daemon video process header file.
 * @version 0.10- Initial version
 */
#include <math.h>

#include "buffer_list.h"
#include "fifo.h"
#include "vsi_daemon_debug.h"
#include "vsi_enc.h"

#include "ewl.h"
#ifndef USE_H1
#include "vsi_enc_video_h2.h"
#else
#include "vsi_enc_video_h1.h"
#endif
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int trans_msg_coding_type(int type) {
  if (type == 0)
    return FRAMETYPE_I;
  else if (type == 1)
    return FRAMETYPE_P;
  else if (type == 2)
    return FRAMETYPE_B;
  else {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Unsupported input.\n");
    ASSERT(0);
    return 0;
  }
}

static void enc_unmap_io_buff(v4l2_enc_inst *h,
                              v4l2_daemon_enc_buffers *io_buf) {
#ifndef USE_HW
    unmap_phy_addr_daemon(
        (uint32_t *)io_buf->busLuma,
        io_buf->busLumaSize + io_buf->busChromaUSize + io_buf->busChromaVSize);
#endif
    unmap_phy_addr_daemon(h->saveOriOutBuf, io_buf->outBufSize);
}

static void handle_encoder_err(v4l2_enc_inst *h, vsi_v4l2_msg *v4l2_msg,
                               v4l2_daemon_enc_buffers *io_buf,
                               int32_t stream_size, int32_t ret, int if_unmap) {
  if (if_unmap) {
    enc_unmap_io_buff(h, io_buf);
  }
  send_enc_orphan_msg(v4l2_msg, io_buf->inbufidx, io_buf->outbufidx,
                      stream_size, ret);
  h->func.close(h);
}

void check_if_cbr_bitrate_changed(v4l2_enc_inst *h,
                                  v4l2_daemon_enc_params *enc_params) {
  if (enc_params->specific.enc_h26x_cmd.picRc == 1 &&
      enc_params->specific.enc_h26x_cmd.hrdConformance == 1 &&
      enc_params->general.bitPerSecond != h->cbr_last_bitrate) {
    h->cbr_last_bitrate = enc_params->general.bitPerSecond;
    h->cbr_bitrate_change = 1;
  }
}

/**
 * @brief _enc_init_func_ptr(), init the function point of the encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return void
 */
static void _enc_init_func_ptr(v4l2_enc_inst *h) {
  ASSERT(h);
#ifndef USE_H1
  h->func.init = encoder_init_h2;
  h->func.check_codec_format = encoder_check_codec_format_h2;
  h->func.get_input = encoder_get_input_h2;
  h->func.set_parameter = encoder_set_parameter_h2;
  h->func.start = encoder_start_h2;
  h->func.encode = encoder_encode_h2;
  h->func.find_next_pic = encoder_find_next_pic_h2;
  h->func.end = encoder_end_h2;
  h->func.close = encoder_close_h2;
  h->func.get_attr = encoder_get_attr_h2;
  h->func.reset_enc = reset_enc_h2;
#else
  h->func.init = encoder_init_h1;
  h->func.check_codec_format = encoder_check_codec_format_h1;
  h->func.get_input = encoder_get_input_h1;
  h->func.set_parameter = encoder_set_parameter_h1;
  h->func.start = encoder_start_h1;
  h->func.encode = encoder_encode_h1;
  h->func.find_next_pic = encoder_find_next_pic_h1;
  h->func.end = encoder_end_h1;
  h->func.close = encoder_close_h1;
  h->func.get_attr = encoder_get_attr_h1;
  h->func.reset_enc = reset_enc_h1;
#endif
}

void pair_io_buffer(v4l2_enc_inst *h) {
    BUFFER *p_buffer_get_output = NULL;
    BUFFER *p_buffer_get_input = NULL;
    BUFFER *p_buffer_reorder = NULL;
    // pairing I/O buffers
    if (bufferlist_get_size(h->bufferlist_input) &&
        bufferlist_get_size(h->bufferlist_output)) {
      p_buffer_get_input = bufferlist_get_buffer(h->bufferlist_input);
      p_buffer_get_output = bufferlist_get_buffer(h->bufferlist_output);
      ASSERT((p_buffer_get_input != NULL) && (p_buffer_get_output != NULL));

      p_buffer_get_input->enc_cmd.io_buffer.busOutBuf =
          p_buffer_get_output->enc_cmd.io_buffer.busOutBuf;
      p_buffer_get_input->enc_cmd.io_buffer.outBufSize =
          p_buffer_get_output->enc_cmd.io_buffer.outBufSize;
      p_buffer_get_input->enc_cmd.io_buffer.outbufidx =
          p_buffer_get_output->enc_cmd.io_buffer.outbufidx;  // save buf idx.
      p_buffer_reorder = (BUFFER *)malloc(sizeof(BUFFER));
      memcpy(&p_buffer_reorder->enc_cmd, &p_buffer_get_input->enc_cmd,
             sizeof(struct v4l2_daemon_enc_params));
      p_buffer_reorder->frame_display_id =
          p_buffer_get_input->frame_display_id;  // output num is more than
                                                 // input num, pick the small
                                                 // one.
      bufferlist_push_buffer(h->bufferlist_reorder, p_buffer_reorder);

      free(p_buffer_get_input);
      free(p_buffer_get_output);
      bufferlist_remove(h->bufferlist_input, 0);
      bufferlist_remove(h->bufferlist_output, 0);
    }
}


/*void is_buffer_in_enqueue_list(v4l2_enc_inst *h) {
}
*/

/**
 * @brief enc_cmd_processor(), process the input command. After
 V4L2_DAEMON_VIDIOC_BUF_RDY received,
          the input and output buffers are put into 2 bufferlist, respectively.
 If both the sizes of input
          and output buffers are none 0, it will pair the input and output
 buffer, and add to the another
          bufferlist. If one of the input and output buffers is 0, it will not
 pair the input and output buffers,
          and return a H26X_ENC_PAIRING state.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: v4l2 message.
 * @return int32_t:The encoder state.
 */
int32_t enc_cmd_processor(v4l2_enc_inst *h, struct vsi_v4l2_msg *v4l2_msg) {
  BUFFER *p_buffer_input = NULL;
  BUFFER *p_buffer_output = NULL;

  //    uint32_t list_num = 0xffffffff;
  switch (v4l2_msg->cmd_id) {
    case V4L2_DAEMON_VIDIOC_BUF_RDY:
      if (v4l2_msg->params.enc_params.io_buffer.inbufidx != -1)  // input
      {
        p_buffer_input = (BUFFER *)malloc(sizeof(BUFFER));
        p_buffer_input->frame_display_id = h->input_frame_cnt++;
        if (v4l2_msg->param_type & UPDATE_INFO) {
          memcpy(&h->mediaconfig, &v4l2_msg->params.enc_params,
                 sizeof(struct v4l2_daemon_enc_params));
          //this var might not be updated in next BUFRDY msg
          h->mediaconfig.specific.enc_h26x_cmd.force_idr = 0;
          memcpy(&p_buffer_input->enc_cmd, &v4l2_msg->params.enc_params,
                 sizeof(struct v4l2_daemon_enc_params));
        } else {
          memcpy(&p_buffer_input->enc_cmd, &h->mediaconfig,
                 sizeof(struct v4l2_daemon_enc_params));
          memcpy(&p_buffer_input->enc_cmd.io_buffer,
                 &v4l2_msg->params.enc_params.io_buffer,
                 sizeof(v4l2_daemon_enc_buffers));
          memcpy(&p_buffer_input->enc_cmd.general,
                 &v4l2_msg->params.enc_params.general,
                 sizeof(v4l2_daemon_enc_general_cmd));
        }
        // HANTRO_LOG(HANTRO_LEVEL_INFO, "[0x%lx] New input: frame %d : %d :
        // %d\n", h->handler->instance_id,p_buffer_input->frame_display_id,
        // p_buffer_input->enc_cmd.io_buffer.bytesused,v4l2_msg->params.enc_params.io_buffer.inbufidx);
        bufferlist_push_buffer(h->bufferlist_input, p_buffer_input);
      } else if (v4l2_msg->params.enc_params.io_buffer.outbufidx !=
                 -1)  // output
      {
        p_buffer_output = (BUFFER *)malloc(sizeof(BUFFER));
        p_buffer_output->frame_display_id = h->output_frame_cnt++;
        // HANTRO_LOG(HANTRO_LEVEL_INFO, "[0x%lx] New output: frame %d state
        // %d\n", h->handler->instance_id, p_buffer_output->frame_display_id,
        // h->state);
        if (v4l2_msg->param_type & UPDATE_INFO) {
          memcpy(&h->mediaconfig, &v4l2_msg->params.enc_params,
                 sizeof(struct v4l2_daemon_enc_params));
          memcpy(&p_buffer_output->enc_cmd, &v4l2_msg->params.enc_params,
                 sizeof(struct v4l2_daemon_enc_params));
        } else {
          memcpy(&p_buffer_output->enc_cmd, &h->mediaconfig,
                 sizeof(struct v4l2_daemon_enc_params));
          memcpy(&p_buffer_output->enc_cmd.io_buffer,
                 &v4l2_msg->params.enc_params.io_buffer,
                 sizeof(v4l2_daemon_enc_buffers));
          memcpy(&p_buffer_output->enc_cmd.general,
                 &v4l2_msg->params.enc_params.general,
                 sizeof(v4l2_daemon_enc_general_cmd));
        }
        bufferlist_push_buffer(h->bufferlist_output, p_buffer_output);
      } else {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "No I/O buffer in BUF_RDY message.\n");
        ASSERT(0);
      }

      // pairing I/O buffers
      if(bufferlist_get_size(h->bufferlist_input) &&
         bufferlist_get_size(h->bufferlist_output) ){
          pair_io_buffer(h);
          if (((h->state == ENC_ENCODING) ||
               (h->state == ENC_PAIRING)) &&
              (h->already_streamon == 1)) {
            h->state = (h->already_command_stop) ? ENC_DRAIN : ENC_ENCODING;
          }
      }
      else if (h->drain_input_num_total > 0) {
            h->state = ENC_PAIRING;
      }
      break;

    case V4L2_DAEMON_VIDIOC_STREAMON:
      if (v4l2_msg->param_type == 0)  // no parameter
      {
      } else if (v4l2_msg->param_type ==
                 1)  // all parameters. include input output and parameters.
      {
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Has Param when streamon.\n");
      }
      h->state = ENC_INIT;
      h->already_streamon = 1;
      h->already_command_stop = h->drain_input_num_total =
          h->drain_input_num_now = 0;
      h->flushbuf = 1;
      break;

    case V4L2_DAEMON_VIDIOC_CMD_STOP:  // this is for flush.
      h->already_command_stop = 1;
      h->drain_input_num_total = bufferlist_get_size(h->bufferlist_input);
      h->drain_input_num_now = 0;
      h->state = ENC_DRAIN;
      break;

    case V4L2_DAEMON_VIDIOC_DESTROY_ENC:
    case V4L2_DAEMON_VIDIOC_EXIT:
      h->state = ENC_RELEASED;
      break;

    case V4L2_DAEMON_VIDIOC_ENC_RESET:
      h->func.reset_enc(h);
      h->state = ENC_STOPPED;
    break;

    case V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT:
      bufferlist_flush(h->bufferlist_input);
      send_cmd_orphan_msg(h->instance_id, V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT_DONE);
      h->state = ENC_STOPPED;
      break;

    case V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE:
      bufferlist_flush(h->bufferlist_output);
      h->func.reset_enc(h);
      send_cmd_orphan_msg(h->instance_id, V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE_DONE);
      h->state = ENC_STOPPED;
      break;

    case V4L2_DAEMON_VIDIOC_FAKE:
      break;
    default:
      break;
  }
  return 0;
}

static int64_t monotonic_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((int64_t)ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}

/**
 * @brief enc_state_processor(), process the input state returned from
 enc_cmd_processor().
          If the state is ENC_INIT, all the parameters is ready for the encoder
 and will be set to
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
 * @param v4l2_enc_inst* h: encoder instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: v4l2 message.
 * @return int32_t:The encoder state after process.
 */
v4l2_inst_state enc_state_processor(v4l2_enc_inst *h, vsi_v4l2_msg *v4l2_msg) {
  BUFFER *p_buffer = NULL;
  BUFFER *p_buffer_output = NULL;
  v4l2_daemon_enc_params *enc_params = NULL;
  v4l2_daemon_enc_buffers *io_buf = NULL;
  uint32_t list_num = 0xffffffff;
  uint32_t size_header_plus_i = 0;
  int32_t gop_tail_size = 0;
  enc_inst_attr attr;
  int codingType = 0;
  int ret = 0;
  int64_t start_time = monotonic_time();

  switch (h->state) {
    case ENC_INIT:
      if (h->flushbuf) {
        h->func.get_attr(h->codec_fmt, &attr);
        ASSERT(h->enc_params);
        memset(h->enc_params, 0, attr.param_size);
        h->func.init(h);
        h->flushbuf = 0;
      }
      /*init step 1: confirm encoder is supported by hw.*/
      if (h->func.check_codec_format(h) != 0) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "Not support format %d.\n",
                   h->codec_fmt);
        send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_ENC_NOT_SUPPORT);
      }

      /*init step 2: find the buffer to encode.*/
      p_buffer = bufferlist_get_head(h->bufferlist_reorder);  // match 0
      if (p_buffer == NULL) {
        HANTRO_LOG(HANTRO_LEVEL_INFO,
                   "No source buffer @ INIT state, wait...\n");
        return ENC_INIT;
      }
      enc_params = &p_buffer->enc_cmd;
      io_buf = &enc_params->io_buffer;
      bufferlist_remove(h->bufferlist_reorder, 0);

      /*init step 3: set all parameters including config, rate control, prep and
       * coding control*/
      /*make sure first encoded frame is I*/
      ret = h->func.set_parameter(h, enc_params, 1);
      if (ret) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "set_parameter() failed, ret=%d\n", ret);
        handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 1);
        free(p_buffer);  // already consumed
        send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_ENC_PARA);
        return ENC_STOPPED;
      }

      /*init step 4: encoder start, generate header.*/
      ret = h->func.start(h, &size_header_plus_i);
      if (ret) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "start() failed, ret=%d\n", ret);
        handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 1);
        free(p_buffer);  // already consumed
        return ENC_STOPPED;
      }

      int32_t header_size = size_header_plus_i;

      /*init step 5: encoder encode, encode the frame.*/
      ret = h->func.encode(h, &size_header_plus_i, &codingType);
      enc_unmap_io_buff(h, io_buf);
      if (ret != DAEMON_ENC_FRAME_READY && ret != DAEMON_ENC_FRAME_ENQUEUE) {
        HANTRO_LOG(HANTRO_LEVEL_INFO, "encode() @INIT state failed, ret=%d\n",
                   ret);
        handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 0);
        free(p_buffer);  // already consumed
        return ENC_STOPPED;
      }

      int32_t payload_size = size_header_plus_i - header_size;

      h->total_frames++;
      h->total_time += monotonic_time() - start_time;
      /*init step 5: inform driver when finished one frame.*/
      v4l2_msg->param_type = trans_msg_coding_type(codingType);
      v4l2_msg->params.enc_params.io_buffer.timestamp = io_buf->timestamp;
      send_enc_orphan_msg(v4l2_msg, io_buf->inbufidx, io_buf->outbufidx,
                          size_header_plus_i, ret);

      //payload_size = 0; hard code for test
      /* if the first frame is not encoded correctly, stop the encoder and inform driver. */
      if(payload_size == 0 /*&& ret == 0*/) {//assume there is no enqueue ret.
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "encode() @INIT state failed, payload size is 0. ret=%d\n", ret);
          handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, DAEMON_ERR_ENC_FATAL_ERROR, 0);
          free(p_buffer);   //already consumed
          return ENC_STOPPED;
      }

      free(p_buffer);
      h->state = ENC_ENCODING;
      break;

    case ENC_ENCODING:
      /*encoding step 1: find next picture, get buffer and set gop structure.*/
      h->func.find_next_pic(h, &p_buffer, &list_num);
      if (p_buffer == NULL) {
        break;
      }
      enc_params = &p_buffer->enc_cmd;
      io_buf = &enc_params->io_buffer;
      bufferlist_remove(h->bufferlist_reorder, list_num);

      /*encoding step 2: set all parameters including config, rate control, prep
       * and coding control*/
      ret = h->func.set_parameter(h, enc_params, 0);
      if (ret) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "set_parameter() failed, ret=%d\n", ret);
        handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 1);
        free(p_buffer);  // already consumed
        send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_ENC_PARA);
        return ENC_STOPPED;
      }

      if (h->cbr_bitrate_change == 1 && h->next_pic_type == 0) {
        h->func.end(h, &size_header_plus_i);
        h->func.close(h);
        enc_unmap_io_buff(h, io_buf);
        h->cbr_bitrate_change = 0;
        h->state = ENC_INIT;
        p_buffer->enc_cmd.specific.enc_h26x_cmd.qpHdr = -1;  // Let rc
                                                             // calculate.
        bufferlist_push_buffer(h->bufferlist_reorder,
                               p_buffer);  // return this buffer.
        return h->state;
      }
      /*encoding step 3: encoder encode, encode the frame.*/
      ret = h->func.encode(h, &size_header_plus_i, &codingType);
      enc_unmap_io_buff(h, io_buf);
      if (ret != DAEMON_ENC_FRAME_READY && ret != DAEMON_ENC_FRAME_ENQUEUE) {
        HANTRO_LOG(HANTRO_LEVEL_INFO, "encode() failed, ret=%d\n", ret);
        handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 0);
        free(p_buffer);  // already consumed
        return ENC_STOPPED;
      }
      h->total_frames++;
      h->total_time += monotonic_time() - start_time;

      /* hard code for test overflow*/
      /*if(h->total_frames == 2) {
          size_header_plus_i = 0;
      }*/

      /*encoding step 4: inform driver when finished one frame.*/
      if(size_header_plus_i > 0) {
          v4l2_msg->param_type = trans_msg_coding_type(codingType);
          v4l2_msg->params.enc_params.io_buffer.timestamp = io_buf->timestamp;
          send_enc_orphan_msg(v4l2_msg, io_buf->inbufidx, io_buf->outbufidx,
                              size_header_plus_i, ret);
      }
      else if(size_header_plus_i == 0) {
          v4l2_msg->params.enc_params.io_buffer.timestamp = io_buf->timestamp;
          send_enc_orphan_msg(v4l2_msg, io_buf->inbufidx, -1, size_header_plus_i, ret);

          p_buffer_output = (BUFFER *)malloc(sizeof(BUFFER));
          memcpy(p_buffer_output, p_buffer, sizeof(BUFFER));
          bufferlist_push_buffer(h->bufferlist_output, p_buffer_output);//return this buffer.
          pair_io_buffer(h);
      }

      if (h->bufferlist_reorder->size) {
        send_fake_cmd(h->handler);
      }
      free(p_buffer);  // already consumed
      break;

    case ENC_DRAIN:
      /*drain step 1: consume left input buffers.*/
      if (h->drain_input_num_total > 0) {
        gop_tail_size = bufferlist_get_size(h->bufferlist_reorder);
        for (int32_t itmp = 0; itmp < gop_tail_size; itmp++) {
          /*find next picture.*/
          h->func.find_next_pic(h, &p_buffer, &list_num);
          if (p_buffer == NULL) {
            HANTRO_LOG(HANTRO_LEVEL_INFO,
                       "Can't find paired I/O buffers @ DRAIN state\n");
            send_enc_orphan_msg(v4l2_msg, -1, -1, size_header_plus_i, DAEMON_ERR_ENC_BUF_MISSED);
            goto _label_enc_stopped;
          }
          enc_params = &p_buffer->enc_cmd;
          io_buf = &enc_params->io_buffer;

          /*set all parameters including config, rate control, prep and coding
           * control*/
          ret = h->func.set_parameter(h, enc_params, 0);
          if (ret) {
            HANTRO_LOG(HANTRO_LEVEL_ERROR, "set_parameter() failed, ret=%d\n",
                       ret);
            handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 1);
            free(p_buffer);  // already consumed
            send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_ENC_PARA);
            return ENC_STOPPED;
          }

          bufferlist_remove(h->bufferlist_reorder, list_num);

          /*encode the frame.*/
          ret = h->func.encode(h, &size_header_plus_i, &codingType);
          enc_unmap_io_buff(h, io_buf);

          h->drain_input_num_now++;
          if (ret != DAEMON_ENC_FRAME_READY && ret != DAEMON_ENC_FRAME_ENQUEUE) {
            HANTRO_LOG(HANTRO_LEVEL_INFO,
                       "encode failed @ DRAIN state, ret=%d\n", ret);
            handle_encoder_err(h, v4l2_msg, io_buf, size_header_plus_i, ret, 1);
            free(p_buffer);  // already consumed
            return ENC_STOPPED;
          }
          h->total_frames++;
          h->total_time += monotonic_time() - start_time;
          /*inform driver when finished one frame.*/

          if(size_header_plus_i > 0) {
              v4l2_msg->param_type = trans_msg_coding_type(codingType);
              if (h->drain_input_num_now == h->drain_input_num_total) {
                  h->drain_input_num_total = 0;  // will send eos
              }
              v4l2_msg->params.enc_params.io_buffer.timestamp = io_buf->timestamp;
              send_enc_orphan_msg(v4l2_msg, io_buf->inbufidx, io_buf->outbufidx,
                                  size_header_plus_i, ret);
          }
          /*acctually, in drain state, the output buffer is enough even if we don't return the buffer.*/
          else if(size_header_plus_i == 0) {
              v4l2_msg->params.enc_params.io_buffer.timestamp = io_buf->timestamp;
              send_enc_orphan_msg(v4l2_msg, io_buf->inbufidx, -1, size_header_plus_i, ret);

              p_buffer_output = (BUFFER *)malloc(sizeof(BUFFER));
              memcpy(p_buffer_output, p_buffer, sizeof(BUFFER));
              bufferlist_push_buffer(h->bufferlist_output, p_buffer_output);//return this buffer.
          }
          free(p_buffer);  // already consumed
          p_buffer = NULL;          
        }
      }
      /*drain step 2: send eos after all left input buffer consumed.*/
      else if (h->drain_input_num_total == 0) {
        p_buffer = bufferlist_get_buffer(h->bufferlist_output);
        if (p_buffer ==
            NULL) {  // wait destination buffer for sequence end content
          break;
        }
        enc_params = &p_buffer->enc_cmd;
        io_buf = &enc_params->io_buffer;

        h->func.get_input(h, enc_params, 0);

        h->func.end(h, &size_header_plus_i);

        unmap_phy_addr_daemon(h->saveOriOutBuf, io_buf->outBufSize);
        send_enc_outputbuf_orphan_msg(v4l2_msg, io_buf->outbufidx,
                                      size_header_plus_i, LAST_BUFFER_FLAG);
        h->drain_input_num_total = -1;
        bufferlist_remove(h->bufferlist_output, 0);
        free(p_buffer);
        send_fake_cmd(h->handler);
        break;
      }
      /*drain step 3: send LAST_BUFFER_FLAG.*/
      else if (h->drain_input_num_total == -1) {
        p_buffer = bufferlist_get_buffer(h->bufferlist_output);
        if (p_buffer == NULL) {  // wait destination buffer for LAST_BUFFER
          break;
        }
        send_enc_outputbuf_orphan_msg(v4l2_msg,
                                      p_buffer->enc_cmd.io_buffer.outbufidx, 0,
                                      LAST_BUFFER_FLAG);
        h->drain_input_num_total = -2;
        bufferlist_remove(h->bufferlist_output, 0);
        free(p_buffer);
        h->state = ENC_STOPPED;
        h->input_frame_cnt = 0;
        break;
      }
      break;

    case ENC_STOPPED:
      if (h->total_time > 0) {
        HANTRO_LOG(HANTRO_LEVEL_INFO,
                   "Encoded frames: %ld, encode fps: %0.2f\n", h->total_frames,
                   ((double)h->total_frames * 1000000) / h->total_time);
      }
      h->func.close(h);
      h->state = ENC_NONE;
      return ENC_STOPPED;
    case ENC_RELEASED:
      h->func.close(h);
      h->state = ENC_NONE;
      return ENC_RELEASED;
    default:
      break;
  }
  return h->state;

_label_enc_stopped:
  h->func.close(h);
  return ENC_STOPPED;
}

/**
 * @brief vsi_encoder_proc(), encoder message processor.
 * @param void* codec: encoder instance.
 * @return int32_t: 0: succeed; 1: encoder is destroy-able.
 */
static int32_t vsi_encoder_proc(void *codec, struct vsi_v4l2_msg *msg) {
  v4l2_enc_inst *h = (v4l2_enc_inst *)codec;
  v4l2_inst_state state;

  ASSERT(h);

  enc_cmd_processor(h, msg);
  state = enc_state_processor(h, msg);

  return (state == ENC_RELEASED);
}

/**
 * @brief vsi_destroy_encoder(), destroy encoder inst.
 * @param void* codec: encoder instance.
 * @return int32_t: 0: succeed; Others: failure.
 */
static int32_t vsi_destroy_encoder(void *codec) {
  v4l2_enc_inst *h = (v4l2_enc_inst *)codec;

  if (h == NULL) {
    return -1;
  }

  h->func.close(h);

  if (h->bufferlist_reorder) {
    bufferlist_destroy(h->bufferlist_reorder);
    free(h->bufferlist_reorder);
    h->bufferlist_reorder = NULL;
  }
  if (h->bufferlist_input) {
    bufferlist_destroy(h->bufferlist_input);
    free(h->bufferlist_input);
    h->bufferlist_input = NULL;
  }
  if (h->bufferlist_output) {
    bufferlist_destroy(h->bufferlist_output);
    free(h->bufferlist_output);
    h->bufferlist_output = NULL;
  }

  if (h->enc_params) {
    free(h->enc_params);
    h->enc_params = NULL;
  }

  return 0;
}

/**
 * @brief vsi_create_encoder(), create encoder inst.
 * @param v4l2_daemon_inst* daemon_inst: daemon instance.
 * @return void *: encoder instance.
 */
void *vsi_create_encoder(v4l2_daemon_inst *daemon_inst) {
  v4l2_enc_inst *h = NULL;
  enc_inst_attr attr;

  ASSERT(daemon_inst->codec_mode == DAEMON_MODE_ENCODER);

  h = (v4l2_enc_inst *)malloc(sizeof(v4l2_enc_inst));
  ASSERT(h);
  memset(h, 0, sizeof(v4l2_enc_inst));

  h->handler = daemon_inst;
  h->codec_fmt = daemon_inst->codec_fmt;
  h->instance_id = daemon_inst->instance_id;
  _enc_init_func_ptr(h);

  h->func.get_attr(h->codec_fmt, &attr);
  h->enc_params = malloc(attr.param_size);
  ASSERT(h->enc_params);
  memset(h->enc_params, 0, attr.param_size);

  h->bufferlist_reorder = (BUFFERLIST *)malloc(sizeof(BUFFERLIST));
  h->bufferlist_input = (BUFFERLIST *)malloc(sizeof(BUFFERLIST));
  h->bufferlist_output = (BUFFERLIST *)malloc(sizeof(BUFFERLIST));
  ASSERT(h->bufferlist_reorder != NULL);
  ASSERT(h->bufferlist_input != NULL);
  ASSERT(h->bufferlist_output != NULL);

  bufferlist_init(h->bufferlist_reorder, MAX_BUFFER_SIZE);
  bufferlist_init(h->bufferlist_input, MAX_BUFFER_SIZE);
  bufferlist_init(h->bufferlist_output, MAX_BUFFER_SIZE);

  h->func.init(h);

  daemon_inst->func.proc = vsi_encoder_proc;
  daemon_inst->func.destroy = vsi_destroy_encoder;
  daemon_inst->func.in_source_change = NULL;

  return (void *)h;
}

/**
 * @brief vsi_enc_get_codec_format(), get supported codec formats.
 * @param struct vsi_v4l2_dev_info *hwinfo: output info.
 * @return int, 0 means support at least 1 format, -1 means error.
 */
int vsi_enc_get_codec_format(struct vsi_v4l2_dev_info *hwinfo) {
  EWLHwConfig_t configure_t = {0};
  int core_num = 0, has_hevc = 0, has_h264 = 0, has_av1 = 0, has_vp8 = 0,
      has_vp9 = 0, has_jpeg = 0;

  hwinfo->encformat = 0;

#if (defined(NXP) && !defined(USE_H1))  // nxp v2
  core_num = EWLGetCoreNum();
  ASSERT(core_num != 0);

  for (int i = 0; i < core_num; i++) {
    configure_t = EWLReadAsicConfig(i);
    has_hevc |= configure_t.hevcEnabled;
    has_h264 |= configure_t.h264Enabled;
    has_vp9 |= configure_t.vp9Enabled;
    has_jpeg |= configure_t.jpegEnabled;
  }
  hwinfo->enc_corenum = core_num;
  hwinfo->enc_isH1 = 0;

#elif (!defined(NXP) && !defined(USE_H1))  // current vc8000e(v2,2020.06)
  core_num = EWLGetCoreNum(NULL);
  ASSERT(core_num != 0);

  for (int i = 0; i < core_num; i++) {
    configure_t = EWLReadAsicConfig(i, NULL);
    has_h264 |= configure_t.h264Enabled;
    has_hevc |= configure_t.hevcEnabled;
    has_av1 |= configure_t.av1Enabled;
    has_vp9 |= configure_t.vp9Enabled;
    has_jpeg |= configure_t.jpegEnabled;
  }
  hwinfo->enc_corenum = core_num;
  hwinfo->enc_isH1 = 0;
#elif (defined(NXP) && defined(USE_H1))    // h1 single core
  core_num = 1;

  configure_t = EWLReadAsicConfig();
  has_h264 |= configure_t.h264Enabled;
  has_jpeg |= configure_t.jpegEnabled;
  has_vp8 |= configure_t.vp8Enabled;

  hwinfo->enc_corenum = core_num;
  hwinfo->enc_isH1 = 1;
#endif

  hwinfo->encformat = ((has_hevc << ENC_HAS_HEVC) | (has_h264 << ENC_HAS_H264) |
                       (has_av1 << ENC_HAS_AV1) | (has_vp8 << ENC_HAS_VP8) |
                       (has_vp9 << ENC_HAS_VP9) | (has_jpeg << ENC_HAS_JPEG));
  if (hwinfo->encformat)
    return 0;
  else
    return -1;
}

#define HEVC_LEVEL_NUM 13
#define H264_LEVEL_NUM 20

const uint32_t VCEncMaxPicSizeHevc[ HEVC_LEVEL_NUM ] =
{
  36864, 122880, 245760, 552960, 983040, 2228224, 2228224, 8912896,
  8912896, 8912896, 35651584, 35651584, 35651584
};
const uint32_t VCEncMaxFSH264[ H264_LEVEL_NUM ] =
{
  99*256, 99*256, 396*256, 396*256, 396*256, 396*256, 792*256, 1620*256
  , 1620*256, 3600*256, 5120*256, 8192*256, 8192*256
  , 8704*256, 22080*256, 36864*256, 36864*256
  , 139264*256, 139264*256, 139264*256
};

const unsigned long long VCEncMaxSBPSHevc[ HEVC_LEVEL_NUM ] =
{
  552960, 3686400, 7372800, 16588800, 33177600, 66846720, 133693440, 267386880,
  534773760, 1069547520, 1069547520, 2139095040, 4278190080
};
const unsigned long long  VCEncMaxSBPSH264[ H264_LEVEL_NUM ] =
{
  1485*256, 1485*256, 3000*256, 6000*256, 11880*256, 11880*256, 19800*256, 20250*256
  , 40500*256, 108000*256, 216000*256, 245760*256, 245760*256
  , 522240*256, 589824*256, 983040*256, 2073600*256
  , 4177920*256, 8355840*256, 16711680ULL*256
};
const uint32_t VCEncMaxCPBSHevc[ HEVC_LEVEL_NUM ] =
{
  350000, 1500000, 3000000, 6000000, 10000000, 12000000, 20000000,
  25000000, 40000000, 60000000, 60000000, 120000000, 240000000
};
const uint32_t VCEncMaxCPBSH264[ H264_LEVEL_NUM ] =
{
  175000, 350000, 500000, 1000000, 2000000, 2000000, 4000000, 4000000
  , 10000000, 14000000, 20000000, 25000000, 62500000
  , 62500000, 135000000, 240000000, 240000000
  , 240000000, 480000000, 800000000
};

uint32_t getMaxCpbSize( v4l2_daemon_codec_fmt codecFormat , i32 levelIdx )
{
  ASSERT( levelIdx >= 0);
  i32 level      = MAX(0, levelIdx);
  u32 maxCpbSize = 0;

  switch( codecFormat )
  {
    case V4L2_DAEMON_CODEC_ENC_HEVC:
      ASSERT( level < HEVC_LEVEL_NUM );
      level      = MIN( level, (HEVC_LEVEL_NUM - 1));
      maxCpbSize = VCEncMaxCPBSHevc[ level ];
      break;

    case V4L2_DAEMON_CODEC_ENC_H264:
      ASSERT( level < H264_LEVEL_NUM );
      level      = MIN( level, (H264_LEVEL_NUM - 1));
      maxCpbSize = VCEncMaxCPBSH264[ level ];
      break;

    case V4L2_DAEMON_CODEC_ENC_AV1:
      break;

    case V4L2_DAEMON_CODEC_ENC_VP9:
      break;

    default:
      break;
  }

  return maxCpbSize;
}

static uint32_t getMaxPicSize( v4l2_daemon_codec_fmt codecFormat , i32 levelIdx )
{
  ASSERT( levelIdx >= 0);
  i32 level      = MAX(0, levelIdx);
  u32 maxPicSize = 0;

  switch( codecFormat )
  {
    case V4L2_DAEMON_CODEC_ENC_HEVC:
      ASSERT( level < HEVC_LEVEL_NUM );
      level      = MIN( level, (HEVC_LEVEL_NUM - 1));
      maxPicSize = VCEncMaxPicSizeHevc[ level ];
      break;

    case V4L2_DAEMON_CODEC_ENC_H264:
      ASSERT( level < H264_LEVEL_NUM );
      level      = MIN( level, (H264_LEVEL_NUM - 1));
      maxPicSize = VCEncMaxFSH264[ level ];
      break;

    case V4L2_DAEMON_CODEC_ENC_AV1:
      break;

    case V4L2_DAEMON_CODEC_ENC_VP9:
      break;

    default:
      break;
  }

  return maxPicSize;
}

static unsigned long long getMaxSBPS( v4l2_daemon_codec_fmt codecFormat , int32_t levelIdx )
{
  ASSERT( levelIdx >= 0);
  i32 level   = MAX(0, levelIdx);
  u64 maxSBPS = 0;

  switch( codecFormat )
  {
    case V4L2_DAEMON_CODEC_ENC_HEVC:
      ASSERT( level < HEVC_LEVEL_NUM );
      level   = MIN( level, (HEVC_LEVEL_NUM - 1));
      maxSBPS = VCEncMaxSBPSHevc[ level ];
      break;

    case V4L2_DAEMON_CODEC_ENC_H264:
      ASSERT( level < H264_LEVEL_NUM );
      level   = MIN( level, (H264_LEVEL_NUM - 1));
      maxSBPS = VCEncMaxSBPSH264[ level ];
      break;

    case V4L2_DAEMON_CODEC_ENC_AV1:
      break;

    case V4L2_DAEMON_CODEC_ENC_VP9:
      break;

    default:
      break;
  }

  return maxSBPS;
}

#ifdef USE_H1
#define H1_H264_LVL_NUM	16
static const uint32_t MaxBRH264[H1_H264_LVL_NUM] = {
    76800, 153600, 230400, 460800, 921600, 2400000, 4800000, 4800000, 12000000,
    16800000, 24000000, 24000000, 60000000, 60000000, 162000000, 288000000
};

static uint32_t getMaxBR(v4l2_daemon_codec_fmt codecFormat, int32_t levelIdx, int32_t profile)
{
    ASSERT(levelIdx >= 0);
    int32_t level = MAX(0, levelIdx);
    uint32_t maxBR = 0;

    switch(codecFormat)
    {
    case V4L2_DAEMON_CODEC_ENC_H264:
        level = MIN(level, (H1_H264_LVL_NUM - 1));
        maxBR = MaxBRH264[level];
        break;
    default:
        break;
    }

    return maxBR;
}
#else		//USE_H1
static const uint32_t VCEncMaxBR[HEVC_LEVEL_NUM + H264_LEVEL_NUM] = {
  /* HEVC Level Limits */
  350000, 1500000, 3000000, 6000000, 10000000, 12000000, 20000000,
  25000000, 40000000, 60000000, 60000000, 120000000, 240000000
  /* H.264 Level Limits */
  , 64000, 128000, 192000, 384000, 768000, 2000000, 4000000, 4000000
  , 10000000, 14000000, 20000000, 20000000, 50000000
  , 50000000, 135000000, 240000000, 240000000
  , 240000000, 480000000, 800000000
};

static uint32_t getMaxBR(v4l2_daemon_codec_fmt codecFormat, int32_t level, int32_t profile)
{
  float h264_high10_factor = 1;
  if(profile == 11)				//VCENC_H264_HIGH_PROFILE
    h264_high10_factor = 1.25;
  else if(profile == 12)		//VCENC_H264_HIGH_10_PROFILE
    h264_high10_factor = 3.0;
  //tier is always main for now
  if (codecFormat == V4L2_DAEMON_CODEC_ENC_HEVC)
    return VCEncMaxBR[level] * h264_high10_factor;
  if (codecFormat == V4L2_DAEMON_CODEC_ENC_H264)
    return VCEncMaxBR[level + HEVC_LEVEL_NUM] * h264_high10_factor;
  return 0;
}
#endif	//USE_H1

int32_t calculate_level(v4l2_daemon_codec_fmt     format, int32_t width, int32_t height,
                            uint32_t frameRateNum, uint32_t frameRateDenom, uint32_t bitPerSecond, int32_t profile)
{
    uint32_t sample_per_picture = width * height;
    uint32_t sample_per_second = sample_per_picture * frameRateNum / frameRateDenom;
    int32_t i = 0, j = 0, k = 0, leveIdx = 0;
    int32_t maxLevel = 0;
    switch(format)
    {
      case V4L2_DAEMON_CODEC_ENC_HEVC:
        maxLevel = HEVC_LEVEL_NUM - 1;
        break;
      case V4L2_DAEMON_CODEC_ENC_H264:
        maxLevel = H264_LEVEL_NUM - 1;
        break;
      default:
        break;
    }

    if(sample_per_picture >  getMaxPicSize(format, maxLevel) || sample_per_second > getMaxSBPS(format, maxLevel))
    {
      HANTRO_LOG(HANTRO_LEVEL_INFO, "encoder level max.\n");
    }
    for(i = 0; i < maxLevel; i++)
    {
      if(sample_per_picture <= getMaxPicSize(format, i))
        break;
    }
    for(j = 0; j < maxLevel; j++)
    {
      if(sample_per_second <= getMaxSBPS(format, j))
        break;
    }
    for (k = 0; k < maxLevel; k++)
    {
        if(bitPerSecond <= getMaxBR(format, k, profile))
           break;
    }

    leveIdx = MAX(MAX(i, j), k);
    return leveIdx;
}

