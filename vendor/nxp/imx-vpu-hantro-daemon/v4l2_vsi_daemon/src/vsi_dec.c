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
 * @file vsi_dec.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */
#include <dlfcn.h>
#include "sys/mman.h"

#include "dec_dpb_buff.h"
#include "vsi_dec.h"
#ifdef NXP_TIMESTAMP_MANAGER
#include "dec_ts.h"
#endif
#ifdef VSI_CMODEL
#include "tb_cfg.h"
#endif

v4l2_inst_dec_event vsi_dec_handle_io_buffer(v4l2_dec_inst *h,
                                             struct vsi_v4l2_msg *v4l2_msg);

char dec_state_name[DAEMON_DEC_STATE_TOTAL][50] = {
    "DAEMON_DEC_STATE_OPEN",          "DAEMON_DEC_STATE_INIT",
    "DAEMON_DEC_STATE_CAPTURE_SETUP", "DAEMON_DEC_STATE_DECODE",
    "DAEMON_DEC_STATE_SOURCE_CHANGE", "DAEMON_DEC_STATE_SEEK",
    "DAEMON_DEC_STATE_END_OF_STREAM", "DAEMON_DEC_STATE_DRAIN",
    "DAEMON_DEC_STATE_STOPPED",       "DAEMON_DEC_STATE_DESTROYED",
};

char dec_event_name[DEC_DECODING_TOTAL_EVENT][50] = {
    "DEC_EMPTY_EVENT",
    "DEC_RECEIVE_CAPTUREON_EVENT",
    "DEC_RECEIVE_CAPTUREOFF_EVENT",
    "DEC_RECEIVE_OUTPUTON_EVENT",
    "DEC_RECEIVE_OUTPUTOFF_EVENT",
    "DEC_RECEIVE_CMDSTOP_EVENT",
    "DEC_RECEIVE_CMDSTART_EVENT",
    "DEC_RECEIVE_DESTROY_EVENT",
    "DEC_BUFFER_EVENT",
    "DEC_LAST_PIC_EVENT",
    "DEC_CACHE_IO_BUFFER_EVENT",
    "DEC_FAKE_EVENT",
    "DEC_SOURCE_CHANGE_EVENT",
    "DEC_PIC_DECODED_EVENT",
    "DEC_GOT_EOS_MARK_EVENT",
    "DEC_NO_DECODING_BUFFER_EVENT",
    "DEC_WAIT_DECODING_BUFFER_EVENT",
    "DEC_PENDING_FLUSH_EVENT",
    "DEC_DECODING_ERROR_EVENT",
    "DEC_FATAL_ERROR_EVENT",
};
char cmd_name[V4L2_DAEMON_VIDIOC_TOTAL_AMOUNT][50] = {
    "V4L2_DAEMON_VIDIOC_STREAMON",
    "V4L2_DAEMON_VIDIOC_BUF_RDY",
    "V4L2_DAEMON_VIDIOC_CMD_STOP",
    "V4L2_DAEMON_VIDIOC_DESTROY_ENC",
    "V4L2_DAEMON_VIDIOC_ENC_RESET",
    "V4L2_DAEMON_VIDIOC_FAKE",
    "V4L2_DAEMON_VIDIOC_S_EXT_CTRLS",
    "V4L2_DAEMON_VIDIOC_RESET_BITRATE",
    "V4L2_DAEMON_VIDIOC_CHANGE_RES",
    "V4L2_DAEMON_VIDIOC_G_FMT",
    "V4L2_DAEMON_VIDIOC_S_SELECTION",
    "V4L2_DAEMON_VIDIOC_S_FMT",
    "V4L2_DAEMON_VIDIOC_PACKET",
    "V4L2_DAEMON_VIDIOC_STREAMON_CAPTURE",
    "V4L2_DAEMON_VIDIOC_STREAMON_OUTPUT",
    "V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE",
    "V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT",
    "V4L2_DAEMON_VIDIOC_CMD_START",
    "V4L2_DAEMON_VIDIOC_FRAME",
    "V4L2_DAEMON_VIDIOC_DESTROY_DEC",
};

char hantro_level[10][20] = {"",      "ERROR", "WARNING",  "",        "INFO",
                             "DEBUG", "LOG",   "TRACEREG", "MEMDUMP", ""};
int hantro_log_level;
#ifdef VSI_CMODEL
struct TBCfg tb_cfg;
uint32_t b_frames;
uint32_t test_case_id;
#endif

#define INVALIDE_DEC_TABLE_INDEX -1
#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))
#define VSI_TS_BUFFER_LENGTH_DEFAULT (1024)
#define SECURE_MODE_SKIP_LEN (16)
extern vsi_v4l2m2m2_deccodec vsidaemon_hevc_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_h264_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_vp8_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_vp9_v4l2m2m_decoder;
#ifdef HAS_FULL_DECFMT
extern vsi_v4l2m2m2_deccodec vsidaemon_vc1l_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_vc1g_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_jpeg_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_rv_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_avs_v4l2m2m_decoder;

extern vsi_v4l2m2m2_deccodec vsidaemon_mpeg2_v4l2m2m_decoder;
extern vsi_v4l2m2m2_deccodec vsidaemon_mpeg4_v4l2m2m_decoder;
#endif  //#ifdef HAS_FULL_DECFMT
int vsi_dec_find_event_index_in_fsm_table(
    v4l2_dec_inst *h, v4l2_inst_dec_event dec_receive_event);
static int vsi_dec_check_crop(v4l2_dec_inst *h, vsi_v4l2_dec_picture *new_pic);
static v4l2_inst_dec_event vsi_dec_handle_cropchange(v4l2_dec_inst *h);

uint64_t capture_tot_size = 0;

/**
 * @brief vsi_dec_get_codec_format(), get h supported codec formats.
 * @param struct vsi_v4l2_dev_info *hwinfo: output info.
 * @return int, 0 means support at least 1 format, -1 means error.
 */
int vsi_dec_get_codec_format(struct vsi_v4l2_dev_info *hwinfo) {
  int core_num = 0;

#ifdef VSI_CMODEL
  core_num = 1;
  hwinfo->dec_corenum = core_num;
  hwinfo->decformat = 0xFFFFFFFF;
  hwinfo->max_dec_resolution = 4096;
#else
  DWLHwConfig hw_cfg[2] = {0};
  unsigned long g1_dec_format = 0;
  unsigned long g2_dec_format = 0;

  DWLReadAsicConfig(&hw_cfg[0], DWL_CLIENT_TYPE_H264_DEC);
  DWLReadAsicConfig(&hw_cfg[1], DWL_CLIENT_TYPE_HEVC_DEC);

  g1_dec_format = (((hw_cfg[0].h264_support != 0) ? (1 << DEC_HAS_H264) : 0) |
                   ((hw_cfg[0].vp8_support != 0) ? (1 << DEC_HAS_VP8) : 0));

#ifdef HAS_FULL_DECFMT
  g1_dec_format |=
      (((hw_cfg[0].vc1_support == 3) ? (1 << DEC_HAS_VC1_G) : 0) |
       ((hw_cfg[0].vc1_support != 0) ? (1 << DEC_HAS_VC1_L) : 0) |
       ((hw_cfg[0].mpeg2_support != 0) ? (1 << DEC_HAS_MPEG2) : 0) |
       ((hw_cfg[0].mpeg4_support != 0) ? (1 << DEC_HAS_MPEG4) : 0) |
       ((hw_cfg[0].mpeg4_support != 0) ? (1 << DEC_HAS_H263) : 0) |
       ((hw_cfg[0].jpeg_support != 0) ? (1 << DEC_HAS_JPEG) : 0) |
       ((hw_cfg[0].avs_support != 0) ? (1 << DEC_HAS_AVS2) : 0) |
       ((hw_cfg[0].rv_support != 0) ? (1 << DEC_HAS_RV) : 0) |
       ((hw_cfg[0].mpeg4_support != 0) ? (1 << DEC_HAS_XVID) : 0));
#endif
  g2_dec_format = (((hw_cfg[1].hevc_support != 0) ? (1 << DEC_HAS_HEVC) : 0) |
                   ((hw_cfg[1].vp9_support != 0) ? (1 << DEC_HAS_VP9) : 0));

  if (g1_dec_format) core_num++;
  if (g2_dec_format) core_num++;

  HANTRO_LOG(HANTRO_LEVEL_INFO, "core_num %d, ReadAsicCoreCount %d\n", core_num,
             DWLReadAsicCoreCount());
  ASSERT(core_num == DWLReadAsicCoreCount());
  hwinfo->dec_corenum = core_num;
  hwinfo->decformat = g1_dec_format | g2_dec_format;
  //is there any better way to determine it?
  if (g1_dec_format & (1 << DEC_HAS_VC1_G))
    hwinfo->max_dec_resolution = 4096;
  else
    hwinfo->max_dec_resolution = 1920;
#endif
  return (core_num != 0) ? 0 : -1;
}

//#define LOCAL_SAVE_FILE
#ifdef LOCAL_SAVE_FILE
uint32_t _dbg_wr_mode = 1; /*0: per frame per file; 1: combined to single file*/
                           /**
                            * @brief _dbg_write_input_data_(), write input data packet to file.
                            * @param v4l2_dec_inst* h: decoder instance.
                            * @param uint32_t *vaddr: virtual address of input packet.
                            * @param uint32_t size: size of input packet buffer.
                            * @param uint32_t payload: bytes number of input packet payload.
                            * @return none.
                            */
void _dbg_write_input_data_(v4l2_dec_inst *h, uint32_t *vaddr, uint32_t size,
                            uint32_t payload) {
  FILE *fp = NULL;
  char name[64];
  char *p = NULL;

  uint32_t frame_id = 0;

  if (_dbg_wr_mode == 0) {
    frame_id = h->dec_in_pic_id;
  }

  switch (h->codec_fmt) {
    case V4L2_DAEMON_CODEC_DEC_HEVC:
      sprintf(name, "recived_%lx_%d.hevc", h->instance_id, frame_id);
      break;
    case V4L2_DAEMON_CODEC_DEC_H264:
      sprintf(name, "recived_%lx_%d.h264", h->instance_id, frame_id);
      break;
    default:
      sprintf(name, "recived_%lx_%d.bin", h->instance_id, frame_id);
      break;
  }

  if (_dbg_wr_mode == 0) {
    fp = fopen(name, "wb");
  } else {
    fp = fopen(name, "ab+");
  }

  if (fp) {
    p = malloc(size);
    ASSERT(p);
    memcpy(p, vaddr, size);
    fwrite(p, 1, payload, fp);
    free(p);
    fclose(fp);
  }
}
#endif

