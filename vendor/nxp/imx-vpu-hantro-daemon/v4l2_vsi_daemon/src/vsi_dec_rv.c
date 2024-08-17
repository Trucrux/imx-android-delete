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
 * @file vsi_dec_rv.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */

#include <dlfcn.h>

#include "buffer_list.h"
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

#include "rvdecapi.h"

#ifdef VSI_CMODEL
#include "deccfg.h"
#include "tb_cfg.h"
#endif

#define FAKE_DECODER_INST (-1)
#define FRAME_BUFFERS (0)
#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

#define ERROR_MODE (-1)
#define RV_MODE 0
#define RAW_MODE 1
#define RV_SLICE_DATA_ALIGN 1
#define RV_MAX_SLICE_NUM 128

typedef struct RV_PRIV_DATA {
  u32 stream_mode;
  u32 total_slice_info_size;
  u32 slice_info_num;
  RvDecSliceInfo slice_info[128];
} RV_PRIV_DATA;

static i32 rv_get_stream_mode(struct DWLLinearMem *in_mem) {
  u8 *buff = (u8 *)in_mem->virtual_address;
  if (strncmp((const char *)(buff + 8), "RV30", 4) == 0 ||
      (strncmp((const char *)(buff + 8), "RV40", 4) == 0)) {
    return RV_MODE;
  } else {
    return RAW_MODE;
  }
}

static u32 rv_dec_parse_sliceinfo(struct DWLLinearMem *in_mem,
                                  RvDecSliceInfo *slice_info) {
  u32 slice_info_num = 0, frame_length = 0, hdr_bytes = 0;
  u32 i;
  u8 *buff = NULL;

  if (RV_MODE == rv_get_stream_mode(in_mem)) {
        u8 *frame_hdr_p = (u8 *)in_mem->virtual_address;
        u32 frame_hdr_length = (frame_hdr_p[0] << 24) | (frame_hdr_p[1] << 16) | (frame_hdr_p[2] << 8) | (frame_hdr_p[3] << 0);
        if (frame_hdr_length >= in_mem->logical_size) {
            HANTRO_LOG(HANTRO_LEVEL_DEBUG, "repeatedly transfer codec data\n");
            return 0;
        }
        in_mem->logical_size -= frame_hdr_length;
        in_mem->virtual_address = (u32 *)((u8 *)in_mem->virtual_address + frame_hdr_length);
        in_mem->bus_address = (addr_t)((u8 *)in_mem->bus_address + frame_hdr_length);
  }

  buff = (u8 *)in_mem->virtual_address;

  frame_length =
      (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3] << 0);

  buff += 16;
  slice_info_num =
      (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3] << 0);
  if ((slice_info_num > RV_MAX_SLICE_NUM) || (slice_info_num == 0)) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "slice_info_num invalid: %d\n", slice_info_num);
    return 0;
  }

  hdr_bytes = 20 + slice_info_num * 8;
  if ((frame_length == 0) || (frame_length > (in_mem->logical_size - hdr_bytes))) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "frame_length(%d), buffer payload size(%d)\n",
                frame_length, in_mem->logical_size - hdr_bytes);
    return 0;
  }

  for (i = 0; i < slice_info_num; i++) {
    buff += 4;
    slice_info[i].is_valid =
        (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3] << 0);
    buff += 4;
    slice_info[i].offset =
        (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3] << 0);
  }

  return slice_info_num;
}

