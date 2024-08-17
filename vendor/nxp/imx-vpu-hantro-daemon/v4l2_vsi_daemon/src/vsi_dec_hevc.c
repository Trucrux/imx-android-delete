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
 * @file vsi_dec_hevc.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */
#include <dlfcn.h>

#include "buffer_list.h"
#include "dec_dpb_buff.h"
#include "vsi_dec.h"

#include "dwl.h"
#include "hevcdecapi.h"
//#include "hevc_container.h"

#ifdef VSI_CMODEL
#include "regdrv.h"
#include "tb_cfg.h"
#include "tb_stream_corrupt.h"
#include "tb_sw_performance.h"
#include "tb_tiled.h"
#endif

/*------------------------------------------------------------------------*/
#define BUFFER_ALIGN_FACTOR (64)
#define ALIGN_OFFSET(A) ((A) & (BUFFER_ALIGN_FACTOR - 1))
#define ALIGN(A) ((A) & (~(BUFFER_ALIGN_FACTOR - 1)))
/*------------------------------------------------------------------------*/

#ifdef VSI_CMODEL
extern struct TBCfg tb_cfg;
#endif

/*------------------------------------------------------------------------*/
typedef struct {
  struct HevcDecConfig dec_cfg;

  enum DecDpbFlags dec_output_fmt;
  enum DecPicturePixelFormat dec_pixel_fmt;

  int32_t set_new_info;
} HEVC_PRIV_DATA;
/*------------------------------------------------------------------------*/

#if 0  // obsolete functions
static void * (*G2DWLInit)(struct DWLInitParam * param) = NULL;
static i32 (*G2DWLRelease)(const void *instance) = NULL;

static void hevc_dec_dlsym_priv_func(v4l2_dec_inst* h)
{
    ASSERT(h->dec_dll_handle);
    G2DWLInit = dlsym(h->dec_dll_handle, "DWLInit");
    G2DWLRelease = dlsym(h->dec_dll_handle, "DWLRelease");
    ASSERT(G2DWLInit);
    ASSERT(G2DWLRelease);
}
#endif

/*------------------------------------------------------------------------*/
v4l2_inst_dec_event hevc_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg);
/*------------------------------------------------------------------------*/