/**
 * @brief vsi_dec_register_fsm_table(), decoder register fsm table.
 * @param v4l2_dec_inst* h: decoder instance.
 * @return none.
 */
void vsi_dec_register_fsm_table(v4l2_dec_inst *h) {
  switch (h->codec_fmt) {
#ifdef USE_G1
    case V4L2_DAEMON_CODEC_DEC_VP8:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_vp8_v4l2m2m_decoder;
      break;
#ifdef HAS_FULL_DECFMT
    case V4L2_DAEMON_CODEC_DEC_MPEG2:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_mpeg2_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_H263:
    case V4L2_DAEMON_CODEC_DEC_MPEG4:
    case V4L2_DAEMON_CODEC_DEC_XVID:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_mpeg4_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_VC1_G:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_vc1g_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_VC1_L:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_vc1l_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_JPEG:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_jpeg_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_AVS2:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_avs_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_RV:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_rv_v4l2m2m_decoder;
      break;

#endif
    case V4L2_DAEMON_CODEC_DEC_H264:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_h264_v4l2m2m_decoder;
      break;

#endif

#ifdef USE_G2
    case V4L2_DAEMON_CODEC_DEC_HEVC:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_hevc_v4l2m2m_decoder;
      break;
    case V4L2_DAEMON_CODEC_DEC_VP9:
      h->vsi_v4l2m2m_decoder = (void *)&vsidaemon_vp9_v4l2m2m_decoder;
      break;
#endif
    case V4L2_DAEMON_CODEC_UNKNOW_TYPE:
      HANTRO_LOG(HANTRO_LEVEL_WARNING, "unknown codec fmt \n");
      break;
    default:
      break;
  }
}
/**
 * @brief vsi_dec_trans_v4l2cmdid_to_eventid(), deduce event ID from
 * v4l2_msg->cmd_id.
 * @param v4l2_dec_inst* h: decoder instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event, deduced event ID.
 */
v4l2_inst_dec_event vsi_dec_trans_v4l2cmdid_to_eventid(
    v4l2_dec_inst *h, struct vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event dec_event = DEC_EMPTY_EVENT;

  switch (v4l2_msg->cmd_id) {
    case V4L2_DAEMON_VIDIOC_BUF_RDY:
      dec_event = DEC_CACHE_IO_BUFFER_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_STREAMON_CAPTURE:
      dec_event = DEC_RECEIVE_CAPTUREON_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE:
      dec_event = DEC_RECEIVE_CAPTUREOFF_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_STREAMON_OUTPUT:
      dec_event = DEC_RECEIVE_OUTPUTON_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT:
      dec_event = DEC_RECEIVE_OUTPUTOFF_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_CMD_START:
      dec_event = DEC_RECEIVE_CMDSTART_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_CMD_STOP:
      dec_event = DEC_RECEIVE_CMDSTOP_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_DESTROY_DEC:
      dec_event = DEC_RECEIVE_DESTROY_EVENT;
      break;

    case V4L2_DAEMON_VIDIOC_FAKE:
      if (bufferlist_get_size(h->bufferlist_input)) {
        dec_event = DEC_FAKE_EVENT;
      }
      break;
    default:
      HANTRO_LOG(HANTRO_LEVEL_WARNING, "unkonw cmd_id %d\n", v4l2_msg->cmd_id);
      break;
  }
  return dec_event;
}


/**
 * @brief vsi_dec_msg_done(), send msg-done notification to driver
 * @param v4l2_dec_inst* h: decoder instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return
 */
void vsi_dec_msg_done(v4l2_dec_inst *h, struct vsi_v4l2_msg *v4l2_msg) {
  if (v4l2_msg->cmd_id == V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE) {
    send_cmd_orphan_msg(h->instance_id, V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE_DONE);
  }
}

#ifndef NXP_TIMESTAMP_MANAGER
static void vsi_dec_set_timestamp(v4l2_dec_inst *h, uint64_t ts_value,
                                  uint32_t index) {
  h->dec_timestamp_values[index % TIMESTAMP_SLOT_NUM] = ts_value;
}

static uint64_t vsi_dec_get_timestamp(v4l2_dec_inst *h, uint32_t index) {
  return h->dec_timestamp_values[index % TIMESTAMP_SLOT_NUM];
}
#endif

static v4l2_inst_dec_event vsi_dec_handle_input_buffer(
    v4l2_dec_inst *h, struct vsi_v4l2_msg *msg) {
  BUFFER *buf = NULL;
  int32_t ret = 0;

  if (msg->params.dec_params.io_buffer.inbufidx == INVALID_IOBUFF_IDX) {
    return DEC_EMPTY_EVENT;
  }

  buf = (BUFFER *)malloc(sizeof(BUFFER)); //TBD, avoid malloc/free each input.
  ASSERT(buf);
  buf->frame_display_id = ++h->input_frame_cnt;
  memcpy(&buf->dec_cmd, &msg->params.dec_params.io_buffer,
         sizeof(struct v4l2_daemon_dec_buffers));
  ret = bufferlist_push_buffer(h->bufferlist_input, buf);
  ASSERT(ret == 0);

#ifndef NXP_TIMESTAMP_MANAGER
  vsi_dec_set_timestamp(h, msg->params.dec_params.io_buffer.timestamp,
                        buf->frame_display_id);
#endif

  HANTRO_LOG(HANTRO_LEVEL_DEBUG,
             "Inst[%lx]: received one input buffer: id=%d, payloadSize=%d.\n",
             h->instance_id,
             msg->params.dec_params.io_buffer.inbufidx,
             msg->params.dec_params.io_buffer.bytesused);

  return DEC_BUFFER_EVENT;
}

static v4l2_inst_dec_event vsi_dec_handle_output_buffer(
    v4l2_dec_inst *h, struct vsi_v4l2_msg *msg) {
  v4l2_daemon_dec_buffers *buf = &msg->params.dec_params.io_buffer;
  int32_t index = buf->outbufidx;

  if (index == INVALID_IOBUFF_IDX) {
    return DEC_EMPTY_EVENT;
  }

  if (h->dec_cur_state == DAEMON_DEC_STATE_OPEN) {
    h->dec_info.frame_width = buf->output_width;
    h->dec_info.frame_height = buf->output_height;
  }

  dpb_receive_buffer(h, index, buf);

  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "Inst[%lx]: received one output buffer: id=%d.\n",
             h->instance_id,
             msg->params.dec_params.io_buffer.outbufidx);
  h->output_frame_cnt++;
  return DEC_BUFFER_EVENT;
}

static void vsi_dec_get_dec_ofmt(v4l2_dec_inst *h, struct vsi_v4l2_msg *msg) {
  if ((msg->params.dec_params.io_buffer.outbufidx == INVALID_IOBUFF_IDX) ||
      (h->dec_pic_id > 0)) {
    return;
  }

  int32_t pixel_depth = msg->params.dec_params.io_buffer.outputPixelDepth;

  // set output format & pixel format
  switch (msg->params.dec_params.io_buffer.outBufFormat) {
  case VSI_V4L2_DECOUT_DTRC:
  case VSI_V4L2_DECOUT_DTRC_10BIT:
  case VSI_V4L2_DECOUT_RFC:
  case VSI_V4L2_DECOUT_RFC_10BIT:
    h->dec_output_fmt = DEC_REF_FRM_TILED_DEFAULT;
    break;
  default:
    h->dec_output_fmt = DEC_REF_FRM_RASTER_SCAN;
    break;
  }

  if (pixel_depth == 8) {
    h->dec_pixel_fmt = DEC_OUT_PIXEL_CUT_8BIT;
  } else if (pixel_depth == 10) {
    h->dec_pixel_fmt = DEC_OUT_PIXEL_DEFAULT;
  } else if (pixel_depth == 16) {
    h->dec_pixel_fmt = DEC_OUT_PIXEL_P010;
  }
  return;
}

v4l2_inst_dec_event vsi_dec_handle_io_buffer(v4l2_dec_inst *h,
                                             struct vsi_v4l2_msg *msg) {
  v4l2_inst_dec_event event_1, event_2;
  vsi_dec_get_dec_ofmt(h, msg);
  event_1 = vsi_dec_handle_input_buffer(h, msg);
  event_2 = vsi_dec_handle_output_buffer(h, msg);

  if (event_1 == DEC_EMPTY_EVENT && event_2 == DEC_EMPTY_EVENT) {
    return DEC_EMPTY_EVENT;
  }

  return DEC_BUFFER_EVENT;
}

void vsi_dec_flush_input_packet(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  BUFFER *p = NULL;

  p = bufferlist_get_buffer(h->bufferlist_input);

  if (p == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: SHOULD GET THE INPUT BUFFER.\n",
               h->instance_id);
    ASSERT(0);
  }

#ifdef DEC_INPUT_NO_COPY
  if (h->dec_cur_input_vaddr) {
    unmap_phy_addr_daemon(h->dec_cur_input_vaddr, h->stream_in_mem.size);
    h->stream_in_mem.virtual_address = NULL;
    h->dec_cur_input_vaddr = NULL;
  }
#endif

  send_dec_inputbuf_orphan_msg(h->handler, v4l2_msg,
                               p->dec_cmd.io_buffer.inbufidx, h->dec_inpkt_flag);

  free(p);
  bufferlist_pop_buffer(h->bufferlist_input);
}

