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

#ifndef DAEMON_INSTNCE_H
#define DAEMON_INSTNCE_H

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include "command_defines.h"
#include "fifo.h"
#include "object_heap.h"
#include "vsi_daemon_debug.h"

typedef struct _v4l2_daemon_inst v4l2_daemon_inst;

//#define LOG_OUTPUT
typedef enum {
  DAEMON_MODE_UNKNOWN = 0,
  DAEMON_MODE_ENCODER,
  DAEMON_MODE_DECODER
} v4l2_daemon_mode;

// for instance flag, bit mapped
typedef enum {
  INST_CATCH_EXCEPTION = 0,
  INST_FATAL_ERROR,
  INST_ZOMBIE,
  INST_DEAD,
  INST_FORCEEXIT,
} v4l2_inst_flag;

typedef struct {
  void *(*create)(v4l2_daemon_inst *h);
  int32_t (*proc)(void *codec, vsi_v4l2_msg *msg);
  int32_t (*destroy)(void *codec);
  int32_t (*in_source_change)(void *codec);
} vsi_daemon_cb;

struct _v4l2_daemon_inst {
  unsigned long instance_id;
  int32_t sequence_id;  // order
  unsigned long flag;

  pthread_t tid;
  pthread_attr_t attr;
  sem_t sem_done;

  jmp_buf sigbuf;

  FifoInst fifo_inst;
  struct object_heap cmds;

  v4l2_daemon_codec_fmt codec_fmt;
  v4l2_daemon_mode codec_mode;

  void *codec;
  vsi_daemon_cb func;

  uint32_t pop_cnt;  // for debug.
};

/**
 * @brief vsi_create_encoder(), create encoder inst.
 * @param v4l2_daemon_inst* h: daemon instance.
 * @return void *: encoder instance.
 */
void *vsi_create_encoder(v4l2_daemon_inst *h);

/**
 * @brief vsi_create_decoder(), create decoder inst.
 * @param v4l2_daemon_inst* h: daemon instance.
 * @return void *: decoder instance.
 */
void *vsi_create_decoder(v4l2_daemon_inst *h);

/**
* @brief vsi_enc_get_codec_format(), get supported codec formats by encoder.
* @param struct vsi_v4l2_dev_info *hwinfo: output info.
* @return int32_t, 0 means support at least 1 format, -1 means error.
*/
int32_t vsi_enc_get_codec_format(struct vsi_v4l2_dev_info *hwinfo);

/**
* @brief vsi_dec_get_codec_format(), get supported codec formats by decoder.
* @param struct vsi_v4l2_dev_info *hwinfo: output info.
* @return int32_t, 0 means support at least 1 format, -1 means error.
*/
int vsi_dec_get_codec_format(struct vsi_v4l2_dev_info *hwinfo);

extern int32_t pipe_fd;
extern int32_t mmap_fd;

/*  open v4l2 device: /dev/v4l2  */
int32_t open_v4l2_device(void);

/*   receive msg from v4l2 device  */
int receive_from_v4l2(int32_t pipe_fd, struct vsi_v4l2_msg *msg, int32_t size);

/*  send an acknowledge immediately  */
void send_ack_to_v4l2(int32_t pipe_fd, struct vsi_v4l2_msg *msg, int32_t size);

/*  send msg or state to v4l2  */
void send_notif_to_v4l2(int32_t pipe_fd, struct vsi_v4l2_msg *buffer,
                        int32_t size);

/* send_enc_orphan_msg(), send BUF_RDY event for encoder. */
void send_enc_orphan_msg(vsi_v4l2_msg *v4l2_msg, int32_t inbuf_idx,
                         int32_t outbuf_idx, int32_t out_buf_size,
                         int32_t ret_value);
/**
 * @brief send_enc_outputbuf_orphan_msg(), send BUF_RDY event for encoder.
 * @param vsi_v4l2_msg* v4l2_msg, pointer of buffer for current message.
 * @param int32_t outbuf_idx, outbuf idx.
 * @param int32_t out_buf_size, stream size.
 * @param int32_t flag, last buffer or not for now.
 * @return void.
 */
void send_enc_outputbuf_orphan_msg(vsi_v4l2_msg *v4l2_msg, int32_t outbuf_idx,
                                   int32_t out_buf_size, int32_t flag);
void send_fatalerror_orphan_msg(unsigned long inst_id, int32_t error);
void send_warning_orphan_msg(unsigned long inst_id, int32_t warn_id);
void send_cmd_orphan_msg(unsigned long inst_id, int32_t cmd_id);

void send_dec_inputbuf_orphan_msg(v4l2_daemon_inst *h, vsi_v4l2_msg *v4l2_msg,
                                  int32_t inbuf_idx, uint32_t flag);

void send_dec_outputbuf_orphan_msg(v4l2_daemon_inst *h, vsi_v4l2_msg *v4l2_msg,
                                   int32_t outbuf_idx, int32_t out_buff_size,
                                   int32_t ret_value);

void send_dec_orphan_msg(v4l2_daemon_inst *h, vsi_v4l2_msg *v4l2_msg,
                         int32_t inbuf_idx, int32_t outbuf_idx,
                         int32_t out_buff_size, int32_t ret_value);

/* open mmap device for map address  */
int32_t open_mmap_device_daemon(void);

/*  map address using mmap device  */
uint32_t *mmap_phy_addr_daemon(int32_t fd, vpu_addr_t phy_addr, int32_t size);

/**
 * @brief unmap_phy_addr_daemon(), unmap memory.
 * @param uint32_t *v_addr: virtual address.
 * @param int32_t size: size of the buffer to unmap.
 * @return
 */
void unmap_phy_addr_daemon(uint32_t *v_addr, int32_t size);

/**
 * @brief send_cmd_to_worker(), send command to worker, and ack to v4l2 driver.
 * @param[in] h: The daemon instance.
 * @param[in] msg: The msg being sent to worker.
 * @return void.
 */
void send_cmd_to_worker(v4l2_daemon_inst *h, vsi_v4l2_msg *msg);

/**
 * @brief send_fake_cmd(), send fake command avoid blocking.
 * @param v4l2_daemon_inst* inst: daemon instance.
 * @return void.
 */
void send_fake_cmd(v4l2_daemon_inst *inst);

/*  open log tools  */
void open_log(char *log_name);

/*  output log to file */
void output_log(int32_t priority, int32_t num, const char *format, ...);

/*  get time for test performance */
void get_time(float *sec);

#endif