/**
 * @brief hevc_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int hevc_dec_release_pic(v4l2_dec_inst *h, void *pic) {
  enum DecRet ret;

  ASSERT(pic);
  ret = HevcDecPictureConsumed(h->decoder_inst, (struct HevcDecPicture *)pic);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: HevcDecPictureConsumed ret = %d, %p\n",
             h->instance_id, ret, pic);

  ASSERT(ret == DEC_OK);
  return (ret == DEC_OK) ? 0 : -1;
}

/**
 * @brief hevc_dec_set_info(), set output & pixel format.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static uint32_t hevc_dec_set_info(v4l2_dec_inst *h) {
  HEVC_PRIV_DATA *priv = (HEVC_PRIV_DATA *)h->priv_data;
  struct HevcDecConfig *dec_cfg = &(priv->dec_cfg);
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

  priv->dec_pixel_fmt = h->dec_pixel_fmt;
  dec_cfg->pixel_format = h->dec_pixel_fmt;
  rv = HevcDecSetInfo(h->decoder_inst, &priv->dec_cfg);
  (void)rv;
  return 0;
}
static int hevc_dec_remove_buffer(v4l2_dec_inst* h) {
  enum DecRet rv = DEC_OK;
  rv = HevcDecRemoveBuffer(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: rv = %d\n", h->instance_id, rv);
  return 0;
}

/**
 * @brief hevc_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int hevc_dec_add_buffer(v4l2_dec_inst *h, struct DWLLinearMem *mem_obj) {
  enum DecRet rv = DEC_OK;
  HEVC_PRIV_DATA *priv = (HEVC_PRIV_DATA *)h->priv_data;
  if (mem_obj) {
    if (h->existed_dpb_nums == 0) {
      hevc_dec_set_info(h);
    }

    if (priv->set_new_info) {
      // if new output format is set, don't do AddBuffer().
      return 0;
    }

    rv = HevcDecAddBuffer(h->decoder_inst, mem_obj);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: HevcDecAddBuffer ret = %d, %p\n",
               h->instance_id, rv, mem_obj);
    if (rv == DEC_PARAM_ERROR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: Buffer is too small!\n",
            h->instance_id);
    }
  }
  return (rv == DEC_OK || rv == DEC_WAITING_FOR_BUFFER) ? 0 : 1;
}

/**
 * @brief hevc_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void hevc_dec_add_external_buffer(v4l2_dec_inst *h) {
#if 1
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: Add all queued buffers to ctrsw\n",
             h->instance_id);
  dpb_get_buffers(h);
#else
  enum DecRet rv;
  struct DWLLinearMem *mem;
  for (int i = 0; i < h->existed_dpb_nums; i++) {
    mem = dpb_get_buffer(h, i);
    if (mem) {
      rv = HevcDecAddBuffer(h->decoder_inst, mem);
      HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: HevcDecAddBuffer ret = %d, %p\n",
                 h->instance_id, rv, mem);
    }
  }
#endif
  h->dpb_buffer_added = 1;
}

/**
 * @brief hevc_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param uint32_t eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int hevc_dec_get_pic(v4l2_dec_inst *h,
                            vsi_v4l2_dec_picture *dec_pic_info, uint32_t eos) {
  struct HevcDecPicture *dec_picture =
      (struct HevcDecPicture *)h->priv_pic_data;
  enum DecRet re = DEC_OK;
  re = HevcDecNextPicture(h->decoder_inst, dec_picture);
  if (DEC_PIC_RDY != re) {
    if (eos) {
      do {
        re = HevcDecNextPicture(h->decoder_inst, dec_picture);
      } while (re != DEC_END_OF_STREAM && re != DEC_PIC_RDY &&
               (re == DEC_OK && h->dec_aborted == 0));
      if (re == DEC_END_OF_STREAM) {
        // workaround for JIRA521: DecEndOfStream() can't flush out all of
        // decoded pictures.
        hevc_dec_seek(h, NULL);
        h->dec_aborted = 1;
      }
    }
    if (re != DEC_PIC_RDY) return -1;
  }

  dec_pic_info->priv_pic_data = h->priv_pic_data;
  dec_pic_info->pic_id = dec_picture->pic_id;

  dec_pic_info->pic_width = dec_picture->pic_width;
  dec_pic_info->pic_height = dec_picture->pic_height;
  dec_pic_info->pic_stride = dec_picture->pic_stride;
  dec_pic_info->pic_corrupt = dec_picture->pic_corrupt;
  dec_pic_info->bit_depth = dec_picture->dec_info.bit_depth;
  dec_pic_info->crop_left = dec_picture->crop_params.crop_left_offset;
  dec_pic_info->crop_top = dec_picture->crop_params.crop_top_offset;
  dec_pic_info->crop_width = dec_picture->crop_params.crop_out_width;
  dec_pic_info->crop_height = dec_picture->crop_params.crop_out_height;
  dec_pic_info->output_picture_bus_address =
      dec_picture->output_picture_bus_address;
  dec_pic_info->output_picture_chroma_bus_address =
      dec_picture->output_picture_chroma_bus_address;
  dec_pic_info->output_rfc_luma_bus_address = dec_picture->output_rfc_luma_bus_address;
  dec_pic_info->output_rfc_chroma_bus_address = dec_picture->output_rfc_chroma_bus_address;  

  dec_pic_info->sar_width = dec_picture->dec_info.sar_width;
  dec_pic_info->sar_height = dec_picture->dec_info.sar_height;
  /* video signal info for HDR */
  dec_pic_info->video_range = dec_picture->dec_info.video_range;
  dec_pic_info->colour_description_present_flag =
      dec_picture->dec_info.colour_description_present_flag;
  dec_pic_info->colour_primaries = dec_picture->dec_info.colour_primaries;
  dec_pic_info->transfer_characteristics =
      dec_picture->dec_info.transfer_characteristics;
  dec_pic_info->matrix_coefficients = dec_picture->dec_info.matrix_coefficients;
  dec_pic_info->chroma_loc_info_present_flag =
      dec_picture->dec_info.chroma_loc_info_present_flag;
  dec_pic_info->chroma_sample_loc_type_top_field =
      dec_picture->dec_info.chroma_sample_loc_type_top_field;
  dec_pic_info->chroma_sample_loc_type_bottom_field =
      dec_picture->dec_info.chroma_sample_loc_type_bottom_field;
  /* HDR10 metadata */
  dec_pic_info->present_flag =
      dec_picture->dec_info.hdr10_metadata.present_flag;
  dec_pic_info->red_primary_x =
      dec_picture->dec_info.hdr10_metadata.red_primary_x;
  dec_pic_info->red_primary_y =
      dec_picture->dec_info.hdr10_metadata.red_primary_y;
  dec_pic_info->green_primary_x =
      dec_picture->dec_info.hdr10_metadata.green_primary_x;
  dec_pic_info->green_primary_y =
      dec_picture->dec_info.hdr10_metadata.green_primary_y;
  dec_pic_info->blue_primary_x =
      dec_picture->dec_info.hdr10_metadata.blue_primary_x;
  dec_pic_info->blue_primary_y =
      dec_picture->dec_info.hdr10_metadata.blue_primary_y;
  dec_pic_info->white_point_x =
      dec_picture->dec_info.hdr10_metadata.white_point_x;
  dec_pic_info->white_point_y =
      dec_picture->dec_info.hdr10_metadata.white_point_y;
  dec_pic_info->max_mastering_luminance =
      dec_picture->dec_info.hdr10_metadata.max_mastering_luminance;
  dec_pic_info->min_mastering_luminance =
      dec_picture->dec_info.hdr10_metadata.min_mastering_luminance;
  dec_pic_info->max_content_light_level =
      dec_picture->dec_info.hdr10_metadata.max_content_light_level;
  dec_pic_info->max_frame_average_light_level =
      dec_picture->dec_info.hdr10_metadata.max_frame_average_light_level;

  return 0;
}