int32_t vsi_dec_get_input_data(v4l2_dec_inst *h) {
  BUFFER *p = NULL;
  v4l2_daemon_dec_buffers *in_buf = NULL;

  if (h->stream_in_mem.logical_size) {
    return 0;
  }
re_get:
  p = bufferlist_get_buffer(h->bufferlist_input);

  if (p == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: should get one input buffer.\n",
              h->instance_id);
    return -2;
  }

  // CHECK: this align for mmap maybe cause overlap
  in_buf = &p->dec_cmd.io_buffer;

  if (in_buf->bytesused == 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: input payload size is zero!\n",
              h->instance_id);
    vsi_dec_flush_input_packet(h, NULL);
    goto re_get;
  }

  uint32_t map_size = h->secure_mode_on ? (in_buf->bytesused + SECURE_MODE_SKIP_LEN) : in_buf->bytesused;

  uint32_t aligned_map_size = NEXT_MULTIPLE(map_size, 0x1000);

  uint32_t *virtual_address =
      mmap_phy_addr_daemon(mmap_fd, in_buf->busInBuf, aligned_map_size);
  if (virtual_address == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: MMAP Input Buffer FAILED\n",
               h->instance_id);
    ASSERT(0);
    return -1;
  }

#ifdef DEC_INPUT_NO_COPY
  h->stream_in_mem.virtual_address = virtual_address;
  h->stream_in_mem.bus_address = in_buf->busInBuf;
  h->stream_in_mem.size = aligned_map_size;  // in_buf->inBufSize; //Using
                                             // aligned_map_size instead of
                                             // inBufSize is for umap.
  h->stream_in_mem.logical_size = in_buf->bytesused;
  h->stream_in_mem.mem_type = DWL_MEM_TYPE_SLICE;

  h->dec_cur_input_vaddr = virtual_address;
  h->dec_inpkt_flag = 0;

  if (h->secure_mode_on) {
    //The first 16 bytes store bus address of the secure packet data.
    h->stream_in_mem.virtual_address += SECURE_MODE_SKIP_LEN/sizeof(uint32_t);
    h->stream_in_mem.bus_address = *((addr_t *)virtual_address);
  }

#ifdef LOCAL_SAVE_FILE
  _dbg_write_input_data_(h, virtual_address, aligned_map_size,
                         in_buf->bytesused);
#endif

#else //DEC_INPUT_NO_COPY
  h->stream_in_mem.logical_size = in_buf->bytesused;
  ASSERT(h->dec_cur_input_vaddr);
  h->stream_in_mem.virtual_address = h->dec_cur_input_vaddr;

  memcpy(h->stream_in_mem.virtual_address, virtual_address,
         aligned_map_size);  // in_buf->bytesused);
  unmap_phy_addr_daemon(virtual_address, aligned_map_size);
#endif //DEC_INPUT_NO_COPY

#ifdef VSI_CMODEL
  h->stream_in_mem.bus_address = (addr_t)h->stream_in_mem.virtual_address;
#endif

  h->curr_inbuf_idx = in_buf->inbufidx;

#ifdef NXP_TIMESTAMP_MANAGER
  if (in_buf->timestamp < 0) in_buf->timestamp = TSM_TIMESTAMP_NONE;

  if (h->need_resync_tsm) {
    resyncTSManager(h->tsm, in_buf->timestamp, MODE_AI);
    h->need_resync_tsm = 0;
  }
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: input time stamp: %ld len: %d\n",
             h->instance_id,
             in_buf->timestamp, in_buf->bytesused);
  TSManagerReceive2(h->tsm, in_buf->timestamp, in_buf->bytesused);
#endif
  h->dec_in_pic_id = p->frame_display_id;
  h->dec_inpkt_pic_decoded_cnt = h->dec_pic_id;
  h->dec_in_new_packet = 1;
  h->dec_inpkt_ignore_picconsumed_event = (in_buf->timestamp < 0) ? 1 : 0;
  return 0;
}

void vsi_dec_send_picconsumed_info(v4l2_dec_inst *h, i32 in_idx, i32 out_idx) {
    vsi_v4l2_msg msg = {0};

    msg.error = 0;
    msg.seq_id = NO_RESPONSE_SEQID;
    msg.size = sizeof(struct v4l2_daemon_dec_buffers);
    msg.cmd_id = V4L2_DAEMON_VIDIOC_PICCONSUMED;
    msg.inst_id = h->instance_id;
    msg.params.dec_params.io_buffer.inbufidx = in_idx;
    msg.params.dec_params.io_buffer.outbufidx = out_idx;
    send_notif_to_v4l2(pipe_fd, &msg,
                        sizeof(struct vsi_v4l2_msg_hdr) +
                            sizeof(struct v4l2_daemon_dec_buffers));
}


static void vsi_dec_handle_inpkt_cnt(v4l2_dec_inst *h) {
  /* If current input packet doesn't have picture decoded,
   * send out a specific event out. It is for Android CTS,
   * which requires input <-> output should match.
  */
  if (h->dec_inpkt_pic_decoded_cnt == h->dec_pic_id &&
      h->dec_inpkt_ignore_picconsumed_event == 0) {
    BUFFER *p = NULL;

    p = bufferlist_get_buffer(h->bufferlist_input);
    if (p == NULL) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: SHOULD GET INPUT BUFFER.\n",
                    h->instance_id);
        ASSERT(0);
    } else {
      vsi_dec_send_picconsumed_info(h, p->dec_cmd.io_buffer.inbufidx, INVALID_IOBUFF_IDX);
      h->dec_inpkt_flag |= ERROR_BUFFER_FLAG;
    }
  }

  if (h->discard_dbp_count > 0) {
    i32 i;

    for (i = 0; i < h->discard_dbp_count; i++) {
      vsi_dec_send_picconsumed_info(h, INVALID_IOBUFF_IDX, INVALID_IOBUFF_IDX);
#ifdef NXP_TIMESTAMP_MANAGER
      TSManagerSend2(h->tsm, NULL);
#endif
    }
    h->discard_dbp_count = 0;
  }
}

void vsi_dec_update_input_data(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  h->dec_in_new_packet = 0;

  if (h->stream_in_mem.logical_size) return;
  vsi_dec_handle_inpkt_cnt(h);
  vsi_dec_flush_input_packet(h, v4l2_msg);
}

/**
 * @brief vsi_dec_flush_input(), decoder gives back OUTPUT queue buffer to
 * v4l2-driver.
 * @param v4l2_dec_inst* h: decoder instance.
 * @return none.
 */
void vsi_dec_flush_input(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  uint32_t avails = bufferlist_get_size(h->bufferlist_input);

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: flush input avails: %d\n",
             h->instance_id, avails);
  for (int i = 0; i < avails; i++) {
    vsi_dec_flush_input_packet(h, v4l2_msg);
  }
  h->stream_in_mem.logical_size = 0;
#ifdef NXP_TIMESTAMP_MANAGER
  h->need_resync_tsm = 1;
  h->consumed_len = 0;
#endif
}

/**
 * @brief vsi_dec_flush_output(), decoder gives back CAPTURE queue buffer to
 * v4l2-driver.
 * @param v4l2_dec_inst* h: decoder instance.
 * @return none.
 */
void vsi_dec_flush_output(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  struct v4l2_decoder_dbp *p;

  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    p = dpb_list_buffer(h, i);
    if (p && p->status != DPB_STATUS_RENDER) {
      ;
      // send_dec_outputbuf_orphan_msg(h->handler, v4l2_msg, p->buff_idx, 0,
      // VSI_DEC_EMPTY_OUTPUT_DATA);
    }
  }
  dpb_destroy(h);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: currently existed dpb nums %d\n",
             h->instance_id,
             h->existed_dpb_nums);
}

/**
* @brief vsi_dec_get_pic(), try to get output picture, then send to driver.
* @param v4l2_dec_inst* h: decoder instance.
* @return int: 0- get & send one picture succeed; others- not pic got.
*/
int vsi_dec_dequeue_pic(v4l2_dec_inst *h) {
  int32_t id = INVALID_IOBUFF_IDX;
  int ret = 0;
  vsi_v4l2m2m2_deccodec *dec = (vsi_v4l2m2m2_deccodec *)h->vsi_v4l2m2m_decoder;
  vsi_v4l2_dec_picture tmp_pic = {0};

  // get picture from decoder
  ASSERT(dec);
  ret = dec->get_pic(h, &tmp_pic, h->dec_capture_eos);

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: get pic ret %d\n",
             h->instance_id, ret);
  if (ret && !h->dec_capture_eos) {
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Inst[%lx]: didn't have available picture for dequeue.\n",
               h->instance_id);
    return DEQUEUE_ERR_PIC_NOT_AVAIL;
  }

  // dpb-buff management - render
  if (ret != 0) {
    id = dpb_render_buffer(h, NULL);
    if (h->dec_drain_tid == 0) {
      pthread_join(h->dec_drain_thread, NULL);
      h->dec_drain_tid = -1;
      HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: joined drain thread.\n",
                 h->instance_id);
    }
  } else {
    if (vsi_dec_check_crop(h, &tmp_pic)) {
      vsi_dec_handle_cropchange(h);
    }
    id = dpb_render_buffer(h, &tmp_pic);
  }

  if (id == INVALID_IOBUFF_IDX) {
    return DEQUEUE_ERR_BUF_NOT_AVAIL;
  }

  // notify kernel for dequeue
  vsi_v4l2_msg msg = {0};
  msg.error = 0;
  msg.seq_id = NO_RESPONSE_SEQID;
  msg.size = sizeof(v4l2_daemon_dec_buffers);
  msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
  msg.inst_id = h->instance_id;
  msg.params.dec_params.io_buffer.inbufidx = INVALID_IOBUFF_IDX;
  msg.params.dec_params.io_buffer.outbufidx = h->dpb_buffers[id].buff_idx;
  msg.params.dec_params.io_buffer.OutBufSize = h->dpb_buffers[id].buff.size;
  h->dec_out_pic_id++;

  if (0 == ret) {
    uint32_t luma_size = tmp_pic.pic_stride * tmp_pic.pic_height;
    uint32_t chroma_size = 0;
    if (tmp_pic.output_picture_chroma_bus_address) {
      if (!tmp_pic.chroma_size) {
        chroma_size = tmp_pic.pic_stride * tmp_pic.pic_height / 2;
      } else {
        chroma_size = tmp_pic.chroma_size;
      }
    }
    msg.params.dec_params.io_buffer.rfc_luma_offset =
        tmp_pic.output_rfc_luma_bus_address - tmp_pic.output_picture_bus_address;
    msg.params.dec_params.io_buffer.rfc_chroma_offset =
        tmp_pic.output_rfc_chroma_bus_address - tmp_pic.output_picture_bus_address;	
    msg.params.dec_params.io_buffer.bytesused = luma_size + chroma_size;

#ifdef NXP_TIMESTAMP_MANAGER
    msg.params.dec_params.io_buffer.timestamp = TSManagerSend2(h->tsm, NULL);
#else
    msg.params.dec_params.io_buffer.timestamp =
        vsi_dec_get_timestamp(h, tmp_pic.pic_id);
#endif
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Inst[%lx]: output time stamp: %ld buffer[id: %d}: %d\n",
               h->instance_id,
               msg.params.dec_params.io_buffer.timestamp,
               id, h->dpb_buffers[id].buff_idx);
  } else {
    msg.params.dec_params.io_buffer.bytesused = 0;
    h->dec_last_pic_sent = 1;
    h->dec_capture_eos = 0;
    msg.params.dec_params.io_buffer.timestamp = 0;
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: last pic is sent out.\n",
              h->instance_id);
  }
  send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr) +
                                        sizeof(struct v4l2_daemon_dec_buffers));
  capture_tot_size += msg.params.dec_params.io_buffer.bytesused;
  return 0;
}

