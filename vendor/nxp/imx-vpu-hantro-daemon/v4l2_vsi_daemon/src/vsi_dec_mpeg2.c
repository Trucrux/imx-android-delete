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
 * @file vsi_dec_mpeg2.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */

#include "vsi_dec.h"
#include "buffer_list.h"
#include "dec_dpb_buff.h"

#include "mpeg2decapi.h"

#ifdef VSI_CMODEL
#include "mpeg2hwd_container.h"
#include "regdrv_g1.h"
#include "tb_cfg.h"
#endif

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

#if 0  // obsolete functions
static void * (*G1DWLInit)(struct DWLInitParam * param) = NULL;
static i32 (*G1DWLRelease)(const void *instance) = NULL;

static void mpeg2_dec_dlsym_priv_func(v4l2_dec_inst* h)
{
    ASSERT(h->dec_dll_handle);
    G1DWLInit = dlsym(h->dec_dll_handle, "DWLInit");
    G1DWLRelease = dlsym(h->dec_dll_handle, "DWLRelease");
    ASSERT(G1DWLInit);
    ASSERT(G1DWLRelease);
}
#endif

/**
 * @brief mpeg2_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int mpeg2_dec_release_pic(v4l2_dec_inst* h, void* pic) {
  Mpeg2DecRet ret = MPEG2DEC_OK;
  ASSERT(pic);
  ret = Mpeg2DecPictureConsumed(h->decoder_inst, (Mpeg2DecPicture*)pic);

  ASSERT(ret == MPEG2DEC_OK);
  return (ret == MPEG2DEC_OK) ? 0 : -1;
}

static int mpeg2_dec_remove_buffer(v4l2_dec_inst* h) {
  Mpeg2DecRet rv = MPEG2DEC_OK;
  rv = Mpeg2DecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}

static int mpeg2_dec_add_buffer(v4l2_dec_inst* h,
                                struct DWLLinearMem* mem_obj) {
  Mpeg2DecRet rv = MPEG2DEC_OK;
  if (mem_obj) {
    rv = Mpeg2DecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Mpeg2DecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == MPEG2DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == MPEG2DEC_OK || rv == MPEG2DEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief mpeg2_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void mpeg2_dec_add_external_buffer(v4l2_dec_inst* h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

/**
 * @brief mpeg2_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param u32 eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int mpeg2_dec_get_pic(v4l2_dec_inst* h,
                             vsi_v4l2_dec_picture* dec_pic_info, u32 eos) {
  Mpeg2DecPicture* dec_picture = (Mpeg2DecPicture*)h->priv_pic_data;
  Mpeg2DecRet re = MPEG2DEC_OK;

  do {
    re = Mpeg2DecNextPicture(h->decoder_inst, dec_picture, 0);
  } while (eos && (re != MPEG2DEC_END_OF_STREAM) && (re != MPEG2DEC_PIC_RDY) &&
           (re != MPEG2DEC_OK));

  if (re != MPEG2DEC_PIC_RDY) return -1;

  if (h->dec_interlaced_sequence) {
    if (dec_picture->output_other_field) {
      re = Mpeg2DecNextPicture(h->decoder_inst, dec_picture, 0);
      if (re != MPEG2DEC_PIC_RDY) return -1;
    }
  }

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
      dec_picture->frame_width * dec_picture->frame_height;

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
 * @brief mpeg2_dec_get_vui_info(), get vui info.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param Mpeg2DecInfo* dec_info: new seq. header info.
 * @return .
 */
static void mpeg2_dec_get_vui_info(v4l2_dec_inst* h, Mpeg2DecInfo* dec_info) {
  h->dec_info.colour_description_present_flag =
      dec_info->colour_description_present_flag;
  h->dec_info.matrix_coefficients = dec_info->matrix_coefficients;
  h->dec_info.colour_primaries = dec_info->colour_primaries;
  h->dec_info.transfer_characteristics = dec_info->transfer_characteristics;
  h->dec_info.video_range = dec_info->video_range;
}

