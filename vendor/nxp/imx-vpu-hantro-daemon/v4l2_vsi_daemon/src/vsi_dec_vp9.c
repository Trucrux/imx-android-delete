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
 * @file vsi_dec_vp9.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */

#include "vsi_dec.h"
#include "buffer_list.h"
#include "dec_dpb_buff.h"

#include "vp9decapi.h"

#ifdef VSI_CMODEL
#include "tb_cfg.h"
#endif

/*------------------------------------------------------------------------*/
#define VP9DEC_DPB_NUMS (9)
#define BUFFER_ALIGN_FACTOR (64)
#define ALIGN_OFFSET(A) ((A) & (BUFFER_ALIGN_FACTOR - 1))
#define ALIGN(A) ((A) & (~(BUFFER_ALIGN_FACTOR - 1)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
/*------------------------------------------------------------------------*/

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

/*------------------------------------------------------------------------*/
typedef struct {
  u32 is_superFrm;  // 0: not a super frame; 1: is a super frame.
  u32 count;        // frame count in this super frame
  u32 index_size;   // index size of this super frame
  u32 cur_frm_id;   // current frame id to decode
  u32 size[8];      // payload size of each frame
  u32 offset[8];    // offset of each frame in current input packet.
} VPX_SUPER_FRAME_INFO;

typedef struct {
  VPX_SUPER_FRAME_INFO super_frame_info;
  struct Vp9DecConfig dec_cfg;

  enum DecDpbFlags dec_output_fmt;
  enum DecPicturePixelFormat dec_pixel_fmt;
  uint32_t set_new_info;
  u32 pic_buff_size;  //output (picture) buffer size calculated by src w/h.
} VP9_PRIV_DATA;

// stubs function in case multi-frame in one package
static int vp9_dec_has_more_frames(v4l2_dec_inst *h,
                                   struct Vp9DecInput *dec_input) {
  return dec_input->data_len ? 1 : 0;
}

static int vp9_dec_release_pic(v4l2_dec_inst *h, void *pic) {
  enum DecRet ret;
  ret = Vp9DecPictureConsumed(h->decoder_inst, (struct Vp9DecPicture *)pic);
  ASSERT(ret == DEC_OK);
  return (ret == DEC_OK) ? 0 : -1;
}