/**
* @brief vsi_dec_dequeue_empty_pic(), send capture buffer (with byteused=0) to
* driver, for last picture.
* @param v4l2_dec_inst* h: decoder instance.
* @return int: 0- succeed; others- no "available" capture buffer got.
*/
int vsi_dec_dequeue_empty_pic(v4l2_dec_inst *h) {
  int32_t id = INVALID_IOBUFF_IDX;
  vsi_v4l2_msg msg = {0};

  // dpb-buff management - render
  id = dpb_render_buffer(h, NULL);
  if (id == INVALID_IOBUFF_IDX) {
    return DEQUEUE_ERR_BUF_NOT_AVAIL;
  }

  // notify kernel for dequeue
  msg.error = 0;
  msg.seq_id = NO_RESPONSE_SEQID;
  msg.size = sizeof(v4l2_daemon_dec_buffers);
  msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
  msg.inst_id = h->instance_id;
  msg.params.dec_params.io_buffer.inbufidx = INVALID_IOBUFF_IDX;
  msg.params.dec_params.io_buffer.outbufidx = h->dpb_buffers[id].buff_idx;
  msg.params.dec_params.io_buffer.OutBufSize = h->dpb_buffers[id].buff.size;
  msg.params.dec_params.io_buffer.bytesused = 0;
  h->dec_out_pic_id++;

  send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr) +
                                        sizeof(struct v4l2_daemon_dec_buffers));
  return 0;
}

/**
* @brief vsi_dec_drop_pic(), try to get output buffer idx, then drop it
* directly.
* @param v4l2_dec_inst* h: decoder instance.
* @return none.
*/
int vsi_dec_drop_pic(v4l2_dec_inst *h) {
  int ret = 0;
  vsi_v4l2m2m2_deccodec *dec = (vsi_v4l2m2m2_deccodec *)h->vsi_v4l2m2m_decoder;
  vsi_v4l2_dec_picture tmp_pic;

  // get picture from decoder
  ASSERT(dec);
  ret = dec->get_pic(h, &tmp_pic, h->dec_capture_eos);
  if (ret) {
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Inst[%lx]: didn't have available picture for dequeue.\n",
               h->instance_id);
    return -1;
  }

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: release picture: bus_addr=0x%llx\n",
             h->instance_id,
             tmp_pic.output_picture_bus_address);
  dec->release_pic(h, tmp_pic.priv_pic_data);
  return 0;
}

void *vsi_dec_send_eos(void *arg) {
  v4l2_dec_inst *h = (v4l2_dec_inst *)arg;
  ASSERT(h->vsi_v4l2m2m_decoder);
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
  int ret = 0;
  ret = dec->drain(h);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: END, ret %d\n", h->instance_id, ret);
  return NULL;
}

v4l2_inst_dec_event vsi_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re = DEC_EMPTY_EVENT;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
  ASSERT(dec);
#ifdef NXP_TIMESTAMP_MANAGER
  h->tsm = createTSManager(VSI_TS_BUFFER_LENGTH_DEFAULT);
  h->need_resync_tsm = 1;
  h->consumed_len = 0;
#endif

#ifdef VSI_CMODEL
  TBSetDefaultCfg(&tb_cfg);
#endif
  re = dec->init(h, v4l2_msg);
  h->priv_pic_data = calloc(1, dec->pic_ctx_size);
  ASSERT(h->priv_pic_data);

#ifndef DEC_INPUT_NO_COPY
  h->stream_in_mem.mem_type = DWL_MEM_TYPE_SLICE;
  DWLMallocLinear(h->dwl_inst, STREAM_INBUFF_SIZE, &h->stream_in_mem);
  h->dec_cur_input_vaddr = h->stream_in_mem.virtual_address;
  ASSERT(h->dec_cur_input_vaddr);
#endif
  h->stream_in_mem.logical_size = 0;
  return re;
}

v4l2_inst_dec_event vsi_dec_destroy(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
  ASSERT(dec);

  dpb_refresh_all_buffers(h);
  if (h->dec_drain_tid == 0) {
    pthread_join(h->dec_drain_thread, NULL);
    h->dec_drain_tid = -1;
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: joined drain thread.\n",
              h->instance_id);
  }

#ifndef DEC_INPUT_NO_COPY
  h->stream_in_mem.virtual_address = h->dec_cur_input_vaddr;
  DWLFreeLinear(h->dwl_inst, &h->stream_in_mem);
#endif
  memset(&h->stream_in_mem, 0, sizeof(struct DWLLinearMem));
  if (h->priv_pic_data) {
    free(h->priv_pic_data);
    h->priv_pic_data = NULL;
  }
  re = dec->destroy(h, v4l2_msg);

  h->vsi_v4l2m2m_decoder = NULL;
#ifdef NXP_TIMESTAMP_MANAGER
  if (h->tsm) {
    destroyTSManager(h->tsm);
    h->tsm = NULL;
  }
#endif
  return re;
}

/**
 * @brief vsi_dec_find_event_index_in_fsm_table(), decoder search status .
 * @param v4l2_dec_inst* h: decoder instance.
 * @param v4l2_inst_dec_event dec_receive_event: message from v4l2 driver.
 * @return int, status index in fsm table.
 */
int vsi_dec_find_event_index_in_fsm_table(
    v4l2_dec_inst *h, v4l2_inst_dec_event dec_receive_event) {
  int status_index = INVALIDE_DEC_TABLE_INDEX;
  int i = 0;
  for (i = 0; i < vsi_fsm_trans_table_count; i++) {
    if (dec_receive_event == vsi_v4l2dec_fsm_trans_table[i].dec_event &&
        h->dec_cur_state == vsi_v4l2dec_fsm_trans_table[i].cur_state) {
      status_index = i;
      break;
    }
  }

  return status_index;
}

void dec_capture_act_proc(v4l2_dec_inst *h) {
  uint32_t act_done = 1;

  if (h->dec_capture_act == DAEMON_QUEUE_ACT_DROP) {
    while (0 == vsi_dec_drop_pic(h))
      ;
  } else if (h->dec_capture_act == DAEMON_QUEUE_ACT_FLUSH) {
    while (0 == vsi_dec_dequeue_pic(h))
      ;
    if (vsi_dec_dequeue_empty_pic(h)) {
      act_done = 0;
    }
  } else if (h->dec_capture_act == DAEMON_QUEUE_ACT_CONSUME) {
    dpb_refresh_all_buffers(h);
  }

  if (act_done) h->dec_capture_act = DAEMON_QUEUE_ACT_NONE;
}

void dec_output_act_proc(v4l2_dec_inst *h) {
  BUFFER *ibuf = NULL;

  if (h->dec_output_act == DAEMON_QUEUE_ACT_EOS) {
    // ASSERT(h->dec_output_state == DAEMON_QUEUE_STATE_PEND_OFF);

    h->dec_output_last_inbuf_idx = INVALID_IOBUFF_IDX;
    ibuf = bufferlist_get_tail(h->bufferlist_input);
    if (ibuf) {
      h->dec_output_last_inbuf_idx = ibuf->dec_cmd.io_buffer.inbufidx;
    }
    if (h->dec_output_last_inbuf_idx == INVALID_IOBUFF_IDX) {
      // h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
      h->dec_eos = 1;
    }
  }
  h->dec_output_act = DAEMON_QUEUE_ACT_NONE;
}

static uint32_t vsi_dec_having_input_data(v4l2_dec_inst *h) {
  if (h->stream_in_mem.logical_size ||
      bufferlist_get_size(h->bufferlist_input)) {
    return 1;
  }
  return 0;
}

v4l2_inst_dec_event vsi_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
  v4l2_inst_dec_event event = DEC_EMPTY_EVENT;
  int32_t ret;

  ASSERT(dec);

  if (h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP) {
    if (h->dec_wait_dbp_buf) return DEC_EMPTY_EVENT;
  }

  if (h->dec_output_state != DAEMON_QUEUE_STATE_PEND_OFF &&
      h->dec_output_state != DAEMON_QUEUE_STATE_ON) {
    return DEC_EMPTY_EVENT;
  }

  ret = vsi_dec_get_input_data(h);

  if (0 == ret) {
    h->dec_eos = 0;
    if (h->dec_output_state == DAEMON_QUEUE_STATE_PEND_OFF) {
      h->dec_eos = (h->curr_inbuf_idx == h->dec_output_last_inbuf_idx);
    }

    // Make sure dpb buffers are enough to avoid ctrlsw seg-fault
    if (h->dec_info.needed_dpb_nums &&
        h->existed_dpb_nums < h->dec_info.needed_dpb_nums) {
      return DEC_NO_DECODING_BUFFER_EVENT;
    }

    event = dec->decode(h, v4l2_msg);
    vsi_dec_update_input_data(h, v4l2_msg);
  }