static void rv_dec_init_rv_mode(v4l2_dec_inst *h) {
  u8 *buff;
  int nPicWidth, nPicHeight;
  u32 is_rv8 = 0, num_frame_sizes = 0, length = 0, frame_code_len = 0;
  u32 frame_sizes[18];
  u32 size[9] = {0, 1, 1, 2, 2, 3, 3, 3, 3};
  u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;

  struct DecDownscaleCfg dscale_cfg = {0};
  RvDecRet re;

  struct DWLLinearMem *in_mem = &h->stream_in_mem;
  buff = (u8 *)in_mem->virtual_address;
  length = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3] << 0);
  (void)length;
  if (strncmp((const char *)(buff + 8), "RV30", 4) == 0)
    is_rv8 = 1;
  else
    is_rv8 = 0;

  nPicWidth = (buff[12] << 8) | buff[13];
  nPicHeight = (buff[14] << 8) | buff[15];

  if (is_rv8) {
    u32 j = 0;
    u8 *p = buff + 26;
    num_frame_sizes = 1 + (p[1] & 0x7);
    p += 8;
    frame_sizes[0] = nPicWidth;
    frame_sizes[1] = nPicHeight;
    for (j = 1; j < num_frame_sizes; j++) {
      frame_sizes[2 * j + 0] = (*p++) << 2;
      frame_sizes[2 * j + 1] = (*p++) << 2;
    }
  }

  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT)
    tiled_output = DEC_REF_FRM_TILED_DEFAULT;

  frame_code_len = size[num_frame_sizes];
  re = RvDecInit((RvDecInst *)(&h->decoder_inst), h->dwl_inst, 0,
                 frame_code_len, frame_sizes, is_rv8 ? 0 : 1, nPicWidth,
                 nPicHeight, FRAME_BUFFERS, tiled_output, 0, 0, &dscale_cfg);

  if (re != RVDEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "rvDecInit failed\n");
    ASSERT(0);
  }
  in_mem->logical_size -= length;
  in_mem->virtual_address = (u32 *)((u8 *)in_mem->virtual_address + length);
  in_mem->bus_address = (addr_t)((u8 *)in_mem->bus_address + length);
  if (in_mem->logical_size != 0) {
    HANTRO_LOG(HANTRO_LEVEL_WARNING,
               "dirty stream header data in one buffer\n");
  }
}

static void rv_dec_init_raw_mode(v4l2_dec_inst *h) {
  u8 *buff;
  u32 is_rv8 = 0;
  u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
  struct DecDownscaleCfg dscale_cfg = {0};
  RvDecRet re;

  struct DWLLinearMem *in_mem = &h->stream_in_mem;
  buff = (u8 *)in_mem->virtual_address;
  if (buff[0] == 0 && buff[1] == 0 && buff[2] == 1) {
    is_rv8 = 1;
  }

  if (h->dec_output_fmt == DEC_REF_FRM_TILED_DEFAULT)
    tiled_output = DEC_REF_FRM_TILED_DEFAULT;

  re = RvDecInit((RvDecInst *)(&h->decoder_inst), h->dwl_inst,
                 DEC_EC_PICTURE_FREEZE, 0, 0, is_rv8 ? 0 : 1, 0, 0,
                 FRAME_BUFFERS, tiled_output, 0, 0, &dscale_cfg);
  if (re != RVDEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "rvDecInit failed\n");
    ASSERT(0);
  }
}

/**
 * @brief rv_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int rv_dec_release_pic(v4l2_dec_inst *h, void *pic) {
  RvDecRet ret;
  ASSERT(pic);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rvDecPictureConsumed  bus address: %lx\n",
             ((RvDecPicture *)pic)->output_picture_bus_address);
  ret = RvDecPictureConsumed(h->decoder_inst, (RvDecPicture *)pic);
  ASSERT(ret == RVDEC_OK);
  return (ret == RVDEC_OK) ? 0 : -1;
}
static int rv_dec_remove_buffer(v4l2_dec_inst* h) {
  RvDecRet rv = RVDEC_OK;
  rv = RvDecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rv = %d, %p\n", rv, h->decoder_inst);
  return 0;
}


/**
 * @brief rv_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int rv_dec_add_buffer(v4l2_dec_inst *h, struct DWLLinearMem *mem_obj) {
  RvDecRet rv = RVDEC_OK;
  if (mem_obj) {
    rv = RvDecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "RvDecAddBuffer rv = %d, %p, %p\n", rv,
               h->decoder_inst, mem_obj);
    if (rv == RVDEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == RVDEC_OK || rv == RVDEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief rv_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void rv_dec_add_external_buffer(v4l2_dec_inst *h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

/**
 * @brief rv_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param u32 eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int rv_dec_get_pic(v4l2_dec_inst *h, vsi_v4l2_dec_picture *dec_pic_info,
                          u32 eos) {
  RvDecInfo dec_info = {0};
  RvDecPicture *dec_picture = (RvDecPicture *)h->priv_pic_data;
  RvDecRet re = RVDEC_OK;
  do {
    re = RvDecNextPicture(h->decoder_inst, dec_picture, 0);
  } while (eos && re != RVDEC_END_OF_STREAM && re != RVDEC_PIC_RDY &&
           re != RVDEC_OK);

  if (RVDEC_PIC_RDY != re) return -1;

  RvDecGetInfo(h->decoder_inst, &dec_info);
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
      dec_pic_info->pic_stride * dec_picture->frame_height;

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
 * @brief rv_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param RvDecInfo* dec_info: new seq. header info.
 * @param RvDecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static u32 rv_dec_check_res_change(v4l2_dec_inst *h, RvDecInfo *dec_info,
                                   RvDecBufferInfo *buf_info) {
  u32 res_change = 0;
  h->dec_info.bit_depth = 8;
  if ((dec_info->frame_width * 16 != h->dec_info.frame_width) ||
      (dec_info->frame_height * 16 != h->dec_info.frame_height) ||
      (dec_info->coded_width != h->dec_info.visible_rect.width) ||
      (dec_info->coded_height != h->dec_info.visible_rect.height) ||
      ((buf_info->buf_num + EXTRA_DPB_BUFFER_NUM) !=
       h->dec_info.needed_dpb_nums)) {
    HANTRO_LOG(HANTRO_LEVEL_WARNING,
               "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->frame_width * 16, dec_info->frame_height * 16);
    h->dec_info.frame_width = dec_info->frame_width * 16;
    h->dec_info.frame_height = dec_info->frame_height * 16;
    h->dec_info.visible_rect.left = 0;
    h->dec_info.visible_rect.top = 0;
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
 * @brief rv_update_input(), update in_mem according to dec_output info.
 * @param struct DWLLinearMem *in_mem.
 * @param RvDecInput *dec_input.
 * @param RvDecOutput *dec_output.
 * @return .
 */
