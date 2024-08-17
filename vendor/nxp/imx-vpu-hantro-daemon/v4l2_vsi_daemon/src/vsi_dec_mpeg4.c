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
 * @file vsi_dec_mpeg4.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */
#include <dlfcn.h>

#include "buffer_list.h"
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

#include "mp4decapi.h"

#ifdef VSI_CMODEL
#include "mp4dechwd_container.h"
#include "tb_cfg.h"
#endif

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif
#define DEFAULT_FILED_STATUS (0)
#define WAIT_SEC_FILED (1)
#define GOT_SEC_FILED (2)

typedef struct { u32 field_status; } mpeg4_dec_priv_data;

/**
 * @brief mpeg4_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int mpeg4_dec_release_pic(v4l2_dec_inst *h, void *pic) {
  MP4DecRet ret;
  ASSERT(pic);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "MP4DecPictureConsumed  bus address: %lx\n",
             ((MP4DecPicture *)pic)->output_picture_bus_address);
  ret = MP4DecPictureConsumed(h->decoder_inst, (MP4DecPicture *)pic);

  ASSERT(ret == MP4DEC_OK);
  return (ret == MP4DEC_OK) ? 0 : -1;
}
static int mpeg4_dec_remove_buffer(v4l2_dec_inst* h) {
  MP4DecRet rv = MP4DEC_OK;
  rv = MP4DecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}

/**
 * @brief mpeg4_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int mpeg4_dec_add_buffer(v4l2_dec_inst *h,
                                struct DWLLinearMem *mem_obj) {
  MP4DecRet rv = MP4DEC_OK;
  if (mem_obj) {
    rv = MP4DecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "MP4DecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == MP4DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == MP4DEC_OK || rv == MP4DEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief mpeg4_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void mpeg4_dec_add_external_buffer(v4l2_dec_inst *h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

/**
 * @brief mpeg4_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param u32 eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int mpeg4_dec_get_pic(v4l2_dec_inst *h,
                             vsi_v4l2_dec_picture *dec_pic_info, u32 eos) {
  mpeg4_dec_priv_data *mp4_priv = (mpeg4_dec_priv_data *)h->priv_data;
  MP4DecPicture *dec_picture = (MP4DecPicture *)h->priv_pic_data;
  MP4DecRet re = MP4DEC_OK;

  do {
    re = MP4DecNextPicture(h->decoder_inst, dec_picture, 0);
  } while (eos && (re != MP4DEC_END_OF_STREAM) && (re != MP4DEC_PIC_RDY) &&
           (re != MP4DEC_OK));

  if (re != MP4DEC_PIC_RDY) return -1;

  if (dec_picture->field_picture) {
    if (mp4_priv->field_status == DEFAULT_FILED_STATUS) {
      re = MP4DecNextPicture(h->decoder_inst, dec_picture, 0);
      if (re != MP4DEC_PIC_RDY) {
        mp4_priv->field_status = WAIT_SEC_FILED;
        return -1;
      }
    } else if (mp4_priv->field_status == WAIT_SEC_FILED) {
      mp4_priv->field_status = GOT_SEC_FILED;
    }
    mp4_priv->field_status = DEFAULT_FILED_STATUS;
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

  dec_pic_info->sar_width =
      dec_picture->coded_width;  // unsure the coded_width or the frame_width.
  dec_pic_info->sar_height = dec_picture->coded_height;
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
 * @brief mpeg4_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param MP4DecInfo* dec_info: new seq. header info.
 * @param MP4DecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static u32 mpeg4_dec_check_res_change(v4l2_dec_inst *h, MP4DecInfo *dec_info,
                                      MP4DecBufferInfo *buf_info) {
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
    h->dec_info.pic_wstride = h->dec_info.frame_width;
    res_change = 1;
  }

  return res_change;
}
/**
 * @brief mpeg4_update_input(), update in_mem & dec_input according to
 * dec_output info.
 * @param struct DWLLinearMem *in_mem.
 * @param MP4DecInput *dec_input.
 * @param MP4DecOutput *dec_output.
 * @return
 */
static void mpeg4_update_input(struct DWLLinearMem *in_mem,
                               MP4DecInput *dec_input,
                               MP4DecOutput *dec_output) {
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
  in_mem->virtual_address = (u32 *)dec_input->stream;
}

/**
 * @brief mpeg4_dec_init(), mpeg4 decoder init functioin
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event mpeg4_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  MP4DecApiVersion dec_api;
  MP4DecBuild dec_build;
  MP4DecRet re;
  u32 dpb_mode = DEC_DPB_FRAME;
  u32 ds_ratio_x = 0, ds_ratio_y = 0;
  struct DecDownscaleCfg dscale_cfg;

  /* Print API version number */
  dec_api = MP4DecGetAPIVersion();
  dec_build = MP4DecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 MPEG4 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_MPEG4_DEC;

  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  dscale_cfg.down_scale_x = ds_ratio_x;
  dscale_cfg.down_scale_y = ds_ratio_y;

  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT)
    dpb_mode = DEC_DPB_INTERLACED_FIELD;

  re =
      MP4DecInit((MP4DecInst *)(&h->decoder_inst), h->dwl_inst, MP4DEC_MPEG4, 0,
                 0, h->dec_output_fmt | (dpb_mode == DEC_DPB_INTERLACED_FIELD
                                             ? DEC_DPB_ALLOW_FIELD_ORDERING
                                             : 0),
                 0, 0, &dscale_cfg);

  if (re != MP4DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "MP4DecInit failed\n");
    goto end;
  }

