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
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

void dpb_init(v4l2_dec_inst *h) {
  memset(h->dpb_buffers, 0, sizeof(h->dpb_buffers));
  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    h->dpb_buffers[i].buff_idx = INVALID_IOBUFF_IDX;
    h->dpb_buffers[i].status = DPB_STATUS_INVALID;
    h->dpb_buffers[i].buff.mem_type = DWL_MEM_TYPE_DPB;
  }
  h->existed_dpb_nums = 0;
}

void dpb_destroy(v4l2_dec_inst *h) {
  struct v4l2_decoder_dbp *p;

  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    p = &h->dpb_buffers[i];
    if (p->pic_ctx) {
      free(p->pic_ctx);
      p->pic_ctx = NULL;
    }

    p->buff.bus_address = 0;
    if (p->buff.virtual_address) {
      unmap_phy_addr_daemon(p->buff.virtual_address, p->buff.size);
      p->buff.virtual_address = NULL;
    }

    p->buff_idx = INVALID_IOBUFF_IDX;
    p->status = DPB_STATUS_INVALID;
  }

  if(h->existed_dpb_nums != 0) {
    vsi_v4l2m2m2_deccodec *dec =
        (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
    if(dec != NULL && dec->remove_buffer != NULL)
      dec->remove_buffer(h);
  }

  h->existed_dpb_nums = 0;
  h->dpb_buffer_added = 0;

}

static uint32_t dpb_find_free_id(v4l2_dec_inst *h) {
  int32_t i;
  for (i = 0; i < MAX_DEC_BUFFERS; i++) {
    if (h->dpb_buffers[i].buff_idx == INVALID_IOBUFF_IDX) {
      return i;
    }
  }
  return INVALID_IOBUFF_IDX;
}

static int32_t dpb_find_idx(v4l2_dec_inst *h, int32_t buf_idx) {
  int32_t i;
  for (i = 0; i < h->existed_dpb_nums; i++) {
    if (h->dpb_buffers[i].buff_idx == buf_idx) {
      return i;
    }
  }
  return INVALID_IOBUFF_IDX;
}

static uint32_t dpb_add_buffer_at(v4l2_dec_inst *h, int32_t buf_idx,
                       v4l2_daemon_dec_buffers *buf) {
  struct v4l2_decoder_dbp *p;
  uint32_t id;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  ASSERT(dec);

  id = dpb_find_free_id(h);
  ASSERT(id < MAX_BUFFER_SIZE);

  p = &h->dpb_buffers[id];
  ASSERT(p->status == DPB_STATUS_INVALID);
  ASSERT(p->buff.virtual_address == NULL);

  p->buff_idx = buf_idx;
  p->buff.virtual_address =
      mmap_phy_addr_daemon(mmap_fd, buf->busOutBuf, buf->OutBufSize);
  if (p->buff.virtual_address == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "MMAP dpb Buffer FAILED\n");
    ASSERT(0);
    return INVALID_IOBUFF_IDX;
  }

  p->buff.bus_address = buf->busOutBuf;
#ifdef VSI_CMODEL
  p->buff.bus_address = (addr_t)p->buff.virtual_address;
#endif
  p->buff.size = p->buff.logical_size = buf->OutBufSize;

  if (NULL == p->pic_ctx) {
    if (dec->pic_ctx_size) {
      p->pic_ctx = calloc(1, dec->pic_ctx_size);
    }
  }

  p->status = DPB_STATUS_EMPTY;

  if (h->dec_add_buffer_allowed && dec->add_buffer) {
    dec->add_buffer(h, &p->buff);
    p->status = DPB_STATUS_DECODE;
  }

  HANTRO_LOG(HANTRO_LEVEL_DEBUG,
             "add dpb buf[%d], status=%d, bus_addr=0x%lx!\n", id, p->status,
             p->buff.bus_address);

  h->existed_dpb_nums++;
  return id;
}

void dpb_refresh_buffer_at(v4l2_dec_inst *h, uint32_t id,
                           v4l2_daemon_dec_buffers *buf) {
  struct v4l2_decoder_dbp *p;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  ASSERT(id < MAX_BUFFER_SIZE);
  ASSERT(buf);

  p = &h->dpb_buffers[id];
  if (buf)
    ASSERT(p->status == DPB_STATUS_RENDER ||
           p->status == DPB_STATUS_PSEUDO_RENDER ||
           p->status == DPB_STATUS_PSEUDO_DECODE);
  ASSERT(p->buff.virtual_address);

#ifndef VSI_CMODEL
  if (buf) ASSERT(p->buff.bus_address == buf->busOutBuf);
#endif
  if (buf) ASSERT(p->buff.size == buf->OutBufSize);

  if (h->dec_add_buffer_allowed && dec && dec->release_pic &&
      p->status == DPB_STATUS_RENDER) {
    dec->release_pic(h, p->pic_ctx);
  }
  p->status = DPB_STATUS_DECODE;
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "pic[%d], status=%d, buf_addr=0x%lx!\n", id,
             p->status, p->buff.bus_address);
}