static void rv_update_input(struct DWLLinearMem *in_mem,
                            RvDecOutput *dec_output) {
#if 0
    ASSERT(dec_output->data_left == 0);
    in_mem->logical_size = 0;//dec_output->data_left;
#else
  in_mem->logical_size =
      0;  // Raw mode dec_output->data_left =1 when finish decode
#endif
}

/**
 * @brief rv_dec_init(), rv decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event rv_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  RvDecApiVersion dec_api;
  RvDecBuild dec_build;
  /* Print API version number */
  dec_api = RvDecGetAPIVersion();
  dec_build = RvDecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 H.264 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;
  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);
  h->decoder_inst = (void *)FAKE_DECODER_INST;
  h->priv_data = calloc(1, sizeof(RV_PRIV_DATA));
  ASSERT(h->priv_data);
  return DEC_EMPTY_EVENT;
}

/**
 * @brief rv_dec_decode(), rv decoding function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event rv_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  i32 ret = 0;
  u32 strm_processed_count = 32;
  u32 skip_non_reference = 0;
  RvDecInput dec_input = {0};
  RvDecOutput dec_output = {0};
  RvDecInfo dec_info = {0};
  RvDecBufferInfo hbuf = {0};
  RvDecInst dec_inst = NULL;
  struct DWLLinearMem *in_mem = NULL;
  RV_PRIV_DATA *rv_priv = (RV_PRIV_DATA *)h->priv_data;
  if (h->decoder_inst == (void *)FAKE_DECODER_INST) {
    rv_priv->stream_mode = rv_get_stream_mode(&h->stream_in_mem);
    if (rv_priv->stream_mode == RV_MODE) {
      h->consumed_len += h->stream_in_mem.logical_size;
      rv_dec_init_rv_mode(h);
      goto end;
    } else {
      rv_dec_init_raw_mode(h);
    }
  }
  dec_inst = h->decoder_inst;
  in_mem = &h->stream_in_mem;
  if (rv_priv->stream_mode == RV_MODE) {
    if (rv_priv->total_slice_info_size == 0) {
      rv_priv->slice_info_num =
          rv_dec_parse_sliceinfo(in_mem, rv_priv->slice_info);
      if (rv_priv->slice_info_num == 0) {
        dec_input.data_len = in_mem->logical_size;
        goto update_input;
      }

      rv_priv->total_slice_info_size = NEXT_MULTIPLE(
          16 + rv_priv->slice_info_num * 8 + 4, RV_SLICE_DATA_ALIGN);
      in_mem->logical_size -= rv_priv->total_slice_info_size;
      h->consumed_len += rv_priv->total_slice_info_size;
#if 0
            memmove((u8 *)in_mem->virtual_address, 
                    (u8 *)in_mem->virtual_address + rv_priv->total_slice_info_size,
                    in_mem->logical_size);
#else
      in_mem->virtual_address = (u32 *)((u8 *)in_mem->virtual_address +
                                        rv_priv->total_slice_info_size);
      in_mem->bus_address =
          (addr_t)((u8 *)in_mem->bus_address + rv_priv->total_slice_info_size);
#endif
    }
  }

  dec_input.stream = (u8 *)in_mem->virtual_address;
  dec_input.stream_bus_address = (addr_t)in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;

  dec_input.slice_info_num =
      (rv_priv->stream_mode == RV_MODE) ? rv_priv->slice_info_num : 0;
  dec_input.slice_info =
      (rv_priv->stream_mode == RV_MODE) ? rv_priv->slice_info : NULL;

  dec_input.skip_non_reference = skip_non_reference;
  dec_input.pic_id = h->dec_in_pic_id;

  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream, dec_input.stream_bus_address,
             dec_input.data_len);
  do {
    ret = RvDecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "RvDecDecode ret %d decoded pic %d, out_pid %d\n", ret,
               h->dec_pic_id, h->dec_out_pic_id);
    switch (ret) {
      case RVDEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "UNSUPPORTED STREAM! \n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case RVDEC_HDRS_RDY:
        RvDecGetBufferInfo(dec_inst, &hbuf);
        if (RvDecGetInfo(dec_inst, &dec_info) != RVDEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        break;

      case RVDEC_PIC_DECODED:
        if (RvDecGetInfo(dec_inst, &dec_info) != RVDEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;

      case RVDEC_OK:
      case RVDEC_PIC_CONSUMED:
      case RVDEC_NONREF_PIC_SKIPPED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
      goto update_input;

      case RVDEC_STRM_PROCESSED:
        if (dec_output.data_left == dec_input.data_len && strm_processed_count--) {
            break;
        }
        goto update_input;

      case RVDEC_BUF_EMPTY:
      case RVDEC_STRM_ERROR:
        if (dec_output.data_left == dec_input.data_len) {
          re_dec_event = DEC_DECODING_ERROR_EVENT;
          in_mem->logical_size = 0;
          h->consumed_len += dec_input.data_len;
          goto end;
        }
        break;

      case RVDEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Waiting for frame buffers\n");
        RvDecGetBufferInfo(dec_inst, &hbuf);
        if (RvDecGetInfo(dec_inst, &dec_info) != RVDEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "ERROR in getting stream info!\n");
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        if (rv_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
          re_dec_event = DEC_SOURCE_CHANGE_EVENT;
          h->consumed_len += dec_input.data_len - dec_output.data_left;
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
          rv_dec_add_external_buffer(h);
        } else {
          goto end;
        }
        break;

      case RVDEC_NO_DECODING_BUFFER:
        if (h->dpb_buffer_added == 0 &&
            (h->dec_info.needed_dpb_nums <= h->existed_dpb_nums)) {
          rv_dec_add_external_buffer(h);
          break;
        } else {
          re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
          goto end;
        }

      case RVDEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;
    }
    h->consumed_len += dec_input.data_len - dec_output.data_left;
  } while (dec_output.data_left > 0);

  if ((ret != RVDEC_OK) && (ret != RVDEC_STRM_PROCESSED) &&
      (ret != RVDEC_NONREF_PIC_SKIPPED) && (ret != RVDEC_PIC_DECODED) &&
      (ret != RVDEC_PIC_CONSUMED)) {
    re_dec_event = DEC_FATAL_ERROR_EVENT;
    goto end;
  } else {
    re_dec_event = DEC_EMPTY_EVENT;
  }

update_input:
  h->consumed_len += dec_input.data_len; //consume all data since logical_size will be cleared
  rv_priv->total_slice_info_size = 0;
  rv_update_input(in_mem, &dec_output);
end:
  return re_dec_event;
}

/**
 * @brief rv_dec_destroy(), rv decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event rv_dec_destroy(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  if (h->priv_data) {
    free(h->priv_data);
    h->priv_data = NULL;
  }
  RvDecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);

  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish \n");
  return DEC_EMPTY_EVENT;
}

/**
 * @brief rv_dec_drain(), rv draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @return int: value of rvDecEndOfStream return.
 */
int rv_dec_drain(v4l2_dec_inst *h) {
  RvDecRet ret = RVDEC_OK;
  ret = RvDecEndOfStream(h->decoder_inst, 1);
  ASSERT(ret == RVDEC_OK);
  return ret;
}

/**
 * @brief rv_dec_seek(), rv seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event rv_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  RvDecRet ret = RVDEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "seek\n");
  ret = RvDecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "rvDecAbort returns %d\n", ret);
  ret = RvDecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(rv, V4L2_DAEMON_CODEC_DEC_RV, sizeof(RvDecPicture),
          DEC_ATTR_NONE);  // vsidaemon_rv_v4l2m2m_decoder