/**
 * @brief hevc_dec_get_vui_info(), get vui info.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param HevcDecInfo* dec_info: new seq. header info.
 * @return .
 */
static void hevc_dec_get_vui_info(v4l2_dec_inst *h,
                                  struct HevcDecInfo *dec_info) {
  h->dec_info.colour_description_present_flag =
      dec_info->colour_description_present_flag;
  h->dec_info.matrix_coefficients = dec_info->matrix_coefficients;
  h->dec_info.colour_primaries = dec_info->colour_primaries;
  if (dec_info->trc_status == 0 || dec_info->trc_status == 2)
    h->dec_info.transfer_characteristics = dec_info->preferred_transfer_characteristics;
  else
    h->dec_info.transfer_characteristics = dec_info->transfer_characteristics;
  h->dec_info.video_range = dec_info->video_range;
}

static void hevc_dec_get_hdr10_metadata(v4l2_dec_inst *h,
                                        struct HevcDecInfo *dec_info) {
  h->dec_info.vpu_hdr10_meta.hasHdr10Meta =
      dec_info->hdr10_metadata.present_flag;
  h->dec_info.vpu_hdr10_meta.redPrimary[0] =
      dec_info->hdr10_metadata.red_primary_x;
  h->dec_info.vpu_hdr10_meta.redPrimary[1] =
      dec_info->hdr10_metadata.red_primary_y;
  h->dec_info.vpu_hdr10_meta.greenPrimary[0] =
      dec_info->hdr10_metadata.green_primary_x;
  h->dec_info.vpu_hdr10_meta.greenPrimary[1] =
      dec_info->hdr10_metadata.green_primary_y;
  h->dec_info.vpu_hdr10_meta.bluePrimary[0] =
      dec_info->hdr10_metadata.blue_primary_x;
  h->dec_info.vpu_hdr10_meta.bluePrimary[1] =
      dec_info->hdr10_metadata.blue_primary_y;
  h->dec_info.vpu_hdr10_meta.whitePoint[0] =
      dec_info->hdr10_metadata.white_point_x;
  h->dec_info.vpu_hdr10_meta.whitePoint[1] =
      dec_info->hdr10_metadata.white_point_y;
  h->dec_info.vpu_hdr10_meta.maxMasteringLuminance =
      dec_info->hdr10_metadata.max_mastering_luminance;
  h->dec_info.vpu_hdr10_meta.minMasteringLuminance =
      dec_info->hdr10_metadata.min_mastering_luminance;
  h->dec_info.vpu_hdr10_meta.maxContentLightLevel =
      dec_info->hdr10_metadata.max_content_light_level;
  h->dec_info.vpu_hdr10_meta.maxFrameAverageLightLevel =
      dec_info->hdr10_metadata.max_frame_average_light_level;
}
/**
 * @brief hevc_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param HevcDecInfo* dec_info: new seq. header info.
 * @param HevcDecBufferInfo buf_info: new buffer info.
 * @return int: 0: no change; 1: changed.
 */
