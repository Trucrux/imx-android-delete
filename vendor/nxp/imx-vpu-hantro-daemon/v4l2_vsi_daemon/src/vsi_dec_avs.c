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
 * @file vsi_dec_avs.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */
#include <dlfcn.h>

#include "buffer_list.h"
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

#include "avsdecapi.h"

#ifdef VSI_CMODEL
#include "tb_cfg.h"
#endif

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

/**
 * @brief avs_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int avs_dec_release_pic(v4l2_dec_inst* h, void* pic) {
  AvsDecRet ret;
  ASSERT(pic);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "AvsDecPictureConsumed  bus address: %lx\n",
             ((AvsDecPicture*)pic)->output_picture_bus_address);
  ret = AvsDecPictureConsumed(h->decoder_inst, (AvsDecPicture*)pic);
  ASSERT(ret == AVSDEC_OK);
  return (ret == AVSDEC_OK) ? 0 : -1;
}
static int avs_dec_remove_buffer(v4l2_dec_inst* h) {
  AvsDecRet rv = AVSDEC_OK;
  rv = AvsDecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}

/**
 * @brief avs_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int avs_dec_add_buffer(v4l2_dec_inst* h, struct DWLLinearMem* mem_obj) {
  AvsDecRet rv = AVSDEC_OK;
  if (mem_obj) {
    rv = AvsDecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "AvsDecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == AVSDEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == AVSDEC_OK || rv == AVSDEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief avs_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void avs_dec_add_external_buffer(v4l2_dec_inst* h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

/**
 * @brief avs_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param uint32_t eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int avs_dec_get_pic(v4l2_dec_inst* h, vsi_v4l2_dec_picture* dec_pic_info,
                           uint32_t eos) {
  AvsDecPicture* dec_picture = (AvsDecPicture*)h->priv_pic_data;
  AvsDecRet re = AVSDEC_OK;
  do {
    re = AvsDecNextPicture(h->decoder_inst, dec_picture, eos);
  } while (eos && (re != AVSDEC_END_OF_STREAM) && (re != AVSDEC_PIC_RDY) &&
           (re != AVSDEC_OK));
  if (AVSDEC_PIC_RDY != re) return -1;
  if (h->dec_interlaced_sequence && dec_picture->first_field) {
    re = AvsDecNextPicture(h->decoder_inst, dec_picture, eos);
    if (re != AVSDEC_PIC_RDY) return -1;
  }

  AvsDecInfo dec_info = {0};
  AvsDecGetInfo(h->decoder_inst, &dec_info);
  dec_pic_info->priv_pic_data = h->priv_pic_data;
  dec_pic_info->pic_id = dec_picture->pic_id;

  dec_pic_info->pic_width = dec_picture->frame_width;
  dec_pic_info->pic_height = dec_picture->frame_height;
  dec_pic_info->pic_stride = dec_picture->frame_width;
  dec_pic_info->pic_corrupt = 0;
  dec_pic_info->bit_depth = 8;
  dec_pic_info->crop_left = 0;
  dec_pic_info->crop_top = 0;
  dec_pic_info->crop_width = dec_picture->coded_width;
  dec_pic_info->crop_height = dec_picture->coded_height;
  dec_pic_info->output_picture_bus_address =
      dec_picture->output_picture_bus_address;

  dec_pic_info->output_picture_chroma_bus_address =
      dec_picture->output_picture_bus_address +
      dec_pic_info->pic_stride * dec_pic_info->pic_width;

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

/**
 * @brief avs_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param AvsDecInfo* dec_info: new seq. header info.
 * @param AvsDecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static uint32_t avs_dec_check_res_change(v4l2_dec_inst* h, AvsDecInfo* dec_info,
                                         AvsDecBufferInfo* buf_info) {
  uint32_t res_change = 0;
  h->dec_info.bit_depth = 8;
  if (dec_info->frame_width != h->dec_info.frame_width ||
      dec_info->frame_height != h->dec_info.frame_height ||
      dec_info->coded_width != h->dec_info.visible_rect.width ||
      dec_info->coded_height != h->dec_info.visible_rect.height ||
      ((buf_info->buf_num + EXTRA_DPB_BUFFER_NUM) !=
       h->dec_info.needed_dpb_nums)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->frame_width, dec_info->frame_height);
    h->dec_info.frame_width = dec_info->frame_width;
    h->dec_info.frame_height = dec_info->frame_height;
    h->dec_info.visible_rect.width = dec_info->coded_width;
    h->dec_info.visible_rect.height = dec_info->coded_height;

    h->dec_info.visible_rect.left = 0;
    h->dec_info.visible_rect.top = 0;

    h->dec_info.src_pix_fmt = VSI_V4L2_DEC_PIX_FMT_NV12;
    h->dec_info.needed_dpb_nums = buf_info->buf_num + EXTRA_DPB_BUFFER_NUM;
    h->dec_info.dpb_buffer_size = buf_info->next_buf_size;
    h->dec_interlaced_sequence = dec_info->interlaced_sequence;
    h->dec_info.pic_wstride = h->dec_info.frame_width;
    res_change = 1;
  }

  return res_change;
}

/**
 * @brief avs_update_input(), update in_mem & dec_input according to dec_output
 * info.
 * @param struct DWLLinearMem *in_mem.
 * @param AvsDecInput *dec_input.
 * @param AvsDecOutput *dec_output.
 * @return .
 */
