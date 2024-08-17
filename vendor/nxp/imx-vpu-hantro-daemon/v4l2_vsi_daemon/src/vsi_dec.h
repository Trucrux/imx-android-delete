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

#ifndef VSI_DEC_DEC_H
#define VSI_DEC_DEC_H

#include "buffer_list.h"
#include "daemon_instance.h"
#include "dwl.h"
#include "vsi_daemon_debug.h"

/****************************************************************************
 *
 * Macros definitions
 *
 ***************************************************************************/
//#define NXP_TIMESTAMP_MANAGER

#define MAX_DEC_BUFFERS 32
#define TIMESTAMP_SLOT_NUM 256

#define VSI_DEC_GOT_OUTPUT_DATA 0
#define VSI_DEC_EMPTY_OUTPUT_DATA 1

/* Decoder engine attributes*/
#define DEC_ATTR_NONE 0x00000000
#define DEC_ATTR_EOS_THREAD 0x00000001
#define DEC_ATTR_SUPPORT_SECURE_MODE 0x00000002

#ifndef DEC_COPY_INPUT
#define DEC_INPUT_NO_COPY
#else
#define STREAM_INBUFF_SIZE (4 * 1024 * 1024)
#endif

#define EXTRA_DPB_BUFFER_NUM (1)

/* error values definition returned by dequeue_pic/dequeue_empty_pic */
#define DEQUEUE_ERR_PIC_NOT_AVAIL (-1)
#define DEQUEUE_ERR_BUF_NOT_AVAIL (-2)
#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))

/****************************************************************************
 *
 * enum definitions
 *
 ***************************************************************************/
typedef enum {
  DAEMON_DEC_STATE_OPEN,
  DAEMON_DEC_STATE_INIT,
  DAEMON_DEC_STATE_CAPTURE_SETUP,
  DAEMON_DEC_STATE_DECODE,
  DAEMON_DEC_STATE_SOURCE_CHANGE,
  DAEMON_DEC_STATE_SEEK,
  DAEMON_DEC_STATE_END_OF_STREAM,
  DAEMON_DEC_STATE_DRAIN,
  DAEMON_DEC_STATE_STOPPED,
  DAEMON_DEC_STATE_DESTROYED,

  DAEMON_DEC_STATE_TOTAL
} v4l2_inst_dec_state;

typedef enum {
  DAEMON_QUEUE_ACT_NONE,
  DAEMON_QUEUE_ACT_FLUSH,  // For capture queue only, to return all queued
                           // buffers to client (dqbuf).
  DAEMON_QUEUE_ACT_DROP,  // For capture queue, drop decoded buffers (not
                          // dqbuf).
  DAEMON_QUEUE_ACT_EOS,  // For output queue only, to indicate the last queued
                         // buffer is last (end-of-stream) OUTPUT buffer.
  DAEMON_QUEUE_ACT_CONSUME,  // For capture queue only, to return all dpb buffer
                             // to control-software.
} v4l2_queue_action;

typedef enum {
  DAEMON_QUEUE_STATE_OFF,
  DAEMON_QUEUE_STATE_ON,
  DAEMON_QUEUE_STATE_PEND_OFF,  // Process all queued buffers, then set state to
                                // off.
  DAEMON_QUEUE_STATE_PAUSE,  // For output queue only, to pause output queue
                             // streaming.
} v4l2_queue_state;

typedef enum {
  DEC_EMPTY_EVENT,
  // external driectly motivate
  DEC_RECEIVE_CAPTUREON_EVENT,
  DEC_RECEIVE_CAPTUREOFF_EVENT,
  DEC_RECEIVE_OUTPUTON_EVENT,
  DEC_RECEIVE_OUTPUTOFF_EVENT,
  DEC_RECEIVE_CMDSTOP_EVENT,
  DEC_RECEIVE_CMDSTART_EVENT,
  DEC_RECEIVE_DESTROY_EVENT,

  // indrirectly motivate
  DEC_BUFFER_EVENT,
  DEC_LAST_PIC_EVENT,
  DEC_CACHE_IO_BUFFER_EVENT,  // 10
  DEC_FAKE_EVENT,
  // internal motivate
  DEC_SOURCE_CHANGE_EVENT,
  DEC_PIC_DECODED_EVENT,
  DEC_GOT_EOS_MARK_EVENT,
  DEC_NO_DECODING_BUFFER_EVENT,    // buffer is not consumed
  DEC_WAIT_DECODING_BUFFER_EVENT,  // no decoding buffer set to decoder yet
  DEC_PENDING_FLUSH_EVENT,  // decoder requires to flush all decoded frames.

  // abnormal event
  DEC_DECODING_ERROR_EVENT,
  DEC_FATAL_ERROR_EVENT,
  DEC_DECODING_TOTAL_EVENT,
} v4l2_inst_dec_event;

