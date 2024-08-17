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
 * @file vsi_dec_vp8.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */

#include "vsi_dec.h"
#include "buffer_list.h"
#include "dec_dpb_buff.h"

#include "vp8decapi.h"

#ifdef VSI_CMODEL
#include "tb_cfg.h"
//#include "vp8hwd_container.h"
#endif

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif
static u32 webp_loop = 0;

// stubs function in case multi-frame in one package
static int vp8_dec_has_more_frames(v4l2_dec_inst* h, VP8DecInput* dec_input) {
  return dec_input->data_len ? 1 : 0;
}

static int vp8_dec_release_pic(v4l2_dec_inst* h, void* pic) {
  VP8DecRet ret;
  ret = VP8DecPictureConsumed(h->decoder_inst, (VP8DecPicture*)pic);

  ASSERT(ret == VP8DEC_OK);
  return (ret == VP8DEC_OK) ? 0 : -1;
}

static int vp8_dec_remove_buffer(v4l2_dec_inst* h) {
  VP8DecRet rv = VP8DEC_OK;
  rv = VP8DecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return VP8DEC_OK;
}

static int vp8_dec_add_buffer(v4l2_dec_inst* h, struct DWLLinearMem* mem_obj) {
  VP8DecRet rv = VP8DEC_OK;
  if (mem_obj) {
    rv = VP8DecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "VP8DecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == VP8DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == VP8DEC_OK || rv == VP8DEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

static void vp8_dec_add_external_buffer(v4l2_dec_inst* h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

static int vp8_dec_get_pic(v4l2_dec_inst* h, vsi_v4l2_dec_picture* dec_pic_info,
                           u32 eos) {
  VP8DecPicture* dec_picture = (VP8DecPicture*)h->priv_pic_data;
  VP8DecRet re = VP8DEC_OK;

  do {
    re = VP8DecNextPicture(h->decoder_inst, dec_picture, 0);
  } while (eos && (re != VP8DEC_END_OF_STREAM) && (re != VP8DEC_PIC_RDY) &&
           (re != VP8DEC_OK));

  if (re != VP8DEC_PIC_RDY) return -1;

  dec_pic_info->priv_pic_data = h->priv_pic_data;
  dec_pic_info->pic_id = dec_picture->pic_id;

  dec_pic_info->pic_width = dec_picture->frame_width;
  dec_pic_info->pic_height = dec_picture->frame_height;
  dec_pic_info->pic_stride = dec_picture->luma_stride;
  dec_pic_info->pic_corrupt = 0;
  dec_pic_info->bit_depth = 8;
  dec_pic_info->crop_left = 0;
  dec_pic_info->crop_top = 0;
  dec_pic_info->crop_width = dec_picture->coded_width;
  dec_pic_info->crop_height = dec_picture->coded_height;
  dec_pic_info->output_picture_bus_address =
      dec_picture->output_frame_bus_address;
  dec_pic_info->output_picture_chroma_bus_address =
      dec_picture->output_frame_bus_address_c;

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

static u32 vp8_dec_check_res_change(v4l2_dec_inst* h, VP8DecInfo* dec_info,
                                    VP8DecBufferInfo* hbuf) {
  u32 res_change = 0;
  h->dec_info.bit_depth = 8;
  if ((dec_info->frame_width != h->dec_info.frame_width) ||
      (dec_info->frame_height != h->dec_info.frame_height) ||
      (dec_info->coded_width != h->dec_info.visible_rect.width) ||
      (dec_info->coded_height != h->dec_info.visible_rect.height) ||
      ((hbuf->buf_num + EXTRA_DPB_BUFFER_NUM) != h->dec_info.needed_dpb_nums)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->frame_width, dec_info->frame_height);
    h->dec_info.frame_width = dec_info->frame_width;
    h->dec_info.frame_height = dec_info->frame_height;
    h->dec_info.visible_rect.width = dec_info->coded_width;
    h->dec_info.visible_rect.height = dec_info->coded_height;
    h->dec_info.src_pix_fmt = VSI_V4L2_DEC_PIX_FMT_NV12;
    h->dec_info.needed_dpb_nums = hbuf->buf_num + EXTRA_DPB_BUFFER_NUM;
    h->dec_info.dpb_buffer_size = hbuf->next_buf_size;
    h->dec_info.pic_wstride = h->dec_info.frame_width;
    res_change = 1;
  }

  return res_change;
}

/*
 * @brief vp8_dec_init(), vp8 decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp8_dec_init(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  VP8DecFormat dec_format;

  VP8DecApiVersion dec_api;
  VP8DecBuild dec_build;
  VP8DecRet re;
  enum DecDpbFlags flags = DEC_REF_FRM_RASTER_SCAN;

  /* Print API version number */
  dec_api = VP8DecGetAPIVersion();
  dec_build = VP8DecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 VP8 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VP8_DEC;
  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  // TBD: VP8 decoder can support VP7/VP8/WebP, need input info to identify it.
  dec_format = VP8DEC_VP8;

  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT)
    flags = DEC_REF_FRM_TILED_DEFAULT;

  re = VP8DecInit((VP8DecInst*)(&h->decoder_inst), h->dwl_inst, dec_format,
                  DEC_EC_NONE, 0, flags, 0, 0);
  if (re != VP8DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8DecInit failed\n");
    goto end;
  }

  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief vp8_dec_decode(), vp8 decoding function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp8_dec_decode(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;

  VP8DecInput dec_input = {0};
  VP8DecOutput dec_output = {0};
  VP8DecInfo dec_info;
  VP8DecRet tmp;
  VP8DecBufferInfo hbuf = {0};

  struct DWLLinearMem* in_mem = &h->stream_in_mem;
  VP8DecInst dec_inst = h->decoder_inst;

  dec_input.stream = (u8*)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/
  dec_input.slice_height = 0;          // TBD, for webP

  h->dec_last_pic_got = 0;

  do {
    ret = VP8DecDecode(dec_inst, &dec_input, &dec_output);

    HANTRO_LOG(HANTRO_LEVEL_INFO, "VP8DecDecode ret %d decoded pic %d\n", ret,
               h->dec_pic_id);

    switch (ret) {
      case VP8DEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case VP8DEC_HDRS_RDY:
        break;

      case VP8DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        in_mem->logical_size = 0;
        goto update_input;
        //dec_input.data_len = 0;
        //break;

      case VP8DEC_PIC_DECODED:
        /* case VP8DEC_FREEZED_PIC_RDY: */
        /* Picture is now ready */
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        in_mem->logical_size = 0;
        goto update_input;
        //dec_input.data_len = 0;
        //break;

      case VP8DEC_STRM_ERROR:
        dec_input.data_len = 0;
        break;

      case VP8DEC_STRM_PROCESSED:
        if (dec_output.data_left == 0) {
          dec_input.data_len = 0;
        }
        break;

      case VP8DEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO,
                   "Strm Processed or Waiting for frame buffers\n");
        VP8DecGetBufferInfo(dec_inst, &hbuf);
        tmp = VP8DecGetInfo(dec_inst, &dec_info);
        if (tmp != VP8DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        if (dec_info.coded_height == 0 || dec_info.coded_width == 0) {
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        if (vp8_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
          re_dec_event = DEC_SOURCE_CHANGE_EVENT;
          goto end;
        }

        if (h->dec_info.needed_dpb_nums > h->existed_dpb_nums) {
          HANTRO_LOG(HANTRO_LEVEL_INFO,
                     "Mismatch: needed dpb num %d, existed dpb num %d\n",
                     h->dec_info.needed_dpb_nums, h->existed_dpb_nums);
          re_dec_event = DEC_WAIT_DECODING_BUFFER_EVENT;
          goto end;
        }

        if (h->dpb_buffer_added == 0) {
          vp8_dec_add_external_buffer(h);
        } else {
          goto end;
        }
        break;

      case VP8DEC_OK:
        if (dec_output.data_left == 0) {
          dec_input.data_len = 0;
          break;
        }
        /* nothing to do, just call again */
        break;

      case VP8DEC_NO_DECODING_BUFFER:
        ASSERT(dec_output.data_left);
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          vp8_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto end;
        }

      // TBD:
      case VP8DEC_SLICE_RDY:
        webp_loop = 1;
        break;

      case VP8DEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto error;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto error;
    }
  } while (vp8_dec_has_more_frames(h, &dec_input));

  in_mem->logical_size = 0;

  if ((ret == VP8DEC_OK) || (ret == VP8DEC_STRM_PROCESSED) ||
      (ret == VP8DEC_PIC_DECODED) || (ret == VP8DEC_PIC_CONSUMED)) {
    re_dec_event = DEC_EMPTY_EVENT;
    goto end;
  } else {
    re_dec_event = DEC_DECODING_ERROR_EVENT;
    goto end;
  }
error:
  in_mem->logical_size = 0;
update_input:
  h->consumed_len += dec_input.data_len;
end:
  return re_dec_event;
}

/**
 * @brief vp8_dec_destroy(), vp8 decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp8_dec_destroy(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  VP8DecRelease(h->decoder_inst);

  DWLRelease(h->dwl_inst);

  return DEC_EMPTY_EVENT;
}

/**
 * @brief vp8_dec_drain(), vp8 draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return int: value of VP8DecEndOfStream return.
 */
int vp8_dec_drain(v4l2_dec_inst* h) {
  VP8DecRet ret = VP8DEC_OK;
  ret = VP8DecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == VP8DEC_OK || ret == VP8DEC_END_OF_STREAM);
  return ret;
}

/**
 * @brief vp8_dec_seek(), vp8 seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vp8_dec_seek(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  VP8DecRet ret = VP8DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = VP8DecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "VP8DecAbort returns %d\n", ret);
  ret = VP8DecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(vp8, V4L2_DAEMON_CODEC_DEC_VP8, sizeof(VP8DecPicture), DEC_ATTR_NONE);