/**
 * @brief vp9_dec_set_info(), , set output & pixel format.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static u32 vp9_dec_set_info(v4l2_dec_inst *h) {
  VP9_PRIV_DATA *priv = (VP9_PRIV_DATA *)h->priv_data;
  struct Vp9DecConfig *dec_cfg = &(priv->dec_cfg);
  enum DecRet rv = DEC_OK;

  if (priv->dec_output_fmt == h->dec_output_fmt &&
      priv->dec_pixel_fmt == h->dec_pixel_fmt) {
    return 0;
  }

  if (priv->dec_output_fmt != h->dec_output_fmt) {
    if (h->dec_output_fmt) {
      dec_cfg->output_format = DEC_OUT_FRM_TILED_4X4;
    } else {
      dec_cfg->output_format = DEC_OUT_FRM_RASTER_SCAN;
    }
    priv->dec_output_fmt = h->dec_output_fmt;
    priv->set_new_info = 1;
  }

  dec_cfg->pixel_format = h->dec_pixel_fmt;
  priv->dec_pixel_fmt = h->dec_pixel_fmt;
  rv = Vp9DecSetInfo(h->decoder_inst, &priv->dec_cfg);
  priv->pic_buff_size = 0;
  (void)rv;
  return 0;
}

static int vp9_dec_remove_buffer(v4l2_dec_inst* h) {
  enum DecRet rv = DEC_OK;
  rv = Vp9DecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}

static int vp9_dec_add_buffer(v4l2_dec_inst *h, struct DWLLinearMem *mem_obj) {
#if 1
  enum DecRet rv = DEC_OK;
  VP9_PRIV_DATA *priv = (VP9_PRIV_DATA *)h->priv_data;
  if (mem_obj) {
    if (h->existed_dpb_nums == 0) {
      vp9_dec_set_info(h);
    }

    if (priv->set_new_info) {
      return 0;
    }
    rv = Vp9DecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Vp9DecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == DEC_OK || rv == DEC_WAITING_FOR_BUFFER) ? 0 : 1;
#endif
}

static void vp9_dec_add_external_buffer(v4l2_dec_inst *h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

static int vp9_dec_get_pic(v4l2_dec_inst *h, vsi_v4l2_dec_picture *dec_pic_info,
                           u32 eos) {
  struct Vp9DecPicture *dec_picture = (struct Vp9DecPicture *)h->priv_pic_data;
  u32 stride = 0;
  enum DecRet re = DEC_OK;

  do {
    re = Vp9DecNextPicture(h->decoder_inst, dec_picture);
  } while (eos && (re != DEC_END_OF_STREAM) && (re != DEC_PIC_RDY) &&
           (re != DEC_OK));

  if(re == DEC_FLUSHED)
    re = Vp9DecNextPicture(h->decoder_inst, dec_picture);

  if (re != DEC_PIC_RDY) return -1;

  dec_pic_info->priv_pic_data = h->priv_pic_data;
  dec_pic_info->pic_id = dec_picture->pic_id;

  dec_pic_info->pic_width = dec_picture->frame_width;
  dec_pic_info->pic_height = dec_picture->frame_height;
  stride = ((u8 *)dec_picture->output_chroma_base -
            (u8 *)dec_picture->output_luma_base) / dec_picture->frame_height;

  if(stride != dec_picture->pic_stride) {
    HANTRO_LOG(HANTRO_LEVEL_WARNING, "un-matched stride: calc %d, get %d\n", 
	           stride, dec_picture->pic_stride);
  }

  dec_pic_info->pic_stride = dec_picture->pic_stride;
  dec_pic_info->pic_corrupt = 0;
  dec_pic_info->bit_depth = dec_picture->bit_depth_luma;
  dec_pic_info->crop_left = 0;
  dec_pic_info->crop_top = 0;
  dec_pic_info->crop_width = dec_picture->coded_width;
  dec_pic_info->crop_height = dec_picture->coded_height;
  dec_pic_info->output_picture_bus_address =
      dec_picture->output_luma_bus_address;
  dec_pic_info->output_picture_chroma_bus_address =
      dec_picture->output_chroma_bus_address;
  dec_pic_info->output_rfc_luma_bus_address = dec_picture->output_rfc_luma_bus_address;
  dec_pic_info->output_rfc_chroma_bus_address = dec_picture->output_rfc_chroma_bus_address;

  dec_pic_info->sar_width = 0;
  dec_pic_info->sar_height = 0;
  /* video signal info for HDR */
  dec_pic_info->video_range = 0;
  dec_pic_info->colour_description_present_flag = 0;
  dec_pic_info->colour_primaries = 0;
  dec_pic_info->transfer_characteristics = 0;
  dec_pic_info->matrix_coefficients = 0;
  dec_pic_info->chroma_loc_info_present_flag = 0;
  dec_pic_info->chroma_sample_loc_type_top_field = 0;
  dec_pic_info->chroma_sample_loc_type_bottom_field = 0;
  /* HDR10 metadata */
  dec_pic_info->present_flag = 0;
  dec_pic_info->red_primary_x = 0;
  dec_pic_info->red_primary_y = 0;
  dec_pic_info->green_primary_x = 0;
  dec_pic_info->green_primary_y = 0;
  dec_pic_info->blue_primary_x = 0;
  dec_pic_info->blue_primary_y = 0;
  dec_pic_info->white_point_x = 0;
  dec_pic_info->white_point_y = 0;
  dec_pic_info->max_mastering_luminance = 0;
  dec_pic_info->min_mastering_luminance = 0;
  dec_pic_info->max_content_light_level = 0;
  dec_pic_info->max_frame_average_light_level = 0;
  return 0;
}

#if 0
static u32 vp9_calc_osize(v4l2_dec_inst *h,
                          struct Vp9DecInfo *dec_info,
                          struct Vp9DecBufferInfo *hbuf) {
  VP9_PRIV_DATA *priv = (VP9_PRIV_DATA *)h->priv_data;
  u64 size = 0;
  h->srcwidth = MAX(h->srcwidth, MAX(dec_info->frame_width, dec_info->scaled_width));
  h->srcheight = MAX(h->srcheight, MAX(dec_info->frame_height, dec_info->scaled_height));
  size = (((u64)h->srcwidth + 15) & (~15)) * (((u64)h->srcheight + 7) & (~7));
  size *= (u64)hbuf->next_buf_size * 16 / dec_info->bit_depth;
  priv->pic_buff_size = (u32)(size / (h->dec_info.frame_width * h->dec_info.frame_height));
  return priv->pic_buff_size;
}
#endif

