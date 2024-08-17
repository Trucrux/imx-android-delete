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
 * @file vsi_dec_vc1.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */
#include <dlfcn.h>

#include "buffer_list.h"
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

#include "vc1decapi.h"

#ifdef VSI_CMODEL
#include "deccfg.h"
#include "tb_cfg.h"
#include "vc1hwd_container.h"
#endif

#define SHOW1(p) \
  (p[0]);        \
  p += 1;
#define SHOW2(p)        \
  (p[0]) | (p[1] << 8); \
  p += 2;
#define SHOW3(p)                       \
  (p[0]) | (p[1] << 8) | (p[2] << 16); \
  p += 3;
#define SHOW4(p)                                      \
  (p[0]) | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); \
  p += 4;
#define VC1L_FILE_HEADER_SIZE (9 * 4)
#define FAKE_DECODER_INST (-1)
#define VC1_IS_NAL_HDR(data, offset)                           \
  ((data[offset + 0] == 0x00) && (data[offset + 1] == 0x00) && \
   (data[offset + 2] == 0x01))

static u32 ds_ratio_x, ds_ratio_y;
static u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
static u32 dpb_mode = DEC_DPB_FRAME;

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

static const u8 *DecodeRCV(const u8 *stream, u32 strm_len,
                           VC1DecMetaData *meta_data) \
{
  u32 tmp1 = 0;
  u8 *p;
  u32 rcv_metadata_size = 0;
  p = (u8 *)stream;

  if (strm_len >= 20 /*RCV Seq Hdr length*/) {
    // standard RCV sequence Header
    tmp1 = SHOW3(p);
    tmp1 = SHOW1(p);
    rcv_metadata_size = SHOW4(p);

    if ((tmp1 & 0x85) != 0x85 || rcv_metadata_size != 0x4) {
      // not a RCV Seq Hdr.
      return stream;
    }

    /* Decode image dimensions */
    p += rcv_metadata_size;
    tmp1 = SHOW4(p);
    meta_data->max_coded_height = tmp1;
    tmp1 = SHOW4(p);
    meta_data->max_coded_width = tmp1;

    return (stream + 8);
  }

  return stream;
}

/**
 * @brief vc1_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int vc1_dec_release_pic(v4l2_dec_inst *h, void *pic) {
  VC1DecRet ret;
  ASSERT(pic);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "VC1DecPictureConsumed  bus address: %lx\n",
             ((VC1DecPicture *)pic)->output_picture_bus_address);
  ret = VC1DecPictureConsumed(h->decoder_inst, (VC1DecPicture *)pic);

  ASSERT(ret == VC1DEC_OK);
  return (ret == VC1DEC_OK) ? 0 : -1;
}
static int vc1_dec_remove_buffer(v4l2_dec_inst *h) {
  VC1DecRet rv = VC1DEC_OK;
  rv = VC1DecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}

/**
 * @brief vc1_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int vc1_dec_add_buffer(v4l2_dec_inst *h, struct DWLLinearMem *mem_obj) {
  VC1DecRet rv = VC1DEC_OK;
  if (mem_obj) {
    rv = VC1DecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "VC1DecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == VC1DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == VC1DEC_OK || rv == VC1DEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief vc1_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void vc1_dec_add_external_buffer(v4l2_dec_inst *h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

/**
 * @brief vc1_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param u32 eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int vc1_dec_get_pic(v4l2_dec_inst *h, vsi_v4l2_dec_picture *dec_pic_info,
                           u32 eos) {
  VC1DecPicture *dec_picture = (VC1DecPicture *)h->priv_pic_data;
  VC1DecRet re = VC1DEC_OK;

  do {
    re = VC1DecNextPicture(h->decoder_inst, dec_picture, 0);
  } while (eos && (re != VC1DEC_END_OF_STREAM) && (re != VC1DEC_PIC_RDY) &&
           (re != VC1DEC_OK));
  if (re != VC1DEC_PIC_RDY) return -1;

  if (h->dec_interlaced_sequence && dec_picture->first_field) {
    re = VC1DecNextPicture(h->decoder_inst, dec_picture, 0);
    if (re != VC1DEC_PIC_RDY) return -1;
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

  dec_pic_info->sar_width = dec_picture->coded_width;
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
 * @brief vc1_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param VC1DecInfo* dec_info: new seq. header info.
 * @param VC1DecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static u32 vc1_dec_check_res_change(v4l2_dec_inst *h, VC1DecInfo *dec_info,
                                    VC1DecBufferInfo *buf_info) {
  u32 res_change = 0;
  h->dec_info.bit_depth = 8;
  if ((NEXT_MULTIPLE(dec_info->max_coded_width, 16) !=
       h->dec_info.frame_width) ||
      (NEXT_MULTIPLE(dec_info->max_coded_height, 16) !=
       h->dec_info.frame_height) ||
      ((buf_info->buf_num + EXTRA_DPB_BUFFER_NUM) !=
       h->dec_info.needed_dpb_nums)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "VC1 Max codec w/h Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->max_coded_width, dec_info->max_coded_height);
    h->dec_info.frame_width = NEXT_MULTIPLE(dec_info->max_coded_width, 16);
    h->dec_info.frame_height = NEXT_MULTIPLE(dec_info->max_coded_height, 16);
    h->dec_info.visible_rect.left = 0;
    h->dec_info.visible_rect.width = dec_info->max_coded_width;
    h->dec_info.visible_rect.top = 0;
    h->dec_info.visible_rect.height = dec_info->max_coded_height;
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
 * @brief vc1l_update_input(), update in_mem & dec_input according to dec_output
 * info for vc1 base/main profile.
 * @param struct DWLLinearMem *in_mem.
 * @param VC1DecInput *dec_input.
 * @param VC1DecOutput *dec_output.
 * @return .
 */