static uint32_t hevc_dec_check_res_change(v4l2_dec_inst *h,
                                          struct HevcDecInfo *dec_info,
                                          struct HevcDecBufferInfo *buf_info) {
  uint32_t res_change = 0;
  u32 pixel_width = dec_info->pic_width;
  u32 bit_depth = 8;
  enum DecRet re = DEC_OK;

  re = HevcDecGetSpsBitDepth(h->decoder_inst, &bit_depth);
  if(re != DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "HevcDecGetSpsBitDepth Failed re = %d\n", re);
    bit_depth = dec_info->bit_depth;
  }
  if ((pixel_width != h->dec_info.frame_width) ||
      (dec_info->pic_height != h->dec_info.frame_height) ||
      (bit_depth != h->dec_info.bit_depth)
      /*        || (dec_info->crop_params.crop_left_offset !=
         h->dec_info.visible_rect.left) */
      ||
      (dec_info->crop_params.crop_out_width != h->dec_info.visible_rect.width)
      /*        || (dec_info->crop_params.crop_top_offset !=
         h->dec_info.visible_rect.top) */
      || (dec_info->crop_params.crop_out_height !=
          h->dec_info.visible_rect.height) ||
      ((buf_info->buf_num + EXTRA_DPB_BUFFER_NUM) !=
       h->dec_info.needed_dpb_nums)||
      (h->dec_info.dpb_buffer_size != buf_info->next_buf_size)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Inst[%lx]: Res-resolution Change <%dx%d> -> <%dx%d>, bit_depth %d -> %d \n",
               h->instance_id,
               h->dec_info.frame_width, h->dec_info.frame_height,
               dec_info->pic_width, dec_info->pic_height,
               h->dec_info.bit_depth, bit_depth);

    h->dec_info.frame_width = pixel_width;
    h->dec_info.frame_height = dec_info->pic_height;
    h->dec_info.bit_depth = bit_depth;
    h->dec_info.visible_rect.left = dec_info->crop_params.crop_left_offset;
    h->dec_info.visible_rect.width = dec_info->crop_params.crop_out_width;
    h->dec_info.visible_rect.top = dec_info->crop_params.crop_top_offset;
    h->dec_info.visible_rect.height = dec_info->crop_params.crop_out_height;
    h->dec_info.src_pix_fmt = VSI_V4L2_DEC_PIX_FMT_NV12;
    h->dec_info.needed_dpb_nums = buf_info->buf_num + EXTRA_DPB_BUFFER_NUM;
    h->dec_info.dpb_buffer_size = buf_info->next_buf_size;
    h->dec_info.pic_wstride = dec_info->pic_stride;
    hevc_dec_get_hdr10_metadata(h, dec_info);
    res_change = 1;
  }

  return res_change;
}