void dpb_receive_buffer(v4l2_dec_inst *h, uint32_t buf_idx,
                        v4l2_daemon_dec_buffers *buf) {
  int32_t id;
  id = dpb_find_idx(h, buf_idx);
  if (id == INVALID_IOBUFF_IDX) {
    id = dpb_add_buffer_at(h, buf_idx, buf);
    ASSERT(id != INVALID_IOBUFF_IDX);
  } else if (h->dpb_buffers[id].status == DPB_STATUS_RENDER ||
             h->dpb_buffers[id].status == DPB_STATUS_PSEUDO_RENDER ||
             h->dpb_buffers[id].status == DPB_STATUS_PSEUDO_DECODE) {
    dpb_refresh_buffer_at(h, id, buf);
  } else if (h->dpb_buffers[id].status == DPB_STATUS_DECODE) {
#ifndef VSI_CMODEL
    ASSERT(h->dpb_buffers[id].buff.bus_address == buf->busOutBuf);
#endif
    ASSERT(h->dpb_buffers[id].buff.size == buf->OutBufSize);
  } else {
    HANTRO_LOG(HANTRO_LEVEL_WARNING,
               "Update dpb buffer[%d] in wrong state %d, buf_idx = %d!\n", id,
               h->dpb_buffers[id].status, buf_idx);
    // TBD        ASSERT(0);
  }

  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "receive buffer[%d], status=%d!\n", id,
             h->dpb_buffers[id].status);
}

int32_t dpb_find_buffer(v4l2_dec_inst *h, addr_t bus_addr) {
  uint32_t i;

  for (i = 0; i < h->existed_dpb_nums; i++) {
    if (h->dpb_buffers[i].buff.bus_address == bus_addr) {
      return i;
    }
  }

  HANTRO_LOG(HANTRO_LEVEL_ERROR,
             "didn't find dbp buffer with bus address %lx\n", bus_addr);
  return INVALID_IOBUFF_IDX;
}

struct v4l2_decoder_dbp *dpb_list_buffer(v4l2_dec_inst *h, uint32_t id) {
  struct v4l2_decoder_dbp *p;

  ASSERT(id < MAX_BUFFER_SIZE);

  p = &h->dpb_buffers[id];
  if (p->status == DPB_STATUS_INVALID) {
    return NULL;
  }

  return p;
}

struct DWLLinearMem *dpb_get_buffer(v4l2_dec_inst *h, uint32_t id) {
  struct v4l2_decoder_dbp *p;

  ASSERT(id < MAX_BUFFER_SIZE);

  p = &h->dpb_buffers[id];
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "get buf[%d], status=%d!\n", id, p->status);
  if (p->status != DPB_STATUS_EMPTY) {
    return NULL;
  }

  ASSERT(p->buff_idx != INVALID_IOBUFF_IDX);
  p->status = DPB_STATUS_DECODE;
  return &p->buff;
}

void dpb_get_buffers(v4l2_dec_inst *h) {
  struct v4l2_decoder_dbp *p;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    p = dpb_list_buffer(h, i);
    if (p && p->status == DPB_STATUS_EMPTY) {
      if (h->dec_add_buffer_allowed && dec->add_buffer) {
        dec->add_buffer(h, &p->buff);
        p->status = DPB_STATUS_DECODE;
      }
    }
  }
}

int32_t dpb_render_buffer(v4l2_dec_inst *h, vsi_v4l2_dec_picture *pic) {
  int32_t id = INVALID_IOBUFF_IDX;
  int32_t empty_id = INVALID_IOBUFF_IDX;
  struct v4l2_decoder_dbp *p;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);

  if (pic == NULL) {
    for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
      p = dpb_list_buffer(h, i);
      if (p && p->status == DPB_STATUS_DECODE) {
        empty_id = i;
      }
      if (0) {  // p && (p->status == DPB_STATUS_RENDER) && dec &&
                // dec->release_pic) {
        HANTRO_LOG(HANTRO_LEVEL_DEBUG, "release pic[%d], status=%d!\n", i,
                   p->status);
        dec->release_pic(h, p->pic_ctx);
        p->status = DPB_STATUS_PSEUDO_DECODE;
      }
    }
    if (empty_id != INVALID_IOBUFF_IDX) {
      p = dpb_list_buffer(h, empty_id);
      p->status = DPB_STATUS_PSEUDO_RENDER;
      HANTRO_LOG(HANTRO_LEVEL_DEBUG, "render buf[%d], status=%d!\n", empty_id,
                 p->status);
    }
    HANTRO_LOG(HANTRO_LEVEL_DEBUG, "render empty buf[%d]\n", empty_id);
    return empty_id;
  }

  id = dpb_find_buffer(h, pic->output_picture_bus_address);

  ASSERT(id != INVALID_IOBUFF_IDX);

  p = &h->dpb_buffers[id];
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "render buf[%d], status=%d bus address %x\n",
             id, p->status, (unsigned int)pic->output_picture_bus_address);
  ASSERT(p->status == DPB_STATUS_DECODE ||
         p->status == DPB_STATUS_PSEUDO_DECODE);

  memcpy(p->pic_ctx, pic->priv_pic_data, dec->pic_ctx_size);
  p->status = DPB_STATUS_RENDER;

  return id;
}

void dpb_refresh_all_buffers(v4l2_dec_inst *h) {
  struct v4l2_decoder_dbp *p = NULL;
  vsi_v4l2m2m2_deccodec *dec =
      (vsi_v4l2m2m2_deccodec *)(h->vsi_v4l2m2m_decoder);
  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    p = dpb_list_buffer(h, i);
    if (p && (p->status == DPB_STATUS_RENDER) && dec && dec->release_pic) {
      HANTRO_LOG(HANTRO_LEVEL_DEBUG, "release pic[%d], status=%d!\n", i,
                 p->status);
      dec->release_pic(h, p->pic_ctx);
      p->status = DPB_STATUS_PSEUDO_DECODE;
    }
  }
}