static void vc1l_update_input(struct DWLLinearMem *in_mem,
                              VC1DecInput *dec_input,
                              VC1DecOutput *dec_output) {
  if (dec_output->data_left) {
    // dec_input->stream_bus_address += (dec_output->p_stream_curr_pos -
    // dec_input->stream);
    // dec_input->stream_size = dec_output->data_left;
    // dec_input->stream = dec_output->p_stream_curr_pos;
  } else {
    dec_input->stream_size = dec_output->data_left;
    // dec_input->stream = dec_output->p_stream_curr_pos;
    // dec_input->stream_bus_address = dec_output->strm_curr_bus_address;
  }
  in_mem->logical_size = dec_input->stream_size;
  in_mem->bus_address = dec_input->stream_bus_address;
  in_mem->virtual_address = (u32 *)dec_input->stream;
}

/**
 * @brief vc1l_dec_init(), vc1 decoder init function for vc1 base/main profile.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vc1l_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  VC1DecApiVersion dec_api;
  VC1DecBuild dec_build;
  /* Print API version number */
  dec_api = VC1DecGetAPIVersion();
  dec_build = VC1DecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 VC1 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VC1_DEC;
  h->decoder_inst = (void *)FAKE_DECODER_INST;
  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);
  return DEC_EMPTY_EVENT;
}

