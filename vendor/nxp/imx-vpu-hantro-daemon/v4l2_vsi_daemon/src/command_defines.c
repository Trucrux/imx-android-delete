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

#include <sys/mman.h>
#include <unistd.h>

#include "daemon_instance.h"
#include "vsi_daemon_debug.h"
/**
 * @file command_defines.c
 * @brief Interface between daemon and v4l2.
 */

int32_t pipe_fd = 0;  // open /dev/vsi_daemon_ctrl
int32_t mmap_fd = 0;  // open /dev/mem

/**
 * @brief open_v4l2_device(), open v4l2 device for communication between daemon
 * and driver.
 * @param none.
 * @return int32_t: file handle.
 */
int32_t open_v4l2_device() {
  int32_t fd = open(PIPE_DEVICE, O_RDWR);
  if (fd == -1) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed to open: %s\n", PIPE_DEVICE);
    output_log(LOG_DEBUG, 0, "failed to open /dev/v4l2");
    ASSERT(0);
  }
  output_log(LOG_DEBUG, 0, "open device: /dev/v4l2");
  return fd;
}

/**
 * @brief receive_from_v4l2(), receive msg from v4l2 device, block if no msg.
 * @param int32_t pipe_fd, device fd.
 * @param struct vsi_v4l2_msg* msg, pointer of buffer for receiving message.
 * @param int32_t size, size to receive.
 * @return void.
 */
int receive_from_v4l2(int32_t pipe_fd, struct vsi_v4l2_msg *msg, int32_t size) {
  int err = read(pipe_fd, msg, size);
  if (err < 0) return -errno;

  return err;
}

/**
 * @brief send_ack_to_v4l2(), after receive msg from v4l2 device, send an
 * acknowledge immediately.
 * @param int32_t pipe_fd, device fd.
 * @param struct vsi_v4l2_msg *msg, pointer of buffer for sending message.
 * @param int32_t size, size to send.
 * @return void.
 */
void send_ack_to_v4l2(int32_t pipe_fd, struct vsi_v4l2_msg *msg, int32_t size) {
  int32_t wsize = size;
  if (write(pipe_fd, msg, wsize) != size) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed to write to: %s\n", PIPE_DEVICE);
    ASSERT(0);
  }
}

/**
 * @brief send_notif_to_v4l2(), send notification to v4l2.
 * @param int32_t pipe_fd, device fd.
 * @param struct vsi_v4l2_msg *msg, pointer of buffer for sending message.
 * @param int32_t size, size to send.
 * @return void.
 */
void send_notif_to_v4l2(int32_t pipe_fd, struct vsi_v4l2_msg *msg,
                        int32_t size) {
  send_ack_to_v4l2(pipe_fd, msg, size);
}

/**
 * @brief send_enc_orphan_msg(), send BUF_RDY event for encoder.
 * @param vsi_v4l2_msg* v4l2_msg, pointer of buffer for current message.
 * @param int32_t inbuf_idx, inbuf idx.
 * @param int32_t outbuf_idx, outbuf idx.
 * @param int32_t out_buf_size, stream size.
 * @param int32_t ret_value, return value.
 * @return void.
 */
void send_enc_orphan_msg(vsi_v4l2_msg *v4l2_msg, int32_t inbuf_idx,
                         int32_t outbuf_idx, int32_t out_buf_size,
                         int32_t ret_value) {
  vsi_v4l2_msg msg = {
      0,
  };

  msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
  msg.params.enc_params.io_buffer.inbufidx = inbuf_idx;
  msg.params.enc_params.io_buffer.outbufidx = -1;
  msg.params.enc_params.io_buffer.bytesused = 0;
  msg.error = ret_value;
  msg.inst_id = v4l2_msg->inst_id;
  msg.seq_id = NO_RESPONSE_SEQID;
  msg.param_type = v4l2_msg->param_type;
  msg.size = sizeof(struct v4l2_daemon_enc_buffers);
  msg.params.enc_params.io_buffer.timestamp = v4l2_msg->params.enc_params.io_buffer.timestamp;
  send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr) + msg.size);

  if(outbuf_idx == -1) {//when -1, encoder may be overflow and will not return buffers.
      msg.cmd_id = V4L2_DAEMON_VIDIOC_PICCONSUMED;
  }

  msg.params.enc_params.io_buffer.inbufidx = -1;
  msg.params.enc_params.io_buffer.outbufidx = outbuf_idx;
  msg.params.enc_params.io_buffer.bytesused = out_buf_size;
  send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr) + msg.size);
}