/**
 * @brief mpeg2_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param Mpeg2DecInfo* dec_info: new seq. header info.
 * @param Mpeg2DecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static u32 mpeg2_dec_check_res_change(v4l2_dec_inst* h, Mpeg2DecInfo* dec_info,
                                      Mpeg2DecBufferInfo* buf_info) {
  u32 res_change = 0;
  h->dec_info.bit_depth = 8;
  if ((dec_info->frame_width != h->dec_info.frame_width) ||
      (dec_info->frame_height != h->dec_info.frame_height) ||
      (dec_info->coded_width != h->dec_info.visible_rect.width) ||
      (dec_info->coded_height != h->dec_info.visible_rect.height) ||
      ((buf_info->buf_num + EXTRA_DPB_BUFFER_NUM) !=
       h->dec_info.needed_dpb_nums)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->frame_width, dec_info->frame_height);
    h->dec_info.frame_width = dec_info->frame_width;
    h->dec_info.frame_height = dec_info->frame_height;
    h->dec_info.visible_rect.width = dec_info->coded_width;
    h->dec_info.visible_rect.height = dec_info->coded_height;
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
 * @brief mpeg2_update_input(), update in_mem & dec_input according to
 * dec_output info.
 * @param struct DWLLinearMem *in_mem.
 * @param Mpeg2DecInput *dec_input.
 * @param Mpeg2DecOutput *dec_output.
 * @return .
 */
static void mpeg2_update_input(struct DWLLinearMem* in_mem,
                               Mpeg2DecInput* dec_input,
                               Mpeg2DecOutput* dec_output) {
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
  in_mem->virtual_address = (u32*)dec_input->stream;
  in_mem->bus_address = dec_input->stream_bus_address;
}

/**
 * @brief mpeg2_dec_init(), mpeg2 decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event mpeg2_dec_init(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  Mpeg2DecRet ret;
  struct DecDownscaleCfg dscale_cfg;
  u32 num_frame_buffers = 0;
  u32 dpb_mode = DEC_DPB_FRAME;
  struct DWLInitParam dwl_init;

  dwl_init.client_type = DWL_CLIENT_TYPE_MPEG2_DEC;

  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  dscale_cfg.down_scale_x = 0;
  dscale_cfg.down_scale_y = 0;
  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT)
    dpb_mode = DEC_DPB_INTERLACED_FIELD;

  ret = Mpeg2DecInit((Mpeg2DecInst*)(&h->decoder_inst), h->dwl_inst, 0,
                     num_frame_buffers,
                     h->dec_output_fmt | (dpb_mode == DEC_DPB_INTERLACED_FIELD
                                              ? DEC_DPB_ALLOW_FIELD_ORDERING
                                              : 0),
                     0, 0, &dscale_cfg);
  if (MPEG2DEC_OK != ret) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Mpeg2DecInit failed\n");
    goto end;
  }

  return DEC_EMPTY_EVENT;

end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief mpeg2_dec_decode(), mpeg decoding function.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event:event for state machine.
 */
