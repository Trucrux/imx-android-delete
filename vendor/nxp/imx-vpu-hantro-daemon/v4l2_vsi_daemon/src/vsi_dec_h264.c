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
 * @file vsi_dec_h264.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */
#include <dlfcn.h>

#include "buffer_list.h"
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

#include "h264decapi.h"

#ifdef VSI_CMODEL
#include "tb_cfg.h"
//#include "h264hwd_container.h"
#include "deccfg.h"
#endif

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

static u32 ds_ratio_x, ds_ratio_y;
static u32 dpb_mode = DEC_DPB_FRAME;

v4l2_inst_dec_event h264_dec_seek(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg);

/**
 * @brief h264_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int h264_dec_release_pic(v4l2_dec_inst* h, void* pic) {
  H264DecRet ret;
  ASSERT(pic);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "H264DecPictureConsumed  bus address: %lx\n",
             ((H264DecPicture*)pic)->output_picture_bus_address);
  ret = H264DecPictureConsumed(h->decoder_inst, (H264DecPicture*)pic);

  ASSERT(ret == H264DEC_OK);
  return (ret == H264DEC_OK) ? 0 : -1;
}

static int h264_dec_remove_buffer(v4l2_dec_inst* h) {
  H264DecRet rv = H264DEC_OK;
  rv = H264DecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}

/**
 * @brief h264_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int h264_dec_add_buffer(v4l2_dec_inst* h, struct DWLLinearMem* mem_obj) {
  H264DecRet rv = H264DEC_OK;
  if (mem_obj) {
    rv = H264DecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "H264DecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == H264DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == H264DEC_OK || rv == H264DEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief h264_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void h264_dec_add_external_buffer(v4l2_dec_inst* h) {
#if 1
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
#else
  H264DecRet rv = H264DEC_OK;
  struct DWLLinearMem* mem;
  for (int i = 0; i < h->existed_dpb_nums; i++) {
    mem = dpb_get_buffer(h, i);
    if (mem) {
      rv = H264DecAddBuffer(h->decoder_inst, mem);
      HANTRO_LOG(HANTRO_LEVEL_INFO,
                 "H264DecAddBuffer rv = %d, %p, %p bus address: %lx\n", rv,
                 h->decoder_inst, mem, mem->bus_address);
    }
  }
#endif
  h->dpb_buffer_added = 1;
}

/**
 * @brief h264_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param uint32_t eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int h264_dec_get_pic(v4l2_dec_inst* h,
                            vsi_v4l2_dec_picture* dec_pic_info, uint32_t eos) {
  H264DecPicture* dec_picture = (H264DecPicture*)h->priv_pic_data;
  H264DecRet re = H264DEC_OK;
#if 1
  re = H264DecNextPicture(h->decoder_inst, dec_picture, 0);

  if (H264DEC_PIC_RDY != re) {
    if (eos) {
      do {
        re = H264DecNextPicture(h->decoder_inst, dec_picture, 0);
      } while (re != H264DEC_END_OF_STREAM && re != H264DEC_PIC_RDY &&
               (re == H264DEC_OK && h->dec_aborted == 0));
      if (re == H264DEC_END_OF_STREAM) {
        h264_dec_seek(h, NULL);
        h->dec_aborted = 1;
      }
    }
    if (H264DEC_PIC_RDY != re) return -1;
  }
#else

  do {
    re = H264DecNextPicture(h->decoder_inst, dec_picture, 0);
  } while (eos && re != H264DEC_END_OF_STREAM && re != H264DEC_PIC_RDY);
  if (H264DEC_PIC_RDY != re) return -1;
#endif
  H264DecInfo dec_info = {0};
  H264DecGetInfo(h->decoder_inst, &dec_info);
  dec_pic_info->priv_pic_data = h->priv_pic_data;
  dec_pic_info->pic_id = dec_picture->pic_id;

  dec_pic_info->pic_width = dec_picture->pic_width;
  dec_pic_info->pic_height = dec_picture->pic_height;
  dec_pic_info->pic_stride = dec_picture->pic_width;
  dec_pic_info->pic_corrupt = 0;
  dec_pic_info->bit_depth = 8;
  dec_pic_info->crop_left = dec_picture->crop_params.crop_left_offset;
  dec_pic_info->crop_top = dec_picture->crop_params.crop_top_offset;
  dec_pic_info->crop_width = dec_picture->crop_params.crop_out_width;
  dec_pic_info->crop_height = dec_picture->crop_params.crop_out_height;
  dec_pic_info->output_picture_bus_address =
      dec_picture->output_picture_bus_address;

  dec_pic_info->output_picture_chroma_bus_address =
      (dec_info.mono_chrome == 1)
          ? (vpu_addr_t)0
          : (dec_picture->output_picture_bus_address +
             dec_picture->pic_stride * dec_picture->pic_height);

  dec_pic_info->sar_width = dec_picture->sar_width;
  dec_pic_info->sar_height = dec_picture->sar_height;
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

/**
 * @brief h264_dec_get_vui_info(), get vui info.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param H264DecInfo* dec_info: new seq. header info.
 * @return .
 */