/**
 * @brief hevc_update_input(), update in_mem & dec_input according to dec_output
 * info.
 * @param struct DWLLinearMem *in_mem.
 * @param HevcDecInput *dec_input.
 * @param HevcDecOutput *dec_output.
 * @return .
 */
static void hevc_update_input(struct DWLLinearMem *in_mem,
                              struct HevcDecInput *dec_input,
                              struct HevcDecOutput *dec_output) {
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

  in_mem->bus_address = dec_input->stream_bus_address;
  in_mem->logical_size = dec_input->data_len;
  in_mem->virtual_address = (u32 *)dec_input->stream;
}

/**
 * @brief hevc_dec_init(), hevc decoder init function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event hevc_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  enum DecRet ret;
  HEVC_PRIV_DATA *priv;
  struct DWLInitParam dwl_params = {DWL_CLIENT_TYPE_HEVC_DEC};
  HevcDecBuild dec_build;

  dec_build = HevcDecGetBuild();
  HANTRO_LOG(HANTRO_LEVEL_INFO, "dec_build <0x%x,0x%x>\n", dec_build.sw_build,
             dec_build.hw_build);

  h->dwl_inst = DWLInit(&dwl_params);
  ASSERT(h->dwl_inst);

  h->priv_data = malloc(sizeof(HEVC_PRIV_DATA));
  ASSERT(h->priv_data);
  memset(h->priv_data, 0, sizeof(HEVC_PRIV_DATA));
  priv = (HEVC_PRIV_DATA *)h->priv_data;

  /* initialize decoder. If unsuccessful -> exit */
  struct HevcDecConfig *dec_cfg = &(priv->dec_cfg);
  dec_cfg->no_output_reordering = h->dec_no_reordering;
  dec_cfg->use_video_freeze_concealment = 0;
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
  dec_cfg->guard_size = 0;
  dec_cfg->use_adaptive_buffers = 0;
  dec_cfg->use_secure_mode = h->secure_mode_on;

  ret = HevcDecInit((HevcDecInst *)(&h->decoder_inst), h->dwl_inst, dec_cfg);

  if (ret != DEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: DECODER INITIALIZATION FAILED\n",
              h->instance_id);
    goto end;
  }

  priv->dec_output_fmt = h->dec_output_fmt;
  priv->dec_pixel_fmt = dec_cfg->pixel_format;
  priv->set_new_info = 0;
  return DEC_EMPTY_EVENT;
end:
  return DEC_FATAL_ERROR_EVENT;
}

/**
 * @brief hevc_dec_decode(), hevc decoding function
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event hevc_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;

  enum DecRet ret = DEC_OK;
  HEVC_PRIV_DATA *priv = (HEVC_PRIV_DATA *)h->priv_data;

  struct HevcDecInfo dec_info = {0};
  struct HevcDecBufferInfo hbuf = {0};

  struct HevcDecInput dec_input = {0};
  struct HevcDecOutput dec_output = {0};
  struct DWLLinearMem *in_mem = &h->stream_in_mem;

  HevcDecInst dec_inst = h->decoder_inst;

  dec_input.stream = (u8 *)in_mem->virtual_address;
  dec_input.stream_bus_address = in_mem->bus_address;
  dec_input.data_len = in_mem->logical_size;

  dec_input.buffer = dec_input.stream;
  dec_input.buffer_bus_address = ALIGN(dec_input.stream_bus_address);
  dec_input.buff_len =
      in_mem->logical_size + ALIGN_OFFSET(dec_input.stream_bus_address);
  dec_input.pic_id = h->dec_in_pic_id; /*curr_pic_id*/

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: stream len: %d\n",
             h->instance_id, dec_input.data_len);

  h->dec_aborted = 0;
  do {
    /* Picture ID is the picture number in decoding order */
    ret = HevcDecDecode(dec_inst, &dec_input, &dec_output);
    h->discard_dbp_count += HevcDecDiscardDpbNums(dec_inst);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: HevcDecDecode ret %d decoded pic %d\n",
               h->instance_id, ret, h->dec_pic_id);
    switch (ret) {
      case DEC_STREAM_NOT_SUPPORTED:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: UNSUPPORTED STREAM!\n",
                  h->instance_id);
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto end;

      case DEC_HDRS_RDY:
        HevcDecGetBufferInfo(dec_inst, &hbuf);
        /* Stream headers were successfully decoded
         * -> stream information is available for query now */
        if (HevcDecGetInfo(dec_inst, &dec_info) != DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR,
                     "Inst[%lx]: ERROR in getting stream info!\n",
                     h->instance_id);
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