#ifdef NXP_TIMESTAMP_MANAGER
  HANTRO_LOG(HANTRO_LEVEL_INFO,
             "event: %d consumed len: %d left stream len: %d\n", event,
             h->consumed_len, h->stream_in_mem.logical_size);
  if (event == DEC_PIC_DECODED_EVENT) {
    TSManagerValid2(h->tsm, h->consumed_len, NULL);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "consumed len: %d\n", h->consumed_len);
    h->consumed_len = 0;
  } else if (event == DEC_DECODING_ERROR_EVENT &&
             h->stream_in_mem.logical_size == 0) {
    TSManagerValid2(h->tsm, h->consumed_len, NULL);
    TSManagerSend2(h->tsm, NULL);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "broken pic consumed len: %d\n",
               h->consumed_len);
    h->consumed_len = 0;
  }
#endif

  if (h->dec_output_state == DAEMON_QUEUE_STATE_PEND_OFF) {
    if (h->dec_eos && h->stream_in_mem.logical_size == 0) {
      h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
      event = DEC_GOT_EOS_MARK_EVENT;
    }
  }

  if ((event == DEC_EMPTY_EVENT || event == DEC_DECODING_ERROR_EVENT) &&
      vsi_dec_having_input_data(h)) {
    send_fake_cmd(h->handler);
  }

  return event;
}

static void vsi_dec_set_crop(v4l2_dec_inst *h) {
  h->pic_info.crop_left = h->dec_info.visible_rect.left;
  h->pic_info.crop_top = h->dec_info.visible_rect.top;
  h->pic_info.crop_width = h->dec_info.visible_rect.width;
  h->pic_info.crop_height = h->dec_info.visible_rect.height;
  h->pic_info.width = h->dec_info.frame_width;
  h->pic_info.height = h->dec_info.frame_height;
  h->pic_info.pic_wstride = h->dec_info.pic_wstride;

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: crop info is updated.\n",
             h->instance_id);
}

static int vsi_dec_check_crop(v4l2_dec_inst *h, vsi_v4l2_dec_picture *new_pic) {
  if (new_pic->crop_left == h->pic_info.crop_left &&
      new_pic->crop_top == h->pic_info.crop_top &&
      new_pic->crop_width == h->pic_info.crop_width &&
      new_pic->crop_height == h->pic_info.crop_height &&
      new_pic->pic_width == h->pic_info.width &&
      new_pic->pic_height == h->pic_info.height &&
      new_pic->pic_stride == h->pic_info.pic_wstride) {
    // crop is not changed
    return 0;
  };

  h->pic_info.crop_left = new_pic->crop_left;
  h->pic_info.crop_top = new_pic->crop_top;
  h->pic_info.crop_width = new_pic->crop_width;
  h->pic_info.crop_height = new_pic->crop_height;
  h->pic_info.width = new_pic->pic_width;
  h->pic_info.height = new_pic->pic_height;
  h->pic_info.pic_wstride = new_pic->pic_stride;

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: crop info is different @ frame %d.\n",
             h->instance_id, h->dec_pic_id);
  return 1;
}

static v4l2_inst_dec_event vsi_dec_handle_cropchange(v4l2_dec_inst *h) {
  vsi_v4l2_msg msg = {0};
  struct v4l2_daemon_dec_pictureinfo_params *pic_info =
      &msg.params.dec_params.pic_info;
  msg.error = 0;
  msg.seq_id = NO_RESPONSE_SEQID;
  // use same structure with res-change params
  msg.size = sizeof(struct v4l2_daemon_dec_pictureinfo_params);
  msg.cmd_id = V4L2_DAEMON_VIDIOC_CROPCHANGE;
  msg.inst_id = h->instance_id;
  /*
      pic_info->io_buffer.inbufidx = INVALID_IOBUFF_IDX;
      pic_info->io_buffer.outbufidx = INVALID_IOBUFF_IDX;
      pic_info->io_buffer.srcwidth = h->dec_info.frame_width;
      pic_info->io_buffer.srcheight = h->dec_info.frame_height;
      pic_info->io_buffer.output_width = h->dec_info.frame_width;
      pic_info->io_buffer.output_height = h->dec_info.frame_height;
      pic_info->io_buffer.OutBufSize = h->dec_info.dpb_buffer_size;
  */
  memcpy(&pic_info->pic_info, &h->pic_info, sizeof(v4l2_daemon_pic_info));
  send_notif_to_v4l2(pipe_fd, &msg,
                     sizeof(struct vsi_v4l2_msg_hdr) +
                         sizeof(struct v4l2_daemon_dec_pictureinfo_params));

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: crop change event is sent.\n",
             h->instance_id);
  return DEC_EMPTY_EVENT;
}

static v4l2_inst_dec_event vsi_dec_handle_reschange(v4l2_dec_inst *h) {
  vsi_v4l2_msg msg = {0};
  msg.error = 0;
  msg.seq_id = NO_RESPONSE_SEQID;
  msg.size = sizeof(struct v4l2_daemon_dec_resochange_params);
  msg.cmd_id = V4L2_DAEMON_VIDIOC_CHANGE_RES;
  msg.inst_id = h->instance_id;
  msg.params.dec_params.dec_info.io_buffer.inbufidx = INVALID_IOBUFF_IDX;
  msg.params.dec_params.dec_info.io_buffer.outbufidx = INVALID_IOBUFF_IDX;
  msg.params.dec_params.dec_info.io_buffer.srcwidth = h->dec_info.frame_width;
  msg.params.dec_params.dec_info.io_buffer.srcheight = h->dec_info.frame_height;
  msg.params.dec_params.dec_info.io_buffer.output_width =
      h->dec_info.frame_width;
  msg.params.dec_params.dec_info.io_buffer.output_height =
      h->dec_info.frame_height;
  msg.params.dec_params.dec_info.io_buffer.OutBufSize =
      h->dec_info.dpb_buffer_size;
  msg.params.dec_params.dec_info.io_buffer.output_wstride =
      h->dec_info.pic_wstride;
  memcpy(&msg.params.dec_params.dec_info.dec_info, &h->dec_info,
         sizeof(v4l2_daemon_dec_info));
  send_notif_to_v4l2(pipe_fd, &msg,
                     sizeof(struct vsi_v4l2_msg_hdr) +
                         sizeof(struct v4l2_daemon_dec_resochange_params));

  vsi_dec_set_crop(h);
  return DEC_EMPTY_EVENT;
}

v4l2_inst_dec_event vsi_dec_streaming_output(v4l2_dec_inst *h,
                                             struct vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event dec_event = DEC_EMPTY_EVENT;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  dec_output_act_proc(h);

  dec_event = vsi_dec_decode(h, v4l2_msg);

  switch (dec_event) {
    case DEC_SOURCE_CHANGE_EVENT:
      vsi_dec_handle_reschange(h);
      break;

    case DEC_WAIT_DECODING_BUFFER_EVENT:
      h->dec_wait_dbp_buf = 1;
      break;

    case DEC_GOT_EOS_MARK_EVENT:
      h->dec_capture_eos = 1;
      ASSERT(h->dec_drain_tid == -1);
      if (dec->attributes & DEC_ATTR_EOS_THREAD)
        h->dec_drain_tid = pthread_create(
            &h->dec_drain_thread, &h->handler->attr, &vsi_dec_send_eos, h);
      else
        vsi_dec_send_eos(h);

      HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: DecEndOfStream\n",
                 h->instance_id);
      break;

    case DEC_PENDING_FLUSH_EVENT:
    case DEC_PIC_DECODED_EVENT:
      break;

    case DEC_FATAL_ERROR_EVENT:
      break;

    case DEC_DECODING_ERROR_EVENT:
    case DEC_NO_DECODING_BUFFER_EVENT:
    default:
      dec_event = DEC_EMPTY_EVENT;
      break;
  }
  return dec_event;
}

v4l2_inst_dec_event vsi_dec_streaming_capture(v4l2_dec_inst *h,
                                              struct vsi_v4l2_msg *v4l2_msg) {
  int ret = 0;
  v4l2_inst_dec_event dec_event = DEC_EMPTY_EVENT;

  dec_capture_act_proc(h);
  if (h->dec_capture_act != DAEMON_QUEUE_ACT_NONE) {
    return dec_event;
  }

  if (h->dec_cur_state == DAEMON_DEC_STATE_STOPPED) {
    ASSERT(h->dec_capture_state != DAEMON_QUEUE_STATE_ON);
    return dec_event;
  }

  if (h->dec_capture_state != DAEMON_QUEUE_STATE_ON) {
    if (h->dec_capture_eos) {
      if (h->existed_dpb_nums == 0) {
        h->dec_capture_eos = 0;
        h->dec_fatal_error = DAEMON_ERR_DEC_METADATA_ONLY;
        dec_event = DEC_FATAL_ERROR_EVENT;
      }
    }
    return dec_event;
  }

  ret = vsi_dec_dequeue_pic(h);
  if (h->dec_eos && h->dec_capture_eos) {
    if (ret == DEQUEUE_ERR_BUF_NOT_AVAIL) {
      return dec_event;
    }

    dec_event = DEC_GOT_EOS_MARK_EVENT;
/*
    if (h->existed_dpb_nums == 0) {
      h->dec_fatal_error = DAEMON_ERR_DEC_METADATA_ONLY;
      dec_event = DEC_FATAL_ERROR_EVENT;
    }
*/
  }

  if (ret) {
    if (h->dec_output_state == DAEMON_QUEUE_STATE_PAUSE)
      dec_event = DEC_PENDING_FLUSH_EVENT;
    if (h->dec_cur_state == DAEMON_DEC_STATE_SOURCE_CHANGE)
      dec_event = DEC_LAST_PIC_EVENT;
  }

  if (h->dec_last_pic_sent) {
    dec_event = DEC_LAST_PIC_EVENT;
    h->dec_last_pic_sent = 0;
  }
  return dec_event;
}

/**
 * @brief vsi_dec_fsm_event_handle(), decoder message processor.
 * @param v4l2_dec_inst* h: decoder instance.
 * @param struct vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return int32_t.
 */