static void h264_dec_get_vui_info(v4l2_dec_inst* h, H264DecInfo* dec_info) {
  h->dec_info.colour_description_present_flag =
      dec_info->colour_description_present_flag;
  h->dec_info.matrix_coefficients = dec_info->matrix_coefficients;
  h->dec_info.colour_primaries = dec_info->colour_primaries;
  h->dec_info.transfer_characteristics = dec_info->transfer_characteristics;
  h->dec_info.video_range = dec_info->video_range;
}

/**
 * @brief h264_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param H264DecInfo* dec_info: new seq. header info.
 * @param H264DecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static uint32_t h264_dec_check_res_change(v4l2_dec_inst* h,
                                          H264DecInfo* dec_info,
                                          H264DecBufferInfo* buf_info) {
  uint32_t res_change = 0;
  h->dec_info.bit_depth = 8;
  if ((dec_info->pic_width != h->dec_info.frame_width) ||
      (dec_info->pic_height != h->dec_info.frame_height) ||
      (dec_info->crop_params.crop_left_offset !=
       h->dec_info.visible_rect.left) ||
      (dec_info->crop_params.crop_out_width !=
       h->dec_info.visible_rect.width) ||
      (dec_info->crop_params.crop_top_offset != h->dec_info.visible_rect.top) ||
      (dec_info->crop_params.crop_out_height !=
       h->dec_info.visible_rect.height) ||
      ((buf_info->buf_num + EXTRA_DPB_BUFFER_NUM) !=
       h->dec_info.needed_dpb_nums)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->pic_width, dec_info->pic_height);
    h->dec_info.frame_width = dec_info->pic_width;
    h->dec_info.frame_height = dec_info->pic_height;
    h->dec_info.visible_rect.left = dec_info->crop_params.crop_left_offset;
    h->dec_info.visible_rect.width = dec_info->crop_params.crop_out_width;
    h->dec_info.visible_rect.top = dec_info->crop_params.crop_top_offset;
    h->dec_info.visible_rect.height = dec_info->crop_params.crop_out_height;
    h->dec_info.src_pix_fmt = (dec_info->output_format != H264DEC_YUV400)
                                  ? VSI_V4L2_DEC_PIX_FMT_NV12
                                  : VSI_V4L2_DEC_PIX_FMT_400;
    h->dec_info.needed_dpb_nums = buf_info->buf_num + EXTRA_DPB_BUFFER_NUM;
    h->dec_info.dpb_buffer_size = buf_info->next_buf_size;
    h->dec_info.pic_wstride = h->dec_info.frame_width;
    res_change = 1;
  }

  return res_change;
}

/**
 * @brief h264_update_input(), update in_mem & dec_input according to dec_output
 * info.
 * @param struct DWLLinearMem *in_mem.
 * @param H264DecInput *dec_input.
 * @param H264DecOutput *dec_output.
 * @return .
 */
static void h264_update_input(struct DWLLinearMem* in_mem,
                              H264DecInput* dec_input,
                              H264DecOutput* dec_output) {
  if (dec_output->data_left) {
    dec_input->stream_bus_address +=
        (dec_output->strm_curr_pos - dec_input->stream);
    dec_input->data_len = dec_output->data_left;
    dec_input->stream = dec_output->strm_curr_pos;
  } else {
    dec_input->data_len = dec_output->data_left;
    dec_input->stream = dec_output->strm_curr_pos;
    dec_input->stream_bus_address = dec_output->strm_curr_bus_address;
  }
  in_mem->logical_size = dec_input->data_len;
  in_mem->bus_address = dec_input->stream_bus_address;
  in_mem->virtual_address = (u32*)dec_input->stream;
}