/**
 * @brief send_enc_outputbuf_orphan_msg(), send BUF_RDY event for encoder.
 * @param vsi_v4l2_msg* v4l2_msg, pointer of buffer for current message.
 * @param int32_t outbuf_idx, outbuf idx.
 * @param int32_t out_buf_size, stream size.
 * @param int32_t flag, last buffer or not for now.
 * @return void.
 */
void send_enc_outputbuf_orphan_msg(vsi_v4l2_msg *v4l2_msg, int32_t outbuf_idx,
                                   int32_t out_buf_size, int32_t flag) {
  vsi_v4l2_msg msg = {
      0,
  };

  msg.inst_id = v4l2_msg->inst_id;
  msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
  msg.param_type |= flag;
  msg.params.enc_params.io_buffer.inbufidx = -1;
  msg.params.enc_params.io_buffer.outbufidx = outbuf_idx;
  msg.params.enc_params.io_buffer.bytesused = out_buf_size;
  msg.seq_id = NO_RESPONSE_SEQID;
  msg.size = sizeof(struct v4l2_daemon_enc_buffers);
  send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr) + msg.size);
}

#define VSI_DEC_GOT_OUTPUT_DATA 0
#define VSI_DEC_EMPTY_OUTPUT_DATA 1

/**
 * @brief send_dec_inputbuf_orphan_msg(), send BUF_RDY of src queue for dec.
 * @param vsi_v4l2_msg* v4l2_msg, pointer of buffer for current message.
 * @param int32_t inbuf_idx, input buffer index.
 * @param uint32_t flag, input buffer flag.
 * @return void.
 */
void send_dec_inputbuf_orphan_msg(v4l2_daemon_inst *h, vsi_v4l2_msg *v4l2_msg,
                                  int32_t inbuf_idx, uint32_t flag) {
  vsi_v4l2_msg msg = {
      0,
  };

  msg.inst_id = h->instance_id;
  msg.seq_id = NO_RESPONSE_SEQID;
  msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
  msg.param_type |= flag;
  msg.error = 0;
  msg.size = sizeof(struct v4l2_daemon_dec_buffers);
  msg.params.dec_params.io_buffer.inbufidx = inbuf_idx;
  msg.params.dec_params.io_buffer.outbufidx = INVALID_IOBUFF_IDX;

  if (pipe_fd >= 0)
    send_notif_to_v4l2(pipe_fd, &msg,
                       sizeof(struct vsi_v4l2_msg_hdr) + msg.size);
}

/**
 * @brief send_fatalerror_orphan_msg(), send error notification to driver.
 * @param unsigned long inst_id, current instance id.
 * @param int32_t error, error id.
 * @return void.
 */
void send_fatalerror_orphan_msg(unsigned long inst_id, int32_t error) {
  vsi_v4l2_msg msg = {0};

  msg.inst_id = inst_id;
  msg.seq_id = NO_RESPONSE_SEQID;
  //    msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
  msg.error = error;
  msg.size = 0;

  if (pipe_fd >= 0)
    send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr));
}

/**
 * @brief send_warning_orphan_msg(), send warning notification to driver.
 * @param unsigned long inst_id, current instance id.
 * @param int32_t warn_id, warning id.
 * @return void.
 */
void send_warning_orphan_msg(unsigned long inst_id, int32_t warn_id)
{
  vsi_v4l2_msg msg = {0};

  if (pipe_fd >= 0) {
    msg.inst_id = inst_id;
    msg.seq_id = NO_RESPONSE_SEQID;
    msg.cmd_id = V4L2_DAEMON_VIDIOC_WARNONOPTION;
    msg.error = warn_id;
    msg.size = 0;
    send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr));
  }
}

/**
 * @brief send_cmd_orphan_msg(), send specified cmd message to driver.
 * @param unsigned long inst_id, current instance id.
 * @param int32_t cmd_id, message id.
 * @return void.
 */
void send_cmd_orphan_msg(unsigned long inst_id, int32_t cmd_id)
{
  vsi_v4l2_msg msg = {0};

  if (pipe_fd >= 0) {
    msg.inst_id = inst_id;
    msg.seq_id = NO_RESPONSE_SEQID;
    msg.cmd_id = cmd_id;
    send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr));
  }
}