static u32 vp9_dec_check_res_change(v4l2_dec_inst *h,
                                    struct Vp9DecInfo *dec_info,
                                    struct Vp9DecBufferInfo *hbuf) {
  u32 res_change = 0;
  h->dec_info.bit_depth = dec_info->bit_depth;
  if ((dec_info->frame_width != h->dec_info.frame_width) ||
      (dec_info->frame_height != h->dec_info.frame_height) ||
      (dec_info->coded_width != h->dec_info.visible_rect.width) ||
      (dec_info->coded_height != h->dec_info.visible_rect.height) ||
      ((hbuf->buf_num + EXTRA_DPB_BUFFER_NUM) != h->dec_info.needed_dpb_nums) ||
      hbuf->next_buf_size != h->dec_info.dpb_buffer_size) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->frame_width, dec_info->frame_height);
    h->dec_info.frame_width = dec_info->frame_width;
    h->dec_info.frame_height = dec_info->frame_height;
    h->dec_info.visible_rect.width = dec_info->coded_width;
    h->dec_info.visible_rect.height = dec_info->coded_height;
    h->dec_info.src_pix_fmt = VSI_V4L2_DEC_PIX_FMT_NV12;
    h->dec_info.needed_dpb_nums = hbuf->buf_num + EXTRA_DPB_BUFFER_NUM;
#if 0
    h->dec_info.dpb_buffer_size = vp9_calc_osize(h, dec_info, hbuf);
#else
    h->dec_info.dpb_buffer_size = hbuf->next_buf_size;
#endif

    h->dec_info.pic_wstride = dec_info->pic_stride;
    res_change = 1;
  }

  return res_change;
}


/*
 * @brief vp9_dec_init(), vp9 decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp9_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  enum DecRet ret = DEC_OK;
  VP9_PRIV_DATA *priv;
  struct DWLInitParam dwl_init = {0};
  dwl_init.client_type = DWL_CLIENT_TYPE_VP9_DEC;
  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  h->priv_data = malloc(sizeof(VP9_PRIV_DATA));
  ASSERT(h->priv_data);
  memset(h->priv_data, 0, sizeof(VP9_PRIV_DATA));
  priv = (VP9_PRIV_DATA *)h->priv_data;

  struct Vp9DecConfig *dec_cfg = &(priv->dec_cfg);
#ifdef VSI_CMODEL
  dec_cfg->use_video_freeze_concealment = TBGetDecIntraFreezeEnable(&tb_cfg);
#endif

  dec_cfg->num_frame_buffers = VP9DEC_DPB_NUMS;
#ifdef HAS_FULL_DECFMT
  dec_cfg->use_video_compressor = 1;
#else
  dec_cfg->use_video_compressor = 0;
#endif
  dec_cfg->use_fetch_one_pic = 0;
  dec_cfg->use_ringbuffer = 0;
  dec_cfg->output_format =
      h->dec_output_fmt ? DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN;

  dec_cfg->pixel_format = h->dec_pixel_fmt;

  dec_cfg->dscale_cfg.down_scale_x = 1;
  dec_cfg->dscale_cfg.down_scale_y = 1;
  dec_cfg->use_secure_mode = h->secure_mode_on;
  dec_cfg->use_cts_test = 1;
  priv->set_new_info = 0;

  ret = Vp9DecInit((Vp9DecInst *)(&h->decoder_inst), h->dwl_inst, dec_cfg);
  if (ret != DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Vp9DecInit failed\n");
    goto end;
  }

  priv->dec_output_fmt = h->dec_output_fmt;
  priv->dec_pixel_fmt = dec_cfg->pixel_format;
  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief vpx_parse_superframeIndex(), parse super-frame index.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return
 */