#ifdef VSI_CMODEL
  if (TBGetDecRlcModeForced(&tb_cfg) != 0) {
    /*Force the decoder into RLC mode */
    ((DecContainer *)h->decoder_inst)->rlc_mode = 1;
  }
#endif

  h->priv_data = calloc(1, sizeof(mpeg4_dec_priv_data));
  ASSERT(h->priv_data);
  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief mpeg4_dec_decode(), mpeg4 decoding functioin
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event mpeg4_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;
  u32 skip_non_reference = 0;

  MP4DecInput dec_input = {0};
  MP4DecOutput dec_output = {0};
  MP4DecInfo dec_info;
  MP4DecBufferInfo hbuf = {0};
  MP4DecRet re_buf_info;

  struct DWLLinearMem *in_mem = &h->stream_in_mem;

  MP4DecInst dec_inst = h->decoder_inst;
  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream = (u8 *)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/

  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream, dec_input.stream_bus_address,
             dec_input.data_len);

  do {
    ret = MP4DecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "MP4DecDecode ret %d decoded pic %d, out_pid %d\n", ret,
               h->dec_pic_id, h->dec_out_pic_id);
    switch (ret) {
      case MP4DEC_STRM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case MP4DEC_HDRS_RDY:
      case MP4DEC_DP_HDRS_RDY:
        re_buf_info = MP4DecGetBufferInfo(dec_inst, &hbuf);
        HANTRO_LOG(HANTRO_LEVEL_INFO, "MP4DecGetBufferInfo ret %d\n",
                   re_buf_info);
        if (MP4DecGetInfo(dec_inst, &dec_info) != MP4DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        break;

      case MP4DEC_PIC_DECODED:
        if (MP4DecGetInfo(dec_inst, &dec_info) != MP4DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;

      case MP4DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
      case MP4DEC_STRM_PROCESSED:
      case MP4DEC_NONREF_PIC_SKIPPED:
      case MP4DEC_VOS_END:
        break;

      case MP4DEC_BUF_EMPTY:
      case MP4DEC_PARAM_ERROR:
      case MP4DEC_STRM_ERROR:
        if (dec_output.data_left == dec_input.data_len) {
          re_dec_event = DEC_DECODING_ERROR_EVENT;
          in_mem->logical_size = 0;
          h->consumed_len += dec_input.data_len;
          goto end;
        }
        // dec_output.data_left = 0;
        break;

      case MP4DEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Waiting for frame buffers\n");
        re_buf_info = MP4DecGetBufferInfo(dec_inst, &hbuf);
        HANTRO_LOG(HANTRO_LEVEL_INFO, "MP4DecGetBufferInfo ret %d\n",
                   re_buf_info);
        if (MP4DecGetInfo(dec_inst, &dec_info) != MP4DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto error;
        }

        if (mpeg4_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
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
          mpeg4_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }

        break;

      case MP4DEC_OK:
        /* nothing to do, just call again */
        break;

      case MP4DEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          mpeg4_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto end;
        }

      case MP4DEC_HW_TIMEOUT:
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

    mpeg4_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.data_len > 0);

  in_mem->logical_size = 0;

  if ((ret != MP4DEC_OK) && (ret != MP4DEC_STRM_PROCESSED) &&
      (ret != MP4DEC_NONREF_PIC_SKIPPED) && (ret != MP4DEC_PIC_DECODED)) {
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
  mpeg4_update_input(in_mem, &dec_input, &dec_output);

end:
  return re_dec_event;
}

/**
 * @brief mpeg4_dec_destroy(), mpeg4 decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event mpeg4_dec_destroy(v4l2_dec_inst *h,
                                      vsi_v4l2_msg *v4l2_msg) {
  if (h->priv_data) {
    free(h->priv_data);
    h->priv_data = NULL;
  }
  MP4DecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish \n");
  return DEC_EMPTY_EVENT;
}

/**
 * @brief mpeg4_dec_drain(), mpeg4 draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @return int: value of MP4DecEndOfStream return.
 */
int mpeg4_dec_drain(v4l2_dec_inst *h) {
  MP4DecRet ret = MP4DEC_OK;
  ret = MP4DecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == MP4DEC_OK);
  return ret;
}

/**
 * @brief mpeg4_dec_seek(), mpeg4 seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event mpeg4_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  MP4DecRet ret = MP4DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = MP4DecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "MP4DecAbort returns %d\n", ret);
  ret = MP4DecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(mpeg4, V4L2_DAEMON_CODEC_DEC_MPEG4, sizeof(MP4DecPicture),
          DEC_ATTR_NONE);  // vsidaemon_mpeg4_v4l2m2m_decoder