/**
 * @brief h264_dec_init(), h264 decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event h264_dec_init(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  u32 use_display_smoothing = 0;

  H264DecApiVersion dec_api;
  H264DecBuild dec_build;
  H264DecRet re;
  struct DecDownscaleCfg dscale_cfg;
  enum DecDpbFlags flags = 0;
  /* Print API version number */
  dec_api = H264DecGetAPIVersion();
  dec_build = H264DecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 H.264 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_H264_DEC;

  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT) {
    flags |= DEC_REF_FRM_TILED_DEFAULT;
    flags |= DEC_DPB_ALLOW_FIELD_ORDERING;
    dpb_mode = DEC_DPB_INTERLACED_FIELD;
  }

  dscale_cfg.down_scale_x = ds_ratio_x;
  dscale_cfg.down_scale_y = ds_ratio_y;

  re = H264DecInit((H264DecInst*)(&h->decoder_inst), h->dwl_inst,
                   h->dec_no_reordering, DEC_EC_NONE, use_display_smoothing,
                   flags, 0, 0, h->secure_mode_on, &dscale_cfg);
  if (re != H264DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264DecInit failed\n");
    goto end;
  }

  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief h264_dec_decode(), h264 decoding function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event h264_dec_decode(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;
  u32 skip_non_reference = 0;

  H264DecInput dec_input = {0};
  H264DecOutput dec_output = {0};
  H264DecInfo dec_info;
  H264DecBufferInfo hbuf = {0};
  H264DecConfig dec_cfg = {0};

  struct DWLLinearMem* in_mem = &h->stream_in_mem;

  H264DecInst dec_inst = h->decoder_inst;
  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream = (u8*)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/

  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream, dec_input.stream_bus_address,
             dec_input.data_len);

  h->dec_aborted = 0;
  do {
    ret = H264DecDecode(dec_inst, &dec_input, &dec_output);
    h->discard_dbp_count += H264DecDiscardDpbNums(dec_inst);
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "H264DecDecode ret %d decoded pic %d, out_pid %d\n", ret,
               h->dec_pic_id, h->dec_out_pic_id);
    switch (ret) {
      case H264DEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case H264DEC_HDRS_RDY:
        H264DecGetBufferInfo(dec_inst, &hbuf);
        if (H264DecGetInfo(dec_inst, &dec_info) != H264DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
#if 0
            if(h264_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
                re_dec_event = DEC_SOURCE_CHANGE_EVENT;
                h->dec_add_buffer_allowed = 0;
                goto update_input;
             }
#endif
        dec_cfg.dpb_flags = 0;
        if (h->dec_output_fmt) dec_cfg.dpb_flags |= DEC_REF_FRM_TILED_DEFAULT;

        if (dpb_mode == DEC_DPB_INTERLACED_FIELD)
          dec_cfg.dpb_flags |= DEC_DPB_ALLOW_FIELD_ORDERING;
        dec_cfg.use_adaptive_buffers = 0;
        dec_cfg.guard_size = 0;
        dec_cfg.use_secure_mode = 0;
        if (H264DEC_OK !=
            H264DecSetInfo((H264DecInst)(h->decoder_inst), &dec_cfg)) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264DecSetInfo failed\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h264_dec_get_vui_info(h, &dec_info);
        break;

      case H264DEC_ADVANCED_TOOLS:
        ASSERT(dec_output.data_left);
        break;

      case H264DEC_PIC_DECODED:
        if (H264DecGetInfo(dec_inst, &dec_info) != H264DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;

      case H264DEC_PENDING_FLUSH:
        if (H264DecGetInfo(dec_inst, &dec_info) != H264DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        /* Reset buffers added and stop adding extra buffers when wh changed. */
        /*if((dec_info.pic_width != h->dec_info.frame_width) ||
                (dec_info.pic_height != h->dec_info.frame_height)) */ {
          /* ctrlsw needs application to take away all of decoded pictures,
              otherwise, xxxDecDecode() will wait output-empty locally.
              So, we shouldn't check if resolution is identical or not here.*/
          re_dec_event = DEC_PENDING_FLUSH_EVENT;
          goto update_input;
        }
        break;

      case H264DEC_FIELD_DECODED:
        HANTRO_LOG(HANTRO_LEVEL_DEBUG, "Field stream, data left %d\n",
                   dec_output.data_left);
        break;

      case H264DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
      case H264DEC_STRM_PROCESSED:
      case H264DEC_NONREF_PIC_SKIPPED:
        break;

      case H264DEC_BUF_EMPTY:
      case H264DEC_STRM_ERROR:
        if (dec_output.data_left == dec_input.data_len) {
          re_dec_event = DEC_DECODING_ERROR_EVENT;
          in_mem->logical_size = 0;
          h->consumed_len += dec_input.data_len;
          goto end;
        }
        break;

      case H264DEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Waiting for frame buffers\n");

        // h->dec_add_buffer_allowed = 1;
        H264DecGetBufferInfo(dec_inst, &hbuf);
        if (H264DecGetInfo(dec_inst, &dec_info) != H264DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        if (h264_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
          re_dec_event = DEC_SOURCE_CHANGE_EVENT;
          //                h->consumed_len += dec_input.data_len -
          //                dec_output.data_left;
          goto update_input;
        }
        if (h->dec_info.needed_dpb_nums > h->existed_dpb_nums) {
          HANTRO_LOG(HANTRO_LEVEL_INFO,
                     "Mismatch: needed dpb num %d, existed dpb num %d\n",
                     h->dec_info.needed_dpb_nums, h->existed_dpb_nums);
          re_dec_event = DEC_WAIT_DECODING_BUFFER_EVENT;
          goto end;
        }

        if (h->dpb_buffer_added == 0) {
          h264_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }
        break;

      case H264DEC_OK:
        /* nothing to do, just call again */
        break;

      case H264DEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          h264_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto update_input;
        }

      case H264DEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;
    }

    h->consumed_len += dec_input.data_len - dec_output.data_left;

    /* break out of do-while if max_num_pics reached (data_len set to 0) */
    if (dec_input.data_len == 0) break;

    h264_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.data_len > 0);

  in_mem->logical_size = 0;

  if ((ret != H264DEC_OK) && (ret != H264DEC_STRM_PROCESSED) &&
      (ret != H264DEC_NONREF_PIC_SKIPPED) && (ret != H264DEC_PIC_DECODED) &&
      (ret != H264DEC_FIELD_DECODED)) {
    re_dec_event = DEC_DECODING_ERROR_EVENT;
    goto end;
  } else {
    re_dec_event = DEC_EMPTY_EVENT;
    goto end;
  }
error:
  if (dec_output.data_left == dec_input.data_len) {
    in_mem->logical_size = 0;
    goto end;
  }
update_input:
  h->consumed_len += dec_input.data_len - dec_output.data_left;
  h264_update_input(in_mem, &dec_input, &dec_output);

end:
  if(ret != H264DEC_STRM_PROCESSED) {
    h->dec_inpkt_ignore_picconsumed_event = 0;
  }
  return re_dec_event;
}

/**
 * @brief h264_dec_destroy(), h264 decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event h264_dec_destroy(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  H264DecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);

  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish \n");
  return DEC_EMPTY_EVENT;
}

/**
 * @brief h264_dec_drain(), h264 draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @return int: value of H264DecEndOfStream return.
 */
int h264_dec_drain(v4l2_dec_inst* h) {
  H264DecRet ret = H264DEC_OK;
  ret = H264DecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == H264DEC_OK);
  return ret;
}

/**
 * @brief h264_dec_seek(), h264 seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event h264_dec_seek(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  H264DecRet ret = H264DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = H264DecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "H264DecAbort returns %d\n", ret);
  ret = H264DecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

// vsidaemon_h264_v4l2m2m_decoder
VSIM2MDEC(h264, V4L2_DAEMON_CODEC_DEC_H264, sizeof(H264DecPicture),
          DEC_ATTR_EOS_THREAD | DEC_ATTR_SUPPORT_SECURE_MODE);