static void avs_update_input(struct DWLLinearMem* in_mem,
                             AvsDecInput* dec_input, AvsDecOutput* dec_output) {
  dec_input->data_len = dec_output->data_left;
  dec_input->stream = dec_output->strm_curr_pos;
  dec_input->stream_bus_address = dec_output->strm_curr_bus_address;
  in_mem->logical_size = dec_input->data_len;
  in_mem->bus_address = dec_input->stream_bus_address;
  in_mem->virtual_address = (u32*)dec_input->stream;
}

/**
 * @brief avs_dec_init(), avs decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event avs_dec_init(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  AvsDecApiVersion dec_api;
  AvsDecBuild dec_build;
  AvsDecRet re;
  struct DecDownscaleCfg dscale_cfg;
  enum DecDpbFlags flags = 0;
  u32 ds_ratio_x = 0, ds_ratio_y = 0;
  /* Print API version number */
  dec_api = AvsDecGetAPIVersion();
  dec_build = AvsDecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 Avs Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_AVS_DEC;

  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT) {
    flags |= DEC_REF_FRM_TILED_DEFAULT;
    flags |= DEC_DPB_ALLOW_FIELD_ORDERING;
  }

  dscale_cfg.down_scale_x = ds_ratio_x;
  dscale_cfg.down_scale_y = ds_ratio_y;

  re = AvsDecInit((AvsDecInst*)(&h->decoder_inst), h->dwl_inst, DEC_EC_NONE, 0,
                  flags, 0, 0, &dscale_cfg);
  if (re != AVSDEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "AvsDecInit failed\n");
    goto end;
  }
  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief avs_dec_decode(), avs decoding function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event avs_dec_decode(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;
  u32 skip_non_reference = 0;

  AvsDecInput dec_input = {0};
  AvsDecOutput dec_output = {0};
  AvsDecInfo dec_info;
  AvsDecBufferInfo hbuf = {0};

  struct DWLLinearMem* in_mem = &h->stream_in_mem;

  AvsDecInst dec_inst = h->decoder_inst;
  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream = (u8*)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/

  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream, dec_input.stream_bus_address,
             dec_input.data_len);

  do {
    ret = AvsDecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "AvsDecDecode ret %d decoded pic %d, out_pid %d\n", ret,
               h->dec_pic_id, h->dec_out_pic_id);
    switch (ret) {
      case AVSDEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case AVSDEC_HDRS_RDY:
        AvsDecGetBufferInfo(dec_inst, &hbuf);
        if (AvsDecGetInfo(dec_inst, &dec_info) != AVSDEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        break;

      case AVSDEC_PIC_DECODED:
        if (AvsDecGetInfo(dec_inst, &dec_info) != AVSDEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;

      case AVSDEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
      case AVSDEC_STRM_PROCESSED:
      case AVSDEC_NONREF_PIC_SKIPPED:
        break;

      case AVSDEC_BUF_EMPTY:
      case AVSDEC_STRM_ERROR:
        if (dec_output.data_left == dec_input.data_len) {
          re_dec_event = DEC_DECODING_ERROR_EVENT;
          in_mem->logical_size = 0;
          h->consumed_len += dec_input.data_len;
          goto end;
        }
        break;

      case AVSDEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Waiting for frame buffers\n");

        // h->dec_add_buffer_allowed = 1;
        AvsDecGetBufferInfo(dec_inst, &hbuf);
        if (AvsDecGetInfo(dec_inst, &dec_info) != AVSDEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto error;
        }

        if (avs_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
          re_dec_event = DEC_SOURCE_CHANGE_EVENT;
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
          avs_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }
        break;

      case AVSDEC_OK:
        /* nothing to do, just call again */
        break;

      case AVSDEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          avs_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto update_input;
        }

      case AVSDEC_HW_TIMEOUT:
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

    avs_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.data_len > 0);

  in_mem->logical_size = 0;

  if ((ret != AVSDEC_OK) && (ret != AVSDEC_STRM_PROCESSED) &&
      (ret != AVSDEC_NONREF_PIC_SKIPPED) && (ret != AVSDEC_PIC_DECODED)) {
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
  avs_update_input(in_mem, &dec_input, &dec_output);

end:
  return re_dec_event;
}

/**
 * @brief avs_dec_destroy(), avs decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event avs_dec_destroy(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  AvsDecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish \n");
  return DEC_EMPTY_EVENT;
}

/**
 * @brief avs_dec_drain(), avs draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @return int: value of AvsDecEndOfStream return.
 */
int avs_dec_drain(v4l2_dec_inst* h) {
  AvsDecRet ret = AVSDEC_OK;
  ret = AvsDecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == AVSDEC_OK);
  return ret;
}

/**
 * @brief avs_dec_seek(), avs seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event avs_dec_seek(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  AvsDecRet ret = AVSDEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = AvsDecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "AvsDecAbort returns %d\n", ret);
  ret = AvsDecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(avs, V4L2_DAEMON_CODEC_DEC_AVS2, sizeof(AvsDecPicture),
          DEC_ATTR_NONE);  // vsidaemon_avs_v4l2m2m_decoder