enum v4l2_daemon_dpb_status {
  DPB_STATUS_INVALID,
  DPB_STATUS_EMPTY,
  DPB_STATUS_DECODE,
  DPB_STATUS_RENDER,
  DPB_STATUS_PSEUDO_DECODE,
  DPB_STATUS_PSEUDO_RENDER
};

/****************************************************************************
 *
 * data struct definitions
 *
 ***************************************************************************/
typedef struct _v4l2_dec_inst v4l2_dec_inst;
typedef struct {
  v4l2_inst_dec_event dec_event;
  v4l2_inst_dec_state cur_state;
  int (*eventActFun)(v4l2_dec_inst *, vsi_v4l2_msg *);
  v4l2_inst_dec_state next_state;
} v4l2_decoder_fsm_element;

struct v4l2_decoder_dbp {
  int32_t buff_idx;
  enum v4l2_daemon_dpb_status status;
  struct DWLLinearMem buff;
  void *pic_ctx;  // dec_picture;
};

struct _v4l2_dec_inst {
  v4l2_daemon_codec_fmt codec_fmt;
  unsigned long instance_id;

  /*handlers*/
  v4l2_daemon_inst *handler;
  void *vsi_v4l2m2m_decoder;
  void *decoder_inst;
  const void *dwl_inst;

  /*configurations*/
  uint32_t dec_no_reordering;
  uint32_t secure_mode_on;
  enum DecDpbFlags dec_output_fmt;
  enum DecPicturePixelFormat dec_pixel_fmt;
  int32_t srcwidth;     // encode width
  int32_t srcheight;    // encode height

  /*states*/
  v4l2_inst_dec_state dec_cur_state;
  // queues state, mainly set by state proc.
  // [dec_output_state,dec_capture_state]:
  //     [off,x ] DEP can not decode, but can get decoded pictures.
  //     [on,off] DEP can only decode header.
  //     [on,1off DEP can decode normally.
  v4l2_queue_state dec_output_state;
  v4l2_queue_state dec_capture_state;
  int dec_output_state2;

  v4l2_queue_action dec_output_act;
  v4l2_queue_action dec_capture_act;

  /*counters*/
  uint32_t dec_pic_id;
  uint32_t dec_out_pic_id;
  uint32_t input_frame_cnt;
  uint32_t output_frame_cnt;

  /*dpb buffers*/
  uint32_t existed_dpb_nums;
  uint32_t dpb_buffer_added;
  struct v4l2_decoder_dbp dpb_buffers[MAX_DEC_BUFFERS];
  uint32_t dec_add_buffer_allowed;
  uint32_t dec_wait_dbp_buf;

  /*input managements*/
  BUFFERLIST *bufferlist_input;
  uint32_t dec_in_pic_id;
  int32_t curr_inbuf_idx;
  uint32_t *dec_cur_input_vaddr;
  struct DWLLinearMem stream_in_mem;
  uint32_t dec_in_new_packet;

  /*stream/pic info*/
  v4l2_daemon_dec_info dec_info;
  v4l2_daemon_pic_info pic_info;
  uint32_t dec_interlaced_sequence;

  /*draining managements*/
  pthread_t dec_drain_thread;
  int dec_drain_tid;
  uint32_t dec_output_last_inbuf_idx;
  uint32_t dec_eos;
  uint32_t dec_capture_eos;
  uint32_t dec_last_pic_got;
  uint32_t dec_last_pic_sent;

  /*time-stamp managements*/
  void *tsm;
  uint32_t need_resync_tsm;
  uint32_t consumed_len;
  uint32_t dec_inpkt_pic_decoded_cnt;
#ifndef NXP_TIMESTAMP_MANAGER
  uint64_t dec_timestamp_values[TIMESTAMP_SLOT_NUM];
#endif

  /*status flags*/
  int32_t dec_fatal_error;
  uint32_t dec_aborted;

  /*privates related to different formats*/
  void *priv_data;
  void *priv_pic_data;
  uint32_t dec_inpkt_ignore_picconsumed_event;
  uint32_t discard_dbp_count;
  uint32_t dec_inpkt_flag;

  /*obsoleted*/
  void *dec_dll_handle;
};