int32_t vsi_dec_fsm_event_handle(void *codec, struct vsi_v4l2_msg *v4l2_msg) {
  v4l2_dec_inst *h = (v4l2_dec_inst *)codec;
  v4l2_inst_dec_event dec_event = DEC_EMPTY_EVENT;

  dec_event = vsi_dec_trans_v4l2cmdid_to_eventid(h, v4l2_msg);
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "Inst[%lx]: receive event [%s]\n",
             h->instance_id,
             dec_event_name[dec_event]);
  if (DEC_CACHE_IO_BUFFER_EVENT == dec_event) {
    dec_event = vsi_dec_handle_io_buffer(h, v4l2_msg);
  }

  while (DEC_EMPTY_EVENT != dec_event) {
    int status_index = vsi_dec_find_event_index_in_fsm_table(h, dec_event);

    if (INVALIDE_DEC_TABLE_INDEX != status_index) {
      HANTRO_LOG(
          HANTRO_LEVEL_DEBUG, "Inst[%lx]: event [%s], state [%s] -> [%s]\n",
          h->instance_id,
          dec_event_name[dec_event], dec_state_name[h->dec_cur_state],
          dec_state_name[vsi_v4l2dec_fsm_trans_table[status_index].next_state]);

      if (vsi_v4l2dec_fsm_trans_table[status_index].eventActFun) {
        int ret =
            vsi_v4l2dec_fsm_trans_table[status_index].eventActFun(h, v4l2_msg);
        if (0 == ret) {
          h->dec_cur_state =
              vsi_v4l2dec_fsm_trans_table[status_index].next_state;
        }
      } else {
        if (h->dec_fatal_error) {
          dec_event = DEC_FATAL_ERROR_EVENT;
          continue;
        }
        HANTRO_LOG(HANTRO_LEVEL_WARNING,
                   "Didn't find event function , shouldn't go here\n");
      }
    }

    if (h->dec_cur_state == DAEMON_DEC_STATE_DESTROYED) break;

    dec_event = vsi_dec_streaming_capture(h, v4l2_msg);
    if (dec_event != DEC_EMPTY_EVENT) continue;

    dec_event = vsi_dec_streaming_output(h, v4l2_msg);
  }

  vsi_dec_msg_done(h, v4l2_msg);
  HANTRO_LOG(HANTRO_LEVEL_DEBUG,
             "Inst[%lx]: finish one external event, curr state [%s] \n",
             h->instance_id,
             dec_state_name[h->dec_cur_state]);
  return (h->dec_cur_state == DAEMON_DEC_STATE_DESTROYED);
}

/**
 * @brief vsi_destroy_decoder(), destroy decoder inst.
 * @param void* codec: decoder instance.
 * @return int32_t: 0: succeed; Others: failure.
 */
static int32_t vsi_destroy_decoder(void *codec) {
  v4l2_dec_inst *h = (v4l2_dec_inst *)codec;

  if (h == NULL) {
    return -1;
  }

  if (h->bufferlist_input) {
    bufferlist_destroy(h->bufferlist_input);
    free(h->bufferlist_input);
    h->bufferlist_input = NULL;
  }

  dpb_destroy(h);

  return 0;
}

/**
 * @brief vsi_dec_in_change(), check if decoder is in source change state.
 * @param void* codec: decoder instance.
 * @return int32_t: 0: NOT; 1: YES.
 */
static int32_t vsi_dec_in_change(void *codec) {
  v4l2_dec_inst *h = (v4l2_dec_inst *)codec;

  if (h == NULL) {
    return -1;
  }

  return (h->dec_cur_state == DAEMON_DEC_STATE_SOURCE_CHANGE);
}

/**
 * @brief vsi_create_decoder(), create decoder inst.
 * @param v4l2_daemon_inst* daemon_inst: decoder instance.
 * @return void *: decoder instance.
 */
void *vsi_create_decoder(v4l2_daemon_inst *daemon_inst) {
  v4l2_dec_inst *h = NULL;

  ASSERT(daemon_inst->codec_mode == DAEMON_MODE_DECODER);

  h = (v4l2_dec_inst *)malloc(sizeof(v4l2_dec_inst));
  ASSERT(h);
  memset(h, 0, sizeof(v4l2_dec_inst));

  h->handler = daemon_inst;
  h->codec_fmt = daemon_inst->codec_fmt;
  h->instance_id = daemon_inst->instance_id;

  h->dec_cur_state = DAEMON_DEC_STATE_OPEN;
  h->curr_inbuf_idx = INVALID_IOBUFF_IDX;
  h->dec_drain_tid = -1;
  h->dec_output_fmt = DEC_REF_FRM_RASTER_SCAN;  // DEC_REF_FRM_TILED_DEFAULT;
  h->dec_pixel_fmt = DEC_OUT_PIXEL_DEFAULT;

  dpb_init(h);
  h->bufferlist_input = (BUFFERLIST *)malloc(sizeof(BUFFERLIST));
  ASSERT(h->bufferlist_input != NULL);
  bufferlist_init(h->bufferlist_input, MAX_BUFFER_SIZE);

  daemon_inst->func.proc = vsi_dec_fsm_event_handle;
  daemon_inst->func.destroy = vsi_destroy_decoder;
  daemon_inst->func.in_source_change = vsi_dec_in_change;
  return (void *)h;
}

/**
 * @brief DECODING_EVENT @open, set codec format on OUTPUT.
 * @param .
 * @return .
 */
int vsi_dec_set_format(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  if (v4l2_msg->codec_fmt == V4L2_DAEMON_CODEC_UNKNOW_TYPE) {
    return -1;
  }

  // get coded format from client
  h->codec_fmt = v4l2_msg->codec_fmt;
  h->dec_no_reordering =
      v4l2_msg->params.dec_params.io_buffer.no_reordering_decoding ? 1 : 0;
  h->srcwidth = v4l2_msg->params.dec_params.io_buffer.srcwidth;
  h->srcheight = v4l2_msg->params.dec_params.io_buffer.srcheight;
  vsi_dec_register_fsm_table(h);
  ASSERT(h->vsi_v4l2m2m_decoder);

  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
  if (dec->attributes & DEC_ATTR_SUPPORT_SECURE_MODE) {
    h->secure_mode_on = v4l2_msg->params.dec_params.io_buffer.securemode_on;
  }

#ifndef DEC_INPUT_NO_COPY
  if (h->secure_mode_on) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "Inst[%lx]: Not support secure playback in INPUT_COPY mode!\n",
               h->instance_id);
  }
#endif

  // create decoder-instance
  vsi_dec_init(h, v4l2_msg);
  if (h->decoder_inst == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "Inst[%lx]: Failed to create decoder instance for format %d\n",
               h->instance_id,
               h->codec_fmt);
    ASSERT(0);
    h->codec_fmt = V4L2_DAEMON_CODEC_UNKNOW_TYPE;
    h->dec_fatal_error = DAEMON_ERR_DEC_FATAL_ERROR;
    return -1;
  }

  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_output_state2 = 0;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_output_act = DAEMON_QUEUE_ACT_NONE;
  h->dec_capture_act = DAEMON_QUEUE_ACT_NONE;
  h->stream_in_mem.logical_size = 0;

#ifdef DEC_INPUT_NO_COPY
  h->dec_cur_input_vaddr = NULL;
#endif

  h->dec_add_buffer_allowed = 1;
  capture_tot_size = 0;
  return 0;
}

/**
 * @brief OUTPUT_STREAMON @init, switch state from init to capture-setup.
 * @param .
 * @return .
 */
int vsi_dec_capture_setup0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_INIT);
  h->dec_output_state = DAEMON_QUEUE_STATE_ON;
  return 0;
}

/**
 * @brief SOURCE_CHANGE @capture-setup, do nothing
 *        -- flush (DQBUF) capture queue should be handled by CAPTURE_STREAMOFF.
 * @param .
 * @return .
 */
int vsi_dec_capture_setup1(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP);
  if (h->dec_output_state == DAEMON_QUEUE_STATE_PEND_OFF) {
    h->dec_output_state2 = 1;
  }
  h->dec_output_state = DAEMON_QUEUE_STATE_PAUSE;
  return 0;
}

/**
 * @brief CAPTURE_STREAMOFF @capture-setup, returns all queued capture buffers
 * @param .
 * @return .
 */
int vsi_dec_capture_setup2(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE ||
         h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP);

  // All decoded buffers should not be pending in capture queue. So, just flush
  // it.
  vsi_dec_flush_output(h, v4l2_msg);
  h->dec_add_buffer_allowed = 1;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_output_state = DAEMON_QUEUE_STATE_PAUSE;
  return 0;
}

/**
 * @brief CAPTURE_STREAMON/V4L2_DECCMD_START @capture-setup, start streaming on
 * capture queue.
 * @param .
 * @return .
 */
int vsi_dec_capture_setup3(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE ||
         h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP);

  h->dec_capture_state = DAEMON_QUEUE_STATE_ON;
  if (h->dec_output_state == DAEMON_QUEUE_STATE_PAUSE) {
    h->dec_output_state = DAEMON_QUEUE_STATE_ON;
    if (h->dec_output_state2) h->dec_output_state = DAEMON_QUEUE_STATE_PEND_OFF;
    h->dec_output_state2 = 0;
  }

  if (h->dec_output_state == DAEMON_QUEUE_STATE_PAUSE) {
    h->dec_output_state = DAEMON_QUEUE_STATE_ON;
  }

  h->dec_wait_dbp_buf = 0;
  return 0;
}

int vsi_dec_capture_setup4(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP);
  vsi_dec_flush_output(h, v4l2_msg);
  h->dec_add_buffer_allowed = 1;
  if (h->dec_output_state == DAEMON_QUEUE_STATE_PEND_OFF) {
    h->dec_output_state2 = 1;
  }
  h->dec_output_state = DAEMON_QUEUE_STATE_PAUSE;
  return 0;
}

/**
 * @brief DEC_PENDING_FLUSH_EVENT @decode, pause OUTPUT queue, CAPTURE streaming
 * remains.
 * @param .
 * @return .
 */