/**
 * @brief vc1l_dec_decode(), vc1 decoding function for base/main profile.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vc1l_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;
  u32 skip_non_reference = 0;

  VC1DecInput dec_input = {0};
  VC1DecOutput dec_output = {0};
  VC1DecInfo dec_info = {0};
  VC1DecBufferInfo hbuf = {0};

  struct DWLLinearMem *in_mem = &h->stream_in_mem;
  VC1DecInst dec_inst = h->decoder_inst;
  dec_input.stream = (u8 *)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream_size = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/
  if (h->decoder_inst == (void *)FAKE_DECODER_INST) {
    const u8 *tmp_strm = dec_input.stream;
    const u8 *p_meta_data = NULL;
    struct DecDownscaleCfg dscale_cfg = {0};
    dscale_cfg.down_scale_x = ds_ratio_x;
    dscale_cfg.down_scale_y = ds_ratio_y;
    VC1DecRet re;
    enum DecDpbFlags flags = 0;
    u32 num_frame_buffers = 0;
    VC1DecMetaData meta_data = {0};

    p_meta_data = DecodeRCV(tmp_strm, dec_input.stream_size, &meta_data);
    VC1DecUnpackMetaData(p_meta_data, 4, &meta_data);
    if (meta_data.max_coded_width == 0 || meta_data.max_coded_height == 0) {
      meta_data.max_coded_width =
          v4l2_msg->params.dec_params.io_buffer.srcwidth;
      meta_data.max_coded_height =
          v4l2_msg->params.dec_params.io_buffer.srcheight;
    }
    if (tiled_output) flags |= DEC_REF_FRM_TILED_DEFAULT;
    if (dpb_mode == DEC_DPB_INTERLACED_FIELD)
      flags |= DEC_DPB_ALLOW_FIELD_ORDERING;

    re = VC1DecInit((VC1DecInst *)(&h->decoder_inst), h->dwl_inst, &meta_data,
#ifdef VSI_CMODEL
                    TBGetDecErrorConcealment(&tb_cfg),
#else
                    DEC_EC_NONE,
#endif
                    num_frame_buffers, flags, 0, 0, &dscale_cfg);

    if (re != VC1DEC_OK) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "VC1DecInit failed\n");
      goto end;
    }
    in_mem->logical_size = 0;
    dec_inst = h->decoder_inst;
    goto end;
  }

  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream, dec_input.stream_bus_address,
             dec_input.stream_size);
  do {
    ret = VC1DecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "VC1DecDecode ret %d decoded pic %d, out_pid %d\n", ret,
               h->dec_pic_id, h->dec_out_pic_id);
    switch (ret) {
      case VC1DEC_FORMAT_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case VC1DEC_RESOLUTION_CHANGED:
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting dec info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        HANTRO_LOG(HANTRO_LEVEL_INFO, "New res w/h <%dx%d>\n",
                   dec_info.coded_width, dec_info.coded_height);
        break;

      case VC1DEC_HDRS_RDY:
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        if ((dec_info.max_coded_width > h->dec_info.frame_width) ||
            (dec_info.max_coded_height > h->dec_info.frame_height)) {
          HANTRO_LOG(HANTRO_LEVEL_WARNING,
                     "daemon get larger framed w/h <%dx%d>-><%dx%d>\n",
                     h->dec_info.frame_width, h->dec_info.frame_height,
                     dec_info.max_coded_width, dec_info.max_coded_height);
        }
        break;

      case VC1DEC_WAITING_FOR_BUFFER:
        VC1DecGetBufferInfo(dec_inst, &hbuf);
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting dec info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        if (vc1_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
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
          vc1_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }
        break;

      case VC1DEC_PIC_DECODED:
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;

      case VC1DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
        //break;

      case VC1DEC_END_OF_SEQ:
        dec_input.stream_size = 0;
        break;

      case VC1DEC_STRM_PROCESSED:
      case VC1DEC_NONREF_PIC_SKIPPED:
        // dec_output.data_left = 0;
        break;

      case VC1DEC_OK:
        /* nothing to do, just call again */
        break;

      case VC1DEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          vc1_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto end;
        }

      case VC1DEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;
    }

    h->consumed_len += dec_input.stream_size - dec_output.data_left;

    if (dec_input.stream_size == 0) break;

    vc1l_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.stream_size > 0);

  in_mem->logical_size = 0;

  if ((ret != VC1DEC_OK) && (ret != VC1DEC_STRM_PROCESSED) &&
      (ret != VC1DEC_NONREF_PIC_SKIPPED) && (ret != VC1DEC_PIC_DECODED) &&
      (ret != VC1DEC_PIC_CONSUMED)) {
    re_dec_event = DEC_DECODING_ERROR_EVENT;
    goto end;
  } else {
    re_dec_event = DEC_EMPTY_EVENT;
    goto end;
  }

error:
  if (dec_output.data_left == dec_input.stream_size) {
    in_mem->logical_size = 0;
    goto end;
  }

update_input:
  h->consumed_len += dec_input.stream_size - dec_output.data_left;
  vc1l_update_input(in_mem, &dec_input, &dec_output);

end:
  return re_dec_event;
}

/**
 * @brief vc1g_update_input(), update in_mem & dec_input according to dec_output
 * info for vc1 advance profile.
 * @param struct DWLLinearMem *in_mem.
 * @param VC1DecInput *dec_input.
 * @param VC1DecOutput *dec_output.
 * @return .
 */