#if 0
void send_dec_outputbuf_orphan_msg(v4l2_daemon_inst* h,
                                      vsi_v4l2_msg* v4l2_msg,
                                      int32_t outbuf_idx,
                                      int32_t out_buff_size, int32_t ret_value)
{
    vsi_v4l2_msg msg = {0};

    memcpy(&msg, v4l2_msg, sizeof(vsi_v4l2_msg));
    msg.inst_id = h->instance_id;
    msg.seq_id = NO_RESPONSE_SEQID;
    msg.cmd_id = V4L2_DAEMON_VIDIOC_BUF_RDY;
    msg.error = 0;
    msg.size = sizeof(struct v4l2_daemon_dec_buffers);
    msg.params.dec_params.io_buffer.inbufidx = INVALID_IOBUFF_IDX;
    msg.params.dec_params.io_buffer.outbufidx = outbuf_idx;
    msg.params.dec_params.io_buffer.OutBufSize = out_buff_size;
    msg.params.dec_params.io_buffer.bytesused = (ret == VSI_DEC_EMPTY_OUTPUT_DATA) ? 0 : out_buff_size;

    if(pipe_fd >= 0)
    {
        send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg_hdr) + msg.size);
    }
}
/**
* @brief send_dec_orphan_msg(), initiatively send deccoder's state to v4l2
* @param v4l2_daemon_inst* h, daemon instance.
* @param vsi_v4l2_msg* v4l2_msg, pointer of buffer for sending message.
* @param int32_t inbuf_idx, inbuf idx.
* @param int32_t outbuf_idx, outbuf idx.
* @param int32_t out_buff_size, destination buffer size.
* @param int32_t ret_value, error number.
* @return void.
*/
void send_dec_orphan_msg(v4l2_daemon_inst* h,
                             vsi_v4l2_msg* v4l2_msg,
                             int32_t inbuf_idx,
                             int32_t outbuf_idx, int32_t out_buff_size,
                             int32_t ret_value)
{
    if(inbuf_idx != INVALID_IOBUFF_IDX)
        send_dec_inputbuf_orphan_msg(h, v4l2_msg, inbuf_idx);
    if(outbuf_idx != INVALID_IOBUFF_IDX)
        send_dec_outputbuf_orphan_msg(h, v4l2_msg, outbuf_idx, out_buff_size, ret_value);
}
#endif

/**
 * @brief open_mmap_device_daemon(), open mmap device for map address.
 * @param none.
 * @return int32_t file handle.
 */
int32_t open_mmap_device_daemon() {
  int32_t fd = open(MMAP_DEVICE, O_RDWR);
  if (fd == -1) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed to open: %s\n", MMAP_DEVICE);
    ASSERT(0);
  }
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "open device: %s\n", MMAP_DEVICE);
  return fd;
}

/**
 * @brief mmap_phy_addr_daemon(), map address using mmap device.
 * @param int32_t fd: file handle.
 * @param uint32_t phy_addr: physical address.
 * @param int32_t size: size of the buffer to map.
 * @return uint32_t *, pointer of address that mapped.
 */
uint32_t *mmap_phy_addr_daemon(int32_t fd, vpu_addr_t phy_addr, int32_t size) {
  void *v_addr; /* virtual addr */
  vpu_addr_t aligned_phy_addr = phy_addr & (~(MEM_PAGE_SIZE - 1));
  uint32_t offset = phy_addr - aligned_phy_addr;

  if (offset & 0x3) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "phy address 0x%llx unmappable\n", phy_addr);
    return NULL;
  }
  v_addr = mmap(0, size + offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                aligned_phy_addr);
  if (v_addr == MAP_FAILED) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "mmap_phy_addr: Failed to mmap phy Address! mmap size %d, phy "
               "address 0x%llx\n",
               size, phy_addr);
    return NULL;
  }
  return (uint32_t *)(v_addr + offset);
}

/**
 * @brief unmap_phy_addr_daemon(), unmap memory.
 * @param uint32_t *v_addr: virtual address.
 * @param int32_t size: size of the buffer to unmap.
 * @return
 */
void unmap_phy_addr_daemon(uint32_t *v_addr, int32_t size) {
  int ret;
  void *p_aligned = (void *)(((uint64_t)v_addr) & (~(MEM_PAGE_SIZE - 1)));

  ret = munmap(p_aligned, size + ((void *)v_addr - p_aligned));
  ASSERT(0 == ret);
  (void)ret;
}