int vsi_dec_pause0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE);

  while (0 == vsi_dec_dequeue_pic(h))
    ;
  if (h->dec_output_state == DAEMON_QUEUE_STATE_ON &&
      h->dec_capture_state == DAEMON_QUEUE_STATE_ON) {
    h->dec_output_state = DAEMON_QUEUE_STATE_PAUSE;
  } else if (h->dec_output_state == DAEMON_QUEUE_STATE_PAUSE) {
    // h->dec_capture_act = DAEMON_QUEUE_ACT_CONSUME;
    h->dec_output_state = DAEMON_QUEUE_STATE_ON;
    if (h->dec_output_state2) h->dec_output_state = DAEMON_QUEUE_STATE_PEND_OFF;
    h->dec_output_state2 = 0;
  }

  return 0;
}

/**
 * @brief V4L2_DEC_CMD_STOP @decode, turn off OUTPUT queue and start draining.
 * @param .
 * @return .
 */
int vsi_dec_drain0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  //    ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE);

  // Not flush all queued buffers on OUTPUT, according to spec4.5.1.10
  h->dec_output_state = DAEMON_QUEUE_STATE_PEND_OFF;
  h->dec_output_act = DAEMON_QUEUE_ACT_EOS;
  return 0;
}

/**
 * @brief DEC_GOT_EOS_MARK_EVENT @decode/capture-setup, or
 * @brief DEC_PENDING_FLUSH_EVENT @drain,
 * @brief  to take all of decoded pictures.
 * @param .
 * @return .
 */
int vsi_dec_drain1(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  while (0 == vsi_dec_dequeue_pic(h))
    ;
  return 0;
}

/**
 * @brief V4L2_DEC_CMD_STOP @capture-setup, turn off OUTPUT queue.
 * @param .
 * @return .
 */
int vsi_dec_drain2(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP);

  // Not flush all queued buffers on OUTPUT, according to spec4.5.1.10
  if (h->dec_output_state == DAEMON_QUEUE_STATE_PAUSE)
    h->dec_output_state2 = 1;
  else
    h->dec_output_state = DAEMON_QUEUE_STATE_PEND_OFF;

  h->dec_output_act = DAEMON_QUEUE_ACT_EOS;
  return 0;
}

/**
 * @brief DEC_RECEIVE_CAPTUREON_EVENT @drain, capture stream on.
 * @param .
 * @return .
 */
int vsi_dec_drain3(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DRAIN);

  h->dec_capture_state = DAEMON_QUEUE_STATE_ON;
  h->dec_wait_dbp_buf = 0;
  return 0;
}


/**
 * @brief OUTPUT_STREAMOFF @decode/source_change, flush all the pending OUTPUT
 * buffers.
 * @param .
 * @return .
 */
int vsi_dec_seek0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE ||
         h->dec_cur_state == DAEMON_DEC_STATE_STOPPED ||
         h->dec_cur_state == DAEMON_DEC_STATE_CAPTURE_SETUP ||
         h->dec_cur_state == DAEMON_DEC_STATE_END_OF_STREAM ||
         h->dec_cur_state == DAEMON_DEC_STATE_SOURCE_CHANGE);
  vsi_dec_flush_input(h, v4l2_msg);
  while (0 == vsi_dec_drop_pic(h))
    ;

  if (h->dec_cur_state != DAEMON_DEC_STATE_STOPPED) {
    dec->seek(h, v4l2_msg);
  }

  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  // just throw away the decoded pictures.
  h->dec_capture_act = DAEMON_QUEUE_ACT_DROP;
  return 0;
}

/**
 * @brief CAPTURE_STREAMOFF/V4L2_DEC_CMD_STOP @seek, drop all the decoded (not
 * dequeued yet) CAPTURE buffers.
 * @param .
 * @return .
 */
int vsi_dec_seek1(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_SEEK);

//  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_act = DAEMON_QUEUE_ACT_DROP;
  return 0;
}

/**
 * @brief CAPTURE_STREAMON/V4L2_DEC_CMD_START @seek, turn on CAPTURE streaming.
 * @param .
 * @return .
 */
int vsi_dec_seek2(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  h->dec_capture_eos = 0;

  h->dec_capture_state = DAEMON_QUEUE_STATE_ON;
  return 0;
}

/**
 * @brief OUTPUT_STREAMON @seek, turn on OUTPUT streaming.
 * @param .
 * @return .
 */
int vsi_dec_seek3(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_SEEK);

  if (h->dec_output_state2 != 0) {
    HANTRO_LOG(HANTRO_LEVEL_DEBUG, "reset dec_output_state2\n");
    h->dec_output_state2 = 0;
  }

  if (h->existed_dpb_nums)
    h->dec_output_state = DAEMON_QUEUE_STATE_ON;
  else
    h->dec_output_state = DAEMON_QUEUE_STATE_PAUSE;
  return 0;
}

/**
 * @brief CAPTURE_STREAMOFF @seek, drop all the decoded (not
 * dequeued yet) CAPTURE buffers and destroy existed dpb buffers.
 * @param .
 * @return .
 */
int vsi_dec_seek4(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_SEEK);
  h->dec_add_buffer_allowed = 1;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_act = DAEMON_QUEUE_ACT_DROP;
  vsi_dec_flush_output(h, v4l2_msg);
  return 0;
}

/**
 * @brief OUTPUT_STREAMOFF on state drain @seek, flush all the pending OUTPUT,
 * @param .
 * @return .
 */
int vsi_dec_seek5(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg)
{
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "seek5: dec_capture_eos %d\n", h->dec_capture_eos);
  vsi_dec_flush_input(h, v4l2_msg);

  if (!h->dec_capture_eos) {
    while (0 == vsi_dec_drop_pic(h));
    dec->seek(h, v4l2_msg);
  }

  h->dec_output_last_inbuf_idx = INVALID_IOBUFF_IDX;
  h->dec_output_act = DAEMON_QUEUE_ACT_NONE;
  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_act = DAEMON_QUEUE_ACT_DROP;
  h->dec_eos = 0;
  h->dec_capture_eos = 0;

  return 0;
}

/**
 * @brief SOURCE_CHANGE @decode/drain, implicit drain CAPTURE queue.
 * @param .
 * @return .
 */
int vsi_dec_reschange0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE ||
         h->dec_cur_state == DAEMON_DEC_STATE_DRAIN);
  if (h->dec_output_state == DAEMON_QUEUE_STATE_PEND_OFF) {
    h->dec_output_state2 = 1;
  }
  h->dec_output_state = DAEMON_QUEUE_STATE_PAUSE;
  // h->dec_capture_act = DAEMON_QUEUE_ACT_FLUSH;
  return 0;
}

/**
 * @brief DEC_LAST_PIC EVENT @source_change, do nothing.
 * @param .
 * @return .
 */
int vsi_dec_reschange1(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_SOURCE_CHANGE);

  // vsi_dec_flush_output(h, v4l2_msg);
  h->dec_add_buffer_allowed = 0;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  return 0;
}

/**
 * @brief DEC_RECEIVE_CAPTUREOFF_EVENT EVENT @source_change, destroy existed dpb buffers.
 * @param .
 * @return .
 */
int vsi_dec_reschange2(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_SOURCE_CHANGE);
  while (0 == vsi_dec_drop_pic(h))
    ;
   vsi_dec_flush_output(h, v4l2_msg);
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: Reveive CAPTURE_OFF @ source_change\n",
             h->instance_id);
  return 0;
}

/**
 * @brief CAPTURE_STREAMOFF @decode/drain, drop all the pending OUTPUT & CAPTURE
 * buffers.
 * @param .
 * @return .
 */
int vsi_dec_stop0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE ||
         h->dec_cur_state == DAEMON_DEC_STATE_DRAIN);
  vsi_dec_flush_input(h, v4l2_msg);
  //    vsi_dec_flush_output(h, v4l2_msg);
  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  return 0;
}

/**
 * @brief OUTPUT_STREAMON @stopped, turn on OUTPUT streaming, start to decode.
 * @param .
 * @return .
 */
int vsi_dec_stop1(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_STOPPED);

  h->dec_output_state = DAEMON_QUEUE_STATE_ON;
  return 0;
}

/**
 * @brief DESTROY @stopped, destroy instance.
 * @param .
 * @return .
 */
int vsi_dec_stop2(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_STOPPED);

  vsi_dec_destroy(h, v4l2_msg);
  return 0;
}

/**
 * @brief DESTROY @any non-stopped state, destroy instance.
 * @param .
 * @return .
 */
int vsi_dec_stop3(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  vsi_dec_flush_input(h, v4l2_msg);
  vsi_dec_flush_output(h, v4l2_msg);

  vsi_dec_destroy(h, v4l2_msg);
  return 0;
}

/**
 * @brief CAPTURE_STREAMOFF @decode/drain, drop all the pending OUTPUT & CAPTURE
 * buffers, and destroy existed dpb buffers.
 * @param .
 * @return .
 */
int vsi_dec_stop4(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE ||
         h->dec_cur_state == DAEMON_DEC_STATE_DRAIN);
  vsi_dec_flush_input(h, v4l2_msg);
  vsi_dec_flush_output(h, v4l2_msg);
  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  return 0;
}

/**
 * @brief CAPTURE_STREAMOFF @stop, destroy existed dpb buffers
 * buffers.
 * @param .
 * @return .
 */
int vsi_dec_stop5(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_STOPPED);
  ASSERT(h->dec_capture_state == DAEMON_QUEUE_STATE_OFF);
  vsi_dec_flush_output(h, v4l2_msg);
  return 0;
}

/**
 * @brief DEC_FATAL_ERROR EVENT @open, flush all OUTPUT & CAPTURE buffers.
 * @param .
 * @return .
 */
int vsi_dec_fatal_error0(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_OPEN);

  vsi_dec_flush_input(h, v4l2_msg);
  vsi_dec_flush_output(h, v4l2_msg);
  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_DEC_FATAL_ERROR);
  return 0;
}

/**
 * @brief DEC_FATAL_ERROR EVENT @decode, flush all OUTPUT & CAPTURE buffers.
 * @param .
 * @return .
 */