static void vpx_parse_superframeIndex(v4l2_dec_inst *h) {
  u8 marker;
  u32 count = 0;
  VPX_SUPER_FRAME_INFO *sf_info =
      &(((VP9_PRIV_DATA *)h->priv_data)->super_frame_info);
  const u8 *data;
  size_t data_sz;
  u32 this_sz = 0, total_sz = 0;

  sf_info->is_superFrm = 0;

  data = (u8 *)h->stream_in_mem.virtual_address;
  data_sz = h->stream_in_mem.logical_size;

  marker = DWLPrivateAreaReadByte(data + data_sz - 1);

  sf_info->index_size = 0;

  if ((marker & 0xe0) == 0xc0) {
    const u32 frames = (marker & 0x7) + 1;
    const u32 mag = ((marker >> 3) & 0x3) + 1;
    sf_info->index_size = 2 + mag * frames;
    u8 index_value;

    if (data_sz >= sf_info->index_size) {
      const u8 *x = data + data_sz - sf_info->index_size;
      index_value = DWLPrivateAreaReadByte(x++);
      if (index_value == marker) {
        /* found a valid superframe index */
        u32 i, j;
        for (i = 0; i < frames; i++) {
          this_sz = 0;
          for (j = 0; j < mag; j++) {
            this_sz |= DWLPrivateAreaReadByte(x++) << (j * 8);
          }
          sf_info->size[i] = this_sz;
          sf_info->offset[i] = total_sz;
          total_sz += this_sz;
        }
        count = frames;
      }
      sf_info->is_superFrm = 1;
      sf_info->cur_frm_id = 0;
    }
  }
  sf_info->count = count;
}

/**
 * @brief vp9_dec_decode(), vp9 decoding function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp9_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  VP9_PRIV_DATA *priv = (VP9_PRIV_DATA *)h->priv_data;
  i32 ret = 0;
  VPX_SUPER_FRAME_INFO *sf_info =
      &(((VP9_PRIV_DATA *)h->priv_data)->super_frame_info);

  struct Vp9DecInfo dec_info = {0};
  enum DecRet tmp;
  struct Vp9DecBufferInfo hbuf = {0};

  struct Vp9DecInput dec_input = {0};
  struct Vp9DecOutput dec_output = {0};
  struct DWLLinearMem *in_mem = &h->stream_in_mem;
  Vp9DecInst dec_inst = h->decoder_inst;

  dec_input.stream = (u8 *)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;

  dec_input.buffer = dec_input.stream;
  dec_input.buffer_bus_address = ALIGN(dec_input.stream_bus_address);
  dec_input.buff_len =
      in_mem->logical_size + ALIGN_OFFSET(dec_input.stream_bus_address);

  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/

  if (h->dec_in_new_packet) {
    vpx_parse_superframeIndex(h);
  }

  if (sf_info->is_superFrm) {
    u32 offset = sf_info->offset[sf_info->cur_frm_id];
    dec_input.stream = (u8 *)in_mem->virtual_address + offset;
    dec_input.stream_bus_address = in_mem->bus_address + offset;
    dec_input.data_len = sf_info->size[sf_info->cur_frm_id];
  }

  do {
    ret = Vp9DecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Vp9DecDecode ret %d decoded pic %d\n", ret,
               h->dec_pic_id);

    switch (ret) {
      case DEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case DEC_HDRS_RDY:
        Vp9DecGetBufferInfo(dec_inst, &hbuf);
        tmp = Vp9DecGetInfo(dec_inst, &dec_info);
        if (tmp != DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

#if 0
            if(vp9_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
                 h->dec_add_buffer_allowed = 0;
                 re_dec_event = DEC_SOURCE_CHANGE_EVENT;
                 goto end;
            }
#else
        HANTRO_LOG(HANTRO_LEVEL_INFO,
                   "HDRS_RDY resolution: <%dx%d> -> <%dx%d>\n",
                   h->dec_info.frame_width, h->dec_info.frame_height,
                   dec_info.frame_width, dec_info.frame_height);
#endif
        break;

      case DEC_PIC_DECODED:
        /* case DEC_FREEZED_PIC_RDY: */
        /* Picture is now ready */
        h->dec_pic_id++;
        sf_info->cur_frm_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //dec_input.data_len = 0;
        //break;

      case DEC_PENDING_FLUSH:
        if (Vp9DecGetInfo(dec_inst, &dec_info) != DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        re_dec_event = DEC_PENDING_FLUSH_EVENT;  // DEC_PENDING_CONSUME_EVENT;
        goto end;

      case DEC_PIC_CONSUMED:
        sf_info->cur_frm_id++;
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
        //dec_input.data_len = 0;
        //break;

      case DEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Waiting for frame buffers\n");
#if 0
            h->dec_add_buffer_allowed = 1;
#else
        Vp9DecGetBufferInfo(dec_inst, &hbuf);
        /* Stream headers were successfully decoded
         * -> stream information is available for query now */
        if (Vp9DecGetInfo(dec_inst, &dec_info) != DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        if (vp9_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
          re_dec_event = DEC_SOURCE_CHANGE_EVENT;
          if (priv->set_new_info) {
              priv->set_new_info = 0;
          }
          goto end;
        }

#endif
        if (h->dec_info.needed_dpb_nums > h->existed_dpb_nums) {
          HANTRO_LOG(HANTRO_LEVEL_INFO,
                     "Mismatch: needed dpb num %d, existed dpb num %d\n",
                     h->dec_info.needed_dpb_nums, h->existed_dpb_nums);
          re_dec_event = DEC_WAIT_DECODING_BUFFER_EVENT;
          goto end;
        }

        if (h->dpb_buffer_added == 0) {
          vp9_dec_add_external_buffer(h);
        } else {
          goto end;
        }
        break;

      case DEC_OK:
        /* nothing to do, just call again */
        break;

      case DEC_NO_DECODING_BUFFER:
        ASSERT(dec_output.data_left);
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          vp9_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto end;
        }
      case DEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;

      case DEC_STRM_PROCESSED:
      case DEC_STRM_ERROR:
        if (dec_output.data_left == dec_input.data_len) {
          in_mem->logical_size = 0;
          goto end;
        }
        HANTRO_LOG(HANTRO_LEVEL_INFO, "NOT expected result: %d\n", ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;

      default:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "NOT expected result: %d\n", ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;
    }
  } while (vp9_dec_has_more_frames(h, &dec_input));

  in_mem->logical_size = 0;

  if ((ret == DEC_OK) || (ret == DEC_STRM_PROCESSED) ||
      (ret == DEC_PIC_DECODED) || (ret == DEC_PIC_CONSUMED)) {
    re_dec_event = DEC_EMPTY_EVENT;
    goto end;
  } else {
    re_dec_event = DEC_DECODING_ERROR_EVENT;
    goto end;
  }