/**
 * @brief open_log(), open log tools for debug daemon.
 * @param int8_t * log_name: log name.
 * @return none.
 */
void open_log(char *log_name) {
#ifdef LOG_OUTPUT
  int32_t log_fd;
  remove(log_name);
  if ((log_fd = open(log_name, O_CREAT | O_RDWR | O_APPEND, 0644)) < 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "log_fd open faild.\n");
    return;
  }
  close(STDERR_FILENO);
  dup2(log_fd, STDERR_FILENO);
  close(log_fd);
  openlog(NULL, LOG_PERROR, LOG_DAEMON);  // open log service
#endif
}

/**
 * @brief send_cmd_to_worker(), send command to worker, and ack to v4l2 driver.
 * @param[in] h: The daemon instance.
 * @param[in] msg: The msg being sent to worker.
 * @return void.
 */
void send_cmd_to_worker(v4l2_daemon_inst *h, vsi_v4l2_msg *msg) {
  ASSERT(h != NULL);
  int32_t cmd_ID;
  struct vsi_v4l2_cmd *command;
  cmd_ID = object_heap_allocate(&h->cmds);
  command = (struct vsi_v4l2_cmd *)object_heap_lookup(&h->cmds, cmd_ID);
  if (command == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "object_heap_lookup error.\n");
    return;
  }
  memcpy(&command->msg, msg, sizeof(struct vsi_v4l2_msg));
  FifoPush(h->fifo_inst, command, FIFO_EXCEPTION_DISABLE);
  msg->size = 0;
  send_ack_to_v4l2(pipe_fd, msg, sizeof(struct vsi_v4l2_msg_hdr));  // ack an
                                                                    // ok;
}

/**
 * @brief send_fake_cmd(), send fake command avoid blocking.
 * @param v4l2_daemon_inst* h: daemon instance.
 * @return void.
 */
void send_fake_cmd(v4l2_daemon_inst *h) {
  ASSERT(h != NULL);
  int32_t cmd_ID;
  struct vsi_v4l2_cmd *command;

  cmd_ID = object_heap_allocate(&h->cmds);
  command = (struct vsi_v4l2_cmd *)object_heap_lookup(&h->cmds, cmd_ID);
  if (command == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "object_heap_lookup error.\n");
    return;
  }

  memset(&command->msg, 0, sizeof(vsi_v4l2_msg));
  command->msg.cmd_id = V4L2_DAEMON_VIDIOC_FAKE;
  command->msg.inst_id = h->instance_id;

  FifoPush(h->fifo_inst, command, FIFO_EXCEPTION_DISABLE);
}
/**
 * @brief output_log(), output log for debug daemon, max 4 parameters.
 * @param int32_t priority: log priority.
 * @param int32_t num: parameter numbers.
 * @param const int8_t* format: variable parameter.
 * @return none.
 */
void output_log(int32_t priority, int32_t num, const char *format, ...) {
#ifdef LOG_OUTPUT
#ifndef MAX_NUM
#define MAX_NUM 4
#endif
  if ((num >= 4) || (num < 0)) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "output_log(): parameter error. num=%d\n",
               num);
    return;
  }

  int32_t m_int[MAX_NUM], t_int = 0;
  va_list valist;
  float t1;
  get_time(&t1);

  int8_t format_time[100] = "Time:[%f]    ";
  strcat(format_time, format);

  va_start(valist, format);
  for (int32_t i = 0; i < num; i++) {
    m_int[t_int++] = va_arg(valist, int);
  }
  switch (num) {
    case 0:
      syslog(priority, format_time, t1);
      break;
    case 1:
      syslog(priority, format_time, t1, m_int[0]);
      break;
    case 2:
      syslog(priority, format_time, t1, m_int[0], m_int[1]);
      break;
    case 3:
      syslog(priority, format_time, t1, m_int[0], m_int[1], m_int[2]);
      break;
    case 4:
      syslog(priority, format_time, t1, m_int[0], m_int[1], m_int[2], m_int[3]);
      break;
  }
  va_end(valist);
#endif
}

/**
 * @brief get_time(), get time for test performance.
 * @param float *sec: point to a float that for second of now.
 * @return none.
 */
void get_time(float *sec) {
  struct timeval tv1;
  gettimeofday(&tv1, NULL);
  tv1.tv_sec &= 0xffff;
  *sec = (float)tv1.tv_sec + (float)(tv1.tv_usec) / 1000000;
}