int vsi_dec_fatal_error1(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  vsi_dec_flush_input(h, v4l2_msg);
  vsi_dec_flush_output(h, v4l2_msg);
  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  send_fatalerror_orphan_msg(h->instance_id, h->dec_fatal_error ?
                             h->dec_fatal_error : DAEMON_ERR_DEC_FATAL_ERROR);
  return 0;
}

/**
 * @brief DEC_FATAL_ERROR EVENT @init/drain, flush all OUTPUT & CAPTURE buffers.
 * @param .
 * @return .
 */
int vsi_dec_fatal_error2(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  ASSERT(h->dec_cur_state == DAEMON_DEC_STATE_DECODE);

#if 1
  vsi_dec_stop0(h, v4l2_msg);
  send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_DEC_FATAL_ERROR);
  return 0;
#else
  vsi_dec_flush_input(h, v4l2_msg);
  vsi_dec_flush_output(h, v4l2_msg);
  h->dec_output_state = DAEMON_QUEUE_STATE_OFF;
  h->dec_capture_state = DAEMON_QUEUE_STATE_OFF;
  return 0;
#endif
}

/**
 * @brief do nothing.
 * @param .
 * @return .
 */
int vsi_dec_trans_state(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) { return 0; }

v4l2_decoder_fsm_element vsi_v4l2dec_fsm_trans_table[] = {
    /*OPEN*/
    {DEC_BUFFER_EVENT,              DAEMON_DEC_STATE_OPEN,              vsi_dec_set_format,         DAEMON_DEC_STATE_INIT},
    {DEC_FATAL_ERROR_EVENT,         DAEMON_DEC_STATE_OPEN,              vsi_dec_fatal_error0,       DAEMON_DEC_STATE_STOPPED},

    /*INIT*/
    {DEC_RECEIVE_OUTPUTON_EVENT,    DAEMON_DEC_STATE_INIT,              vsi_dec_capture_setup0,     DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_BUFFER_EVENT,              DAEMON_DEC_STATE_INIT,              vsi_dec_capture_setup0,     DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_INIT,              vsi_dec_stop3,              DAEMON_DEC_STATE_DESTROYED},
    {DEC_FATAL_ERROR_EVENT,         DAEMON_DEC_STATE_INIT,              vsi_dec_fatal_error1,       DAEMON_DEC_STATE_STOPPED},

    /*CAPTURE-SETUP*/
    {DEC_SOURCE_CHANGE_EVENT,       DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_capture_setup1,     DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_OUTPUTOFF_EVENT,   DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_seek0,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CAPTUREOFF_EVENT,  DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_capture_setup4,     DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_CMDSTART_EVENT,    DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_capture_setup3,     DAEMON_DEC_STATE_DECODE},
    {DEC_RECEIVE_CMDSTOP_EVENT,     DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_drain2,             DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_CAPTUREON_EVENT,   DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_capture_setup3,     DAEMON_DEC_STATE_DECODE},
    {DEC_GOT_EOS_MARK_EVENT,        DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_drain1,             DAEMON_DEC_STATE_DRAIN},
    {DEC_LAST_PIC_EVENT,            DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_stop0,              DAEMON_DEC_STATE_STOPPED},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_stop3,              DAEMON_DEC_STATE_DESTROYED},
    {DEC_FATAL_ERROR_EVENT,         DAEMON_DEC_STATE_CAPTURE_SETUP,     vsi_dec_fatal_error1,       DAEMON_DEC_STATE_STOPPED},

    /*DECODING*/
    {DEC_BUFFER_EVENT,              DAEMON_DEC_STATE_DECODE,            vsi_dec_trans_state,        DAEMON_DEC_STATE_DECODE},
    {DEC_PENDING_FLUSH_EVENT,       DAEMON_DEC_STATE_DECODE,            vsi_dec_pause0,             DAEMON_DEC_STATE_DECODE},
    {DEC_SOURCE_CHANGE_EVENT,       DAEMON_DEC_STATE_DECODE,            vsi_dec_reschange0,         DAEMON_DEC_STATE_SOURCE_CHANGE},
    {DEC_RECEIVE_OUTPUTOFF_EVENT,   DAEMON_DEC_STATE_DECODE,            vsi_dec_seek0,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CAPTUREOFF_EVENT,  DAEMON_DEC_STATE_DECODE,            vsi_dec_capture_setup2,     DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_CAPTUREON_EVENT,   DAEMON_DEC_STATE_DECODE,            vsi_dec_capture_setup3,     DAEMON_DEC_STATE_DECODE},
    {DEC_RECEIVE_CMDSTOP_EVENT,     DAEMON_DEC_STATE_DECODE,            vsi_dec_drain0,             DAEMON_DEC_STATE_DRAIN},
    {DEC_GOT_EOS_MARK_EVENT,        DAEMON_DEC_STATE_DECODE,            vsi_dec_drain1,             DAEMON_DEC_STATE_DRAIN},
    {DEC_FATAL_ERROR_EVENT,         DAEMON_DEC_STATE_DECODE,            vsi_dec_fatal_error2,       DAEMON_DEC_STATE_STOPPED},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_DECODE,            vsi_dec_stop3,              DAEMON_DEC_STATE_DESTROYED},

    /*SEEK*/
    {DEC_RECEIVE_CAPTUREOFF_EVENT,  DAEMON_DEC_STATE_SEEK,              vsi_dec_seek4,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CAPTUREON_EVENT,   DAEMON_DEC_STATE_SEEK,              vsi_dec_seek2,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CMDSTOP_EVENT,     DAEMON_DEC_STATE_SEEK,              vsi_dec_seek1,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CMDSTART_EVENT,    DAEMON_DEC_STATE_SEEK,              vsi_dec_seek2,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_OUTPUTON_EVENT,    DAEMON_DEC_STATE_SEEK,              vsi_dec_seek3,              DAEMON_DEC_STATE_DECODE},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_SEEK,              vsi_dec_stop3,              DAEMON_DEC_STATE_DESTROYED},
    {DEC_FATAL_ERROR_EVENT,         DAEMON_DEC_STATE_SEEK,              vsi_dec_fatal_error1,       DAEMON_DEC_STATE_STOPPED},

    /*SOURCE(RESOLUTION) CHANGE*/
    {DEC_LAST_PIC_EVENT,            DAEMON_DEC_STATE_SOURCE_CHANGE,     vsi_dec_reschange1,         DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_OUTPUTOFF_EVENT,   DAEMON_DEC_STATE_SOURCE_CHANGE,     vsi_dec_seek0,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CAPTUREON_EVENT,   DAEMON_DEC_STATE_SOURCE_CHANGE,     vsi_dec_trans_state,        DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_CAPTUREOFF_EVENT,  DAEMON_DEC_STATE_SOURCE_CHANGE,     vsi_dec_reschange2,         DAEMON_DEC_STATE_CAPTURE_SETUP},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_SOURCE_CHANGE,     vsi_dec_stop3,              DAEMON_DEC_STATE_DESTROYED},

    /*END OF STREAM*/
    {DEC_BUFFER_EVENT,              DAEMON_DEC_STATE_END_OF_STREAM,     vsi_dec_trans_state,        DAEMON_DEC_STATE_DRAIN},
    {DEC_RECEIVE_OUTPUTOFF_EVENT,   DAEMON_DEC_STATE_END_OF_STREAM,     vsi_dec_seek0,              DAEMON_DEC_STATE_SEEK},

    /*DRAIN*/
    {DEC_BUFFER_EVENT,              DAEMON_DEC_STATE_DRAIN,             vsi_dec_trans_state,        DAEMON_DEC_STATE_DRAIN},
    {DEC_PENDING_FLUSH_EVENT,       DAEMON_DEC_STATE_DRAIN,             vsi_dec_drain1,             DAEMON_DEC_STATE_DRAIN},
    {DEC_SOURCE_CHANGE_EVENT,       DAEMON_DEC_STATE_DRAIN,             vsi_dec_reschange0,         DAEMON_DEC_STATE_SOURCE_CHANGE},
    {DEC_LAST_PIC_EVENT,            DAEMON_DEC_STATE_DRAIN,             vsi_dec_stop0,              DAEMON_DEC_STATE_STOPPED},
    {DEC_RECEIVE_CAPTUREON_EVENT,   DAEMON_DEC_STATE_DRAIN,             vsi_dec_drain3,             DAEMON_DEC_STATE_DRAIN},
    {DEC_RECEIVE_OUTPUTOFF_EVENT,   DAEMON_DEC_STATE_DRAIN,             vsi_dec_seek5,              DAEMON_DEC_STATE_SEEK},
    {DEC_RECEIVE_CAPTUREOFF_EVENT,  DAEMON_DEC_STATE_DRAIN,             vsi_dec_stop4,              DAEMON_DEC_STATE_STOPPED},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_DRAIN,             vsi_dec_stop3,              DAEMON_DEC_STATE_DESTROYED},
    {DEC_FATAL_ERROR_EVENT,         DAEMON_DEC_STATE_DRAIN,             vsi_dec_fatal_error1,       DAEMON_DEC_STATE_STOPPED},

    /*STOPPPED*/
    {DEC_RECEIVE_OUTPUTOFF_EVENT,   DAEMON_DEC_STATE_STOPPED,           vsi_dec_seek0,              DAEMON_DEC_STATE_SEEK}, 
    {DEC_RECEIVE_OUTPUTON_EVENT,    DAEMON_DEC_STATE_STOPPED,           vsi_dec_stop1,              DAEMON_DEC_STATE_DECODE},
    {DEC_RECEIVE_CAPTUREOFF_EVENT,  DAEMON_DEC_STATE_STOPPED,           vsi_dec_stop5,              DAEMON_DEC_STATE_STOPPED},
    {DEC_RECEIVE_CAPTUREON_EVENT,   DAEMON_DEC_STATE_STOPPED,           vsi_dec_seek2,              DAEMON_DEC_STATE_DECODE},
    {DEC_RECEIVE_DESTROY_EVENT,     DAEMON_DEC_STATE_STOPPED,           vsi_dec_stop2,              DAEMON_DEC_STATE_DESTROYED},

};
int vsi_fsm_trans_table_count = sizeof(vsi_v4l2dec_fsm_trans_table) /
                                sizeof((vsi_v4l2dec_fsm_trans_table)[0]);