static void vc1g_update_input(struct DWLLinearMem *in_mem,
                              VC1DecInput *dec_input,
                              VC1DecOutput *dec_output) {
  if (dec_output->data_left) {
    dec_input->stream_bus_address +=
        (dec_output->p_stream_curr_pos - dec_input->stream);
    dec_input->stream_size = dec_output->data_left;
    dec_input->stream = dec_output->p_stream_curr_pos;
  } else {
    dec_input->stream_size = dec_output->data_left;
    dec_input->stream = dec_output->p_stream_curr_pos;
    dec_input->stream_bus_address = dec_output->strm_curr_bus_address;
  }
  in_mem->logical_size = dec_input->stream_size;
  in_mem->bus_address = dec_input->stream_bus_address;
  in_mem->virtual_address = (u32 *)dec_input->stream;
}

/**
 * @brief vc1g_dec_init(), vc1 decoder init function for advance profile.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vc1g_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  VC1DecApiVersion dec_api;
  VC1DecBuild dec_build;
  struct DecDownscaleCfg dscale_cfg;
  VC1DecRet re;
  /* Print API version number */
  dec_api = VC1DecGetAPIVersion();
  dec_build = VC1DecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 VC1 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VC1_DEC;

  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  dscale_cfg.down_scale_x = ds_ratio_x;
  dscale_cfg.down_scale_y = ds_ratio_y;
  enum DecDpbFlags flags = 0;
  u32 num_frame_buffers = 0;
  VC1DecMetaData meta_data = {0};
  meta_data.profile = 8;

  if (tiled_output) flags |= DEC_REF_FRM_TILED_DEFAULT;
  if (dpb_mode == DEC_DPB_INTERLACED_FIELD)
    flags |= DEC_DPB_ALLOW_FIELD_ORDERING;

  re = VC1DecInit((VC1DecInst *)(&h->decoder_inst), h->dwl_inst, &meta_data,
#ifdef VSI_CMODEL
                  TBGetDecErrorConcealment(&tb_cfg),
#else
                  DEC_EC_NONE,
#endif
                  num_frame_buffers, flags, 0, 0, &dscale_cfg);

  if (re != VC1DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VC1DecInit failed\n");
    goto end;
  }
  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief vc1g_dec_decode(), vc1 decoding function for advance profile.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vc1g_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;
  u32 skip_non_reference = 0;
  VC1DecInput dec_input = {0};
  VC1DecOutput dec_output = {0};
  VC1DecInfo dec_info = {0};
  VC1DecBufferInfo hbuf = {0};

  struct DWLLinearMem *in_mem = &h->stream_in_mem;
  VC1DecInst dec_inst = h->decoder_inst;
  dec_input.stream = (u8 *)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream_size = in_mem->logical_size;
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/
  if (VC1_IS_NAL_HDR(dec_input.stream, 0) ||
      VC1_IS_NAL_HDR(dec_input.stream, 1)) {
    VC1DecSetFrameDataMode(dec_inst, 0);
  } else {
    if (h->dec_in_new_packet)
      VC1DecSetFrameDataMode(dec_inst, 1);
  }
  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream, dec_input.stream_bus_address,
             dec_input.stream_size);
  do {
    ret = VC1DecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "VC1DecDecode ret %d decoded pic %d, out_pid %d\n", ret,
               h->dec_pic_id, h->dec_out_pic_id);
    switch (ret) {
      case VC1DEC_FORMAT_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case VC1DEC_RESOLUTION_CHANGED:
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting dec info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        HANTRO_LOG(HANTRO_LEVEL_INFO, "New res w/h <%dx%d>\n",
                   dec_info.coded_width, dec_info.coded_height);
        break;

      case VC1DEC_HDRS_RDY:
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        if ((dec_info.max_coded_width > h->dec_info.frame_width) ||
            (dec_info.max_coded_height > h->dec_info.frame_height)) {
          HANTRO_LOG(HANTRO_LEVEL_WARNING,
                     "daemon get larger framed w/h <%dx%d>-><%dx%d>\n",
                     h->dec_info.frame_width, h->dec_info.frame_height,
                     dec_info.max_coded_width, dec_info.max_coded_height);
        }
        break;

      case VC1DEC_WAITING_FOR_BUFFER:
        VC1DecGetBufferInfo(dec_inst, &hbuf);
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting dec info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        if (vc1_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
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
          vc1_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }
        break;

      case VC1DEC_PIC_DECODED:
        if (VC1DecGetInfo(dec_inst, &dec_info) != VC1DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;

      case VC1DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;
        //break;

      case VC1DEC_END_OF_SEQ:
        dec_input.stream_size = 0;
        break;

      case VC1DEC_STRM_PROCESSED:
      case VC1DEC_NONREF_PIC_SKIPPED:
        // dec_output.data_left = 0;
        break;

      case VC1DEC_OK:
        /* nothing to do, just call again */
        break;

      case VC1DEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          vc1_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto end;
        }

      case VC1DEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;
    }

    h->consumed_len += dec_input.stream_size - dec_output.data_left;

    if (dec_input.stream_size == 0) break;

    vc1g_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.stream_size > 0);

  in_mem->logical_size = 0;

  if ((ret != VC1DEC_OK) && (ret != VC1DEC_STRM_PROCESSED) &&
      (ret != VC1DEC_NONREF_PIC_SKIPPED) && (ret != VC1DEC_PIC_DECODED) &&
      (ret != VC1DEC_PIC_CONSUMED)) {
    re_dec_event = DEC_DECODING_ERROR_EVENT;
    goto end;
  } else {
    re_dec_event = DEC_EMPTY_EVENT;
    goto end;
  }