typedef struct {
  uint32_t pic_width;   /* pixels width of the picture as stored in memory */
  uint32_t pic_height;  /* pixel height of the picture as stored in memory */
  uint32_t pic_corrupt; /* Indicates that picture is corrupted */
  uint32_t bit_depth;   /* bit depth per pixel stored in memory */
  uint32_t pic_stride;  /* Byte width of the picture as stored in memory */

  uint32_t pic_id; /* Picture id specified by corresponding dec_input*/

  vpu_addr_t output_picture_bus_address; /* DMA bus address of the output
                                            picture buffer */
  vpu_addr_t
      output_picture_chroma_bus_address; /* DMA bus address of the output
                                            picture buffer */
  vpu_addr_t output_rfc_luma_bus_address; /* Bus address of the luminance table */
  vpu_addr_t output_rfc_chroma_bus_address; /* Bus address of the chrominance table */

  uint32_t sar_width;  /* sample aspect ratio */
  uint32_t sar_height; /* sample aspect ratio */

  /* video signal info for HDR */
  uint32_t video_range; /* black level and range of luma chroma signals, If 0,
                           not present */
  uint32_t
      colour_description_present_flag; /* indicate
                                          colour_primaries/transfer_characteristics/matrix_coeffs
                                          present or not */
  uint32_t colour_primaries; /* indicates the chromaticity coordinates of the
                                source primaries */
  uint32_t transfer_characteristics; /* indicate the reference opto-electronic
                                        transfer characteristic function */
  uint32_t
      matrix_coefficients; /* the matrix coefficients used in deriving luma and
                              chroma signals */
  uint32_t chroma_loc_info_present_flag;     /* indicate
                                                chroma_sample_loc_type_top_field or
                                                bottom field are present or not */
  uint32_t chroma_sample_loc_type_top_field; /* specify the location of chroma
                                                samples */
  uint32_t chroma_sample_loc_type_bottom_field; /* specify the location of
                                                   chroma samples */

  /* HDR10 metadata */
  uint32_t present_flag;
  uint32_t red_primary_x;
  uint32_t red_primary_y;
  uint32_t green_primary_x;
  uint32_t green_primary_y;
  uint32_t blue_primary_x;
  uint32_t blue_primary_y;
  uint32_t white_point_x;
  uint32_t white_point_y;
  uint32_t max_mastering_luminance;
  uint32_t min_mastering_luminance;
  uint32_t max_content_light_level;        // MaxCLL
  uint32_t max_frame_average_light_level;  // MaxFALL

  void *priv_pic_data;
  uint32_t chroma_size;

  uint32_t crop_left;
  uint32_t crop_top;
  uint32_t crop_width;
  uint32_t crop_height;
} vsi_v4l2_dec_picture;

typedef struct vsi_v4l2m2m2_deccodec {
  const char *name;
  v4l2_daemon_codec_fmt codec_id;
  uint32_t capabilities;
  uint32_t attributes;
  const char *wrapper_name;
  uint32_t pic_ctx_size;
  v4l2_inst_dec_event (*decode)(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg);
  v4l2_inst_dec_event (*init)(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg);
  v4l2_inst_dec_event (*destroy)(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg);
  int (*drain)(v4l2_dec_inst *h);
  v4l2_inst_dec_event (*seek)(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg);
  int (*get_pic)(v4l2_dec_inst *h, vsi_v4l2_dec_picture *dec_pic_info,
                 uint32_t eos);
  int (*release_pic)(v4l2_dec_inst *h, void *pic);
  int (*add_buffer)(v4l2_dec_inst *h, struct DWLLinearMem *mem_obj);
  int (*remove_buffer)(v4l2_dec_inst *h);
} vsi_v4l2m2m2_deccodec;

#define VSIM2MDEC(NAME, CODEC, PIC_CTX_SIZE, ATTR)             \
  vsi_v4l2m2m2_deccodec vsidaemon_##NAME##_v4l2m2m_decoder = { \
      .name = #NAME "_v4l2m2m",                                \
      .codec_id = CODEC,                                       \
      .pic_ctx_size = PIC_CTX_SIZE,                            \
      .init = NAME##_dec_init,                                 \
      .decode = NAME##_dec_decode,                             \
      .destroy = NAME##_dec_destroy,                           \
      .drain = NAME##_dec_drain,                               \
      .seek = NAME##_dec_seek,                                 \
      .get_pic = NAME##_dec_get_pic,                           \
      .release_pic = NAME##_dec_release_pic,                   \
      .add_buffer = NAME##_dec_add_buffer,                     \
      .remove_buffer = NAME##_dec_remove_buffer,               \
      .capabilities = 1,                                       \
      .wrapper_name = "vsiv4l2daemon",                         \
      .attributes = ATTR,                                      \
  }

extern v4l2_decoder_fsm_element vsi_v4l2dec_fsm_trans_table[];
extern int vsi_fsm_trans_table_count;

#endif  // VSI_DEC_DEC_H