#if 0
            if(hevc_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
                re_dec_event = DEC_SOURCE_CHANGE_EVENT;
                h->dec_add_buffer_allowed = 0;
                goto update_input; //end;
            }
#else
        HANTRO_LOG(HANTRO_LEVEL_INFO,
                   "Inst[%lx]: HDRS_RDY resolution: <%dx%d> -> <%dx%d>\n",
                   h->instance_id,
                   h->dec_info.frame_width, h->dec_info.frame_height,
                   dec_info.pic_width, dec_info.pic_height);
#endif
        hevc_dec_get_vui_info(h, &dec_info);
        break;

      case DEC_ADVANCED_TOOLS:
        /* ASO/STREAM ERROR was noticed in the stream. The decoder has to
         * reallocate resources */
        ASSERT(dec_output.data_left);
        break;

      case DEC_PIC_DECODED:
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        goto update_input;
        //break;

      case DEC_PENDING_FLUSH:

        /* Stream headers were successfully decoded
         * -> stream information is available for query now */
        if (HevcDecGetInfo(dec_inst, &dec_info) != DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR,
                     "Inst[%lx]: ERROR in getting stream info!\n",
                     h->instance_id);
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        /* Reset buffers added and stop adding extra buffers when wh changed. */
        /* if((dec_info.pic_width != h->dec_info.frame_width) ||
               (dec_info.pic_height != h->dec_info.frame_height)) */ {
          /* ctrlsw needs application to take away all of decoded pictures,
              otherwise, xxxDecDecode() will wait output-empty locally.
              So, we shouldn't check if resolution is identical or not here.*/
          re_dec_event = DEC_PENDING_FLUSH_EVENT;
          goto update_input;
        }
        break;

      case DEC_PIC_CONSUMED:
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto update_input;

      case DEC_STRM_PROCESSED:
      case DEC_NONREF_PIC_SKIPPED:
        break;

      case DEC_BUF_EMPTY:
      case DEC_STRM_ERROR:
        /* Used to indicate that picture decoding needs to finalized prior to
         * corrupting next picture
         * pic_rdy = 0; */
        if (dec_output.data_left == dec_input.data_len) {
          re_dec_event = DEC_DECODING_ERROR_EVENT;
          in_mem->logical_size = 0;
          h->consumed_len += dec_input.data_len;
          goto end;
        }
        break;

      case DEC_WAITING_FOR_BUFFER:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: Waiting for frame buffers\n",
                   h->instance_id);

#if 0
            h->dec_add_buffer_allowed = 1;
#else
        HevcDecGetBufferInfo(dec_inst, &hbuf);
        /* Stream headers were successfully decoded
         * -> stream information is available for query now */
        if (HevcDecGetInfo(dec_inst, &dec_info) != DEC_OK) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR,
                     "Inst[%lx]: ERROR in getting stream info!\n",
                     h->instance_id);
          re_dec_event = DEC_FATAL_ERROR_EVENT;
          goto end;
        }

        if (hevc_dec_check_res_change(h, &dec_info, &hbuf) != 0) {
          re_dec_event = DEC_SOURCE_CHANGE_EVENT;
          //                h->dec_add_buffer_allowed = 0;
        if (priv->set_new_info) {
            priv->set_new_info = 0;
        }
          goto update_input;  // end;
        }