error:
  if (dec_output.data_left == dec_input.stream_size) {
    in_mem->logical_size = 0;
    goto end;
  }
update_input:
  h->consumed_len += dec_input.stream_size - dec_output.data_left;
  vc1g_update_input(in_mem, &dec_input, &dec_output);

end:
  return re_dec_event;
}

/**
 * @brief vc1_dec_destroy(), vc1 decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vc1_dec_destroy(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  VC1DecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);

  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish \n");
  return DEC_EMPTY_EVENT;
}

/**
 * @brief vc1_dec_drain(), vc1 draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @return int: value of VC1DecEndOfStream return.
 */
int vc1_dec_drain(v4l2_dec_inst *h) {
  VC1DecRet ret = VC1DEC_OK;
  ret = VC1DecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == VC1DEC_OK);
  return ret;
}

/**
 * @brief vc1_dec_seek(), vc1 seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event vc1_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  VC1DecRet ret = VC1DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = VC1DecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "VC1DecAbort returns %d\n", ret);
  ret = VC1DecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

vsi_v4l2m2m2_deccodec vsidaemon_vc1l_v4l2m2m_decoder = {
    .name = "vc1l_v4l2m2m",
    .codec_id = V4L2_DAEMON_CODEC_DEC_VC1_L,
    .pic_ctx_size = sizeof(VC1DecPicture),
    .init = vc1l_dec_init,
    .decode = vc1l_dec_decode,
    .destroy = vc1_dec_destroy,
    .drain = vc1_dec_drain,
    .seek = vc1_dec_seek,
    .get_pic = vc1_dec_get_pic,
    .release_pic = vc1_dec_release_pic,
    .add_buffer = vc1_dec_add_buffer,
    .remove_buffer = vc1_dec_remove_buffer,
    .capabilities = 1,
    .wrapper_name = "vsiv4l2daemon",
    .attributes = DEC_ATTR_NONE,
};

vsi_v4l2m2m2_deccodec vsidaemon_vc1g_v4l2m2m_decoder = {
    .name = "vc1g_v4l2m2m",
    .codec_id = V4L2_DAEMON_CODEC_DEC_VC1_G,
    .pic_ctx_size = sizeof(VC1DecPicture),
    .init = vc1g_dec_init,
    .decode = vc1g_dec_decode,
    .destroy = vc1_dec_destroy,
    .drain = vc1_dec_drain,
    .seek = vc1_dec_seek,
    .get_pic = vc1_dec_get_pic,
    .release_pic = vc1_dec_release_pic,
    .add_buffer = vc1_dec_add_buffer,
    .remove_buffer = vc1_dec_remove_buffer,
    .capabilities = 1,
    .wrapper_name = "vsiv4l2daemon",
    .attributes = DEC_ATTR_NONE,
};