error:
  if (dec_output.data_left == dec_input.data_len) {
    in_mem->logical_size = 0;
    goto end;
  }
update_input:
  h->consumed_len += dec_input.data_len;
  if (sf_info->is_superFrm) {
    if (sf_info->count > sf_info->cur_frm_id) {
      h->consumed_len += sf_info->index_size;
      goto end;
    }
  }
  in_mem->logical_size = 0;

end:
  if(ret != DEC_STRM_PROCESSED) {
    h->dec_inpkt_ignore_picconsumed_event = 0;
  }
  return re_dec_event;
}

/**
 * @brief vp9_dec_destroy(), vp9 decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp9_dec_destroy(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  if (h->priv_data) {
    free(h->priv_data);
    h->priv_data = NULL;
  }
  Vp9DecRelease(h->decoder_inst);

  DWLRelease(h->dwl_inst);

  return DEC_EMPTY_EVENT;
}

/**
 * @brief vp9_dec_drain(), vp9 draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return int: value of Vp9DecEndOfStream return.
 */
int vp9_dec_drain(v4l2_dec_inst *h) {
  enum DecRet ret = DEC_OK;
  ret = Vp9DecEndOfStream(h->decoder_inst);
  ASSERT(ret == DEC_OK);
  return ret;
}

/**
 * @brief vp9_dec_seek(), vp9 seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp9_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  enum DecRet ret = DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = Vp9DecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Vp9DecAbort returns %d\n", ret);
  ret = Vp9DecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(vp9, V4L2_DAEMON_CODEC_DEC_VP9, sizeof(struct Vp9DecPicture),
          DEC_ATTR_EOS_THREAD | DEC_ATTR_SUPPORT_SECURE_MODE);