#endif

        if (h->dec_info.needed_dpb_nums > h->existed_dpb_nums) {
          HANTRO_LOG(HANTRO_LEVEL_INFO,
                     "Inst[%lx]: Mismatch: needed dpb num %d, existed dpb num %d\n",
                     h->instance_id,
                     h->dec_info.needed_dpb_nums, h->existed_dpb_nums);
          re_dec_event = DEC_WAIT_DECODING_BUFFER_EVENT;
          goto end;
        }

        if (h->dpb_buffer_added == 0) {
          hevc_dec_add_external_buffer(h);
        } else {
          goto update_input;
        }

        break;

      case DEC_OK:
        /* nothing to do, just call again */
        break;

      case DEC_NO_DECODING_BUFFER:
        re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
        goto end;

      case DEC_HW_TIMEOUT:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: Timeout\n", h->instance_id);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "Inst[%lx]: FATAL ERROR: %d\n",
                  h->instance_id, ret);
        re_dec_event = DEC_DECODING_ERROR_EVENT;
        goto error;
    }

    h->consumed_len += dec_input.data_len - dec_output.data_left;

    /* break out of do-while if max_num_pics reached (data_len set to 0) */
    if (dec_input.data_len == 0) break;

    hevc_update_input(in_mem, &dec_input, &dec_output);

  } while (dec_input.data_len > 0);

  in_mem->logical_size = 0;

  if ((ret != DEC_OK) && (ret != DEC_STRM_PROCESSED) &&
      (ret != DEC_NONREF_PIC_SKIPPED) && (ret != DEC_PIC_DECODED) &&
      (ret != DEC_PIC_CONSUMED)) {
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
  hevc_update_input(in_mem, &dec_input, &dec_output);

end:
  if(ret != DEC_STRM_PROCESSED) {
    h->dec_inpkt_ignore_picconsumed_event = 0;
  }
  return re_dec_event;
}

/**
 * @brief hevc_dec_destroy(), hevc decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event hevc_dec_destroy(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: begin\n", h->instance_id);
  if (h->priv_data) {
    free(h->priv_data);
    h->priv_data = NULL;
  }
  HevcDecRelease(h->decoder_inst);
#ifdef VSI_CMODEL
  DWLRelease(h->dwl_inst);
#else
  DWLRelease(h->dwl_inst);
#endif

  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: finish\n", h->instance_id);
  return DEC_EMPTY_EVENT;
}

/**
 * @brief hevc_dec_drain(), hevc drain, free existed buffers in dpb.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return int: value of HevcDecEndOfStream return.
 */
int hevc_dec_drain(v4l2_dec_inst *h) {
  enum DecRet ret = DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: drain\n", h->instance_id);
  ret = HevcDecEndOfStream(h->decoder_inst);
  ASSERT(ret == DEC_OK);
  return ret;
}

/**
 * @brief hevc_dec_seek(), hevc seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event hevc_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  enum DecRet ret = DEC_OK;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: seek\n", h->instance_id);

  ret = HevcDecAbort(h->decoder_inst);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Inst[%lx]: HevcDecAbort returns %d\n",
             h->instance_id, ret);
  ret = HevcDecAbortAfter(h->decoder_inst);
  return DEC_EMPTY_EVENT;
}

// vsidaemon_hevc_v4l2m2m_decoder
VSIM2MDEC(hevc, V4L2_DAEMON_CODEC_DEC_HEVC, sizeof(struct HevcDecPicture),
          DEC_ATTR_EOS_THREAD | DEC_ATTR_SUPPORT_SECURE_MODE);