v4l2_inst_dec_event mpeg2_dec_decode(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  u32 skip_non_reference = 0;

  Mpeg2DecRet ret = MPEG2DEC_OK;
  Mpeg2DecInput dec_input = {0};
  Mpeg2DecOutput dec_output = {0};
  Mpeg2DecInfo dec_info = {0};
  Mpeg2DecBufferInfo hbuf = {0};

  struct DWLLinearMem* in_mem = &h->stream_in_mem;
  Mpeg2DecInst dec_inst = h->decoder_inst;

  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream = (u8*)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/

  do {
    ret = Mpeg2DecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Mpeg2DecDecode ret %d decoded pic %d\n", ret,
               h->dec_pic_id);

    switch (ret) {
      case MPEG2DEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case MPEG2DEC_HDRS_RDY:
#ifdef VSI_CMODEL
        TBSetRefbuMemModel(&tb_cfg, ((DecContainer*)dec_inst)->mpeg2_regs,
                           &((DecContainer*)dec_inst)->ref_buffer_ctrl);
#endif

        Mpeg2DecGetBufferInfo(dec_inst, &hbuf);
        if (Mpeg2DecGetInfo(dec_inst, &dec_info) != MPEG2DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
#if 0
            if(mpeg2_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
                 re_dec_event = DEC_SOURCE_CHANGE_EVENT;
                 goto update_input;
             }
#endif
        mpeg2_dec_get_vui_info(h, &dec_info);
        break;

      case MPEG2DEC_PIC_DECODED:
        if (Mpeg2DecGetInfo(dec_inst, &dec_info) != MPEG2DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;
      case MPEG2DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
        //break;
      case MPEG2DEC_NONREF_PIC_SKIPPED:
      case MPEG2DEC_STRM_PROCESSED:
        break;
      case MPEG2DEC_BUF_EMPTY:
        if (dec_output.data_left == dec_input.data_len) {
          re_dec_event = DEC_DECODING_ERROR_EVENT;
          in_mem->logical_size = 0;
          h->consumed_len += dec_input.data_len;
          goto end;
        }
        break;

      case MPEG2DEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Waiting for frame buffers\n");
        Mpeg2DecGetBufferInfo(dec_inst, &hbuf);
        if (Mpeg2DecGetInfo(dec_inst, &dec_info) != MPEG2DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto error;
        }

        if (mpeg2_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
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
          mpeg2_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }
        break;

      case MPEG2DEC_OK:
        break;

      case MPEG2DEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          mpeg2_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto update_input;
        }

      case MPEG2DEC_PARAM_ERROR:
      case MPEG2DEC_STRM_ERROR:
      case MPEG2DEC_HW_BUS_ERROR:
        HANTRO_LOG(HANTRO_LEVEL_WARNING, "INCORRECT STREAM PARAMS\n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case MPEG2DEC_END_OF_STREAM:
        re_dec_event = DEC_GOT_EOS_MARK_EVENT;
        goto end;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;
    }

    h->consumed_len += dec_input.data_len - dec_output.data_left;

    /* break out of do-while if max_num_pics reached (data_len set to 0) */
    if (dec_input.data_len == 0) break;

    mpeg2_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.data_len > 0);
  in_mem->logical_size = 0;
  if ((ret == MPEG2DEC_OK) || (ret == MPEG2DEC_STRM_PROCESSED) ||
      (ret == MPEG2DEC_NONREF_PIC_SKIPPED) || (ret == MPEG2DEC_PIC_DECODED)) {
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
  h->consumed_len += dec_input.data_len - dec_output.data_left;
  mpeg2_update_input(in_mem, &dec_input, &dec_output);
end:
  return re_dec_event;
}

/**
 * @brief mpeg2_dec_destroy(), mpeg decoder destory function.
 * @param v4l2_dec_inst* inst: daemon instance (IN).
 * @param vsi_v4l2_msg* v4l2_msg: output buffer which send to v4l2-driver (IN).
 * @return v4l2_inst_dec_event:state event
 */
v4l2_inst_dec_event mpeg2_dec_destroy(v4l2_dec_inst* h,
                                      vsi_v4l2_msg* v4l2_msg) {
  Mpeg2DecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);

  return DEC_EMPTY_EVENT;
}

/**
 * @brief mpeg2_dec_drain(), mpeg decoder draining function.
 * @param v4l2_dec_inst* inst: daemon instance (IN).
 * @return int: value of Mpeg2DecEndOfStream return.
 */
int mpeg2_dec_drain(v4l2_dec_inst* h) {
  Mpeg2DecRet ret = MPEG2DEC_OK;
  ret = Mpeg2DecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == MPEG2DEC_OK);
  return ret;
}

/**
 * @brief mpeg2_dec_seek(), mpeg decoder seek function.
 * @param v4l2_dec_inst* inst: daemon instance (IN).
 * @param vsi_v4l2_msg* v4l2_msg: output buffer which send to v4l2-driver (IN).
 * @return v4l2_inst_dec_event:state event
 */
v4l2_inst_dec_event mpeg2_dec_seek(v4l2_dec_inst* h, vsi_v4l2_msg* v4l2_msg) {
  Mpeg2DecRet ret = MPEG2DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = Mpeg2DecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Mpeg2DecAbort returns %d\n", ret);
  ret = Mpeg2DecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(mpeg2, V4L2_DAEMON_CODEC_DEC_MPEG2, sizeof(Mpeg2DecPicture),
          DEC_ATTR_NONE);  // vsidaemon_mpeg2_v4l2m2m_decoder
