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
 * @file vsi_dec_jpeg.c
 * @brief V4L2 Daemon video process file.
 * @version 0.10- Initial version
 */

#include "vsi_dec.h"
#include "buffer_list.h"
#include "dec_dpb_buff.h"

#include "jpegdecapi.h"

#ifdef VSI_CMODEL
#include "deccfg.h"
#endif

#define MAX_BUFFER_NUM (32)
#define TRUE (1)
#define FALSE (0)
#define MCU_SIZE (16)
#define MCU_MAX_COUNT (8100)
typedef struct {
  struct DWLLinearMem mem_obj[MAX_BUFFER_NUM];
  uint32_t pic_id[MAX_BUFFER_NUM];
  int head;
  int tail;
} JpegBufferQueue;

typedef struct {
  JpegBufferQueue out_buffer_for_decoding;
  JpegBufferQueue out_buffer_for_dequeue;
  JpegDecImageInfo jpg_img_info;
} JpegDecPrivData;

static vsi_v4l2dec_pixfmt jpeg_dec_get_src_pixfmt(u32 output_format) {
  vsi_v4l2dec_pixfmt ret = VSI_V4L2_DEC_PIX_FMT_NV12;
  switch (output_format) {
    case JPEGDEC_YCbCr420_SEMIPLANAR:
      ret = VSI_V4L2_DEC_PIX_FMT_NV12;
      break;
    case JPEGDEC_YCbCr400:
      ret = VSI_V4L2_DEC_PIX_FMT_400;
      break;
    case JPEGDEC_YCbCr411_SEMIPLANAR:
      ret = VSI_V4L2_DEC_PIX_FMT_411SP;
      break;
    case JPEGDEC_YCbCr422_SEMIPLANAR:
      ret = VSI_V4L2_DEC_PIX_FMT_422SP;
      break;
    case JPEGDEC_YCbCr444_SEMIPLANAR:
      ret = VSI_V4L2_DEC_PIX_FMT_444SP;
      break;
    default:
      break;
  }
  return ret;
}

static void jpeg_init_buf_queue(JpegBufferQueue *buf_q) {
  buf_q->tail = 0;
  buf_q->head = 0;
}

static u32 jpeg_is_full_buf_queue(JpegBufferQueue *buf_q) {
  if (buf_q->tail == ((buf_q->head + 1) % MAX_BUFFER_NUM))
    return TRUE;
  else {
    return FALSE;
  }
}

static u32 jpeg_is_empty_buf_queue(JpegBufferQueue *buf_q) {
  if (buf_q->tail == buf_q->head)
    return TRUE;
  else {
    return FALSE;
  }
}

static int jpeg_buf_enqueue(JpegBufferQueue *buf_q,
                            struct DWLLinearMem *out_buffer,
                            uint32_t pic_id) {
  if (jpeg_is_full_buf_queue(buf_q) == TRUE) return -1;
  memcpy(&buf_q->mem_obj[buf_q->head], out_buffer, sizeof(struct DWLLinearMem));
  buf_q->pic_id[buf_q->head] = pic_id;
  buf_q->head = (buf_q->head + 1) % MAX_BUFFER_NUM;
  return 0;
}

static int jpeg_buffer_dequeue(JpegBufferQueue *buf_q,
                               struct DWLLinearMem **display_mem,
                               uint32_t *pic_id) {
  if (jpeg_is_empty_buf_queue(buf_q) == TRUE) return -1;
  *display_mem = &buf_q->mem_obj[buf_q->tail];
  if(pic_id != NULL) *pic_id = buf_q->pic_id[buf_q->tail];
  buf_q->tail = (buf_q->tail + 1) % MAX_BUFFER_NUM;
  return 0;
}

/**
 * @brief jpeg_get_output_size(), get the output picture size.
 * @param JpegDecImageInfo image_info
 * @return int: output size.
 */
static u32 jpeg_get_output_size(JpegDecImageInfo *image_info) {
  u32 output_size = 0;
  if (image_info->output_format == JPEGDEC_YCbCr420_SEMIPLANAR ||
      image_info->output_format == JPEGDEC_YCbCr411_SEMIPLANAR) {
    output_size =
        (image_info->output_width * image_info->output_height) * 3 / 2;
  } else if (image_info->output_format == JPEGDEC_YCbCr422_SEMIPLANAR ||
             image_info->output_format == JPEGDEC_YCbCr440) {
    output_size = (image_info->output_width * image_info->output_height) * 2;
  } else if (image_info->output_format == JPEGDEC_YCbCr444_SEMIPLANAR) {
    output_size = (image_info->output_width * image_info->output_height) * 3;
  } else if (image_info->output_format == JPEGDEC_YCbCr400) {
    output_size = image_info->output_width * image_info->output_height;
  } else {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "unknow image format 0x%x.\n",
               image_info->output_format);
    ASSERT(0);
  }
  return output_size;
}

static u32 jpeg_get_chroma_size(JpegDecImageInfo *image_info) {
  u32 chroma_size = 0;
  if (image_info->output_format == JPEGDEC_YCbCr420_SEMIPLANAR ||
      image_info->output_format == JPEGDEC_YCbCr411_SEMIPLANAR) {
    chroma_size = 0;
  } else if (image_info->output_format == JPEGDEC_YCbCr422_SEMIPLANAR ||
             image_info->output_format == JPEGDEC_YCbCr440) {
    chroma_size = image_info->output_width * image_info->output_height;
  } else if (image_info->output_format == JPEGDEC_YCbCr444_SEMIPLANAR) {
    chroma_size = (image_info->output_width * image_info->output_height) * 2;
  }
  return chroma_size;
}

/**
 * @brief jpeg_dec_release_pic(), to notify ctrlsw one output picture has been
 * consumed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param void* pic: pointer of consumed pic.
 * @return .
 */
static int jpeg_dec_release_pic(v4l2_dec_inst *h, void *pic) {
  int rv = -1;
  if (pic) {
    rv = jpeg_buf_enqueue(
        &((JpegDecPrivData *)h->priv_data)->out_buffer_for_decoding,
        (struct DWLLinearMem *)pic, 0);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Jpeg Dec Consumed  bus address: %lx\n",
               ((struct DWLLinearMem *)pic)->bus_address);
  }
  return rv;
}

static int jpeg_dec_remove_buffer(v4l2_dec_inst* h) {
  struct DWLLinearMem *mem = NULL;
  while(jpeg_buffer_dequeue(&((JpegDecPrivData *)h->priv_data)->out_buffer_for_decoding, &mem, NULL) == 0) ;
  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish\n");
  return 0;
}

/**
 * @brief jpeg_dec_add_buffer(), to add one queued buffer into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return 0: succeed; other: failed.
 */
static int jpeg_dec_add_buffer(v4l2_dec_inst *h, struct DWLLinearMem *mem_obj) {
  int rv = -1;
  if (mem_obj) {
    rv = jpeg_buf_enqueue(
        &((JpegDecPrivData *)h->priv_data)->out_buffer_for_decoding, mem_obj, 0);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "JpegDecAddBuffer rv = %d, %p\n", rv,
               mem_obj);
  }
  return rv;
}

/**
 * @brief jpeg_dec_add_external_buffer(), to add queued buffers into ctrlsw.
 * @param v4l2_dec_inst* h: daemon instance.
 * @return .
 */
static void jpeg_dec_add_external_buffer(v4l2_dec_inst *h) {
  HANTRO_LOG(HANTRO_LEVEL_INFO, "Add all queued buffers to ctrsw\n");
  dpb_get_buffers(h);
  h->dpb_buffer_added = 1;
}

/**
 * @brief jpeg_dec_get_pic(), get one renderable pic from dpb.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_dec_picture* dec_pic_info: pointer of pic info.
 * @param u32 eos: to indicate end of stream or not.
 * @return int: 0: get an output pic successfully; others: failed to get.
 */
static int jpeg_dec_get_pic(v4l2_dec_inst *h,
                            vsi_v4l2_dec_picture *dec_pic_info, u32 eos) {
  int mem_ret = 0;
  struct DWLLinearMem *display_mem = NULL;
  JpegDecPrivData *jpeg_priv_data = (JpegDecPrivData *)h->priv_data;
  uint32_t pic_id = 0;

  mem_ret = jpeg_buffer_dequeue(&jpeg_priv_data->out_buffer_for_dequeue,
                                &display_mem, &pic_id);
  if (mem_ret == -1) return -1;
  JpegDecImageInfo img_info = jpeg_priv_data->jpg_img_info;
  u32 chroma_size = jpeg_get_chroma_size(&img_info);
  ASSERT(display_mem);

  dec_pic_info->pic_id = pic_id;
  dec_pic_info->priv_pic_data = display_mem;
  dec_pic_info->pic_width = h->dec_info.frame_width;
  dec_pic_info->pic_height = h->dec_info.frame_height;
  dec_pic_info->pic_stride = h->dec_info.frame_width;
  dec_pic_info->pic_corrupt = 0;
  dec_pic_info->bit_depth = 8;
  dec_pic_info->crop_left = 0;
  dec_pic_info->crop_top = 0;
  dec_pic_info->crop_width = h->dec_info.frame_width;
  dec_pic_info->crop_height = h->dec_info.frame_height;
  dec_pic_info->output_picture_bus_address = display_mem->bus_address;
  if (img_info.output_format == JPEGDEC_YCbCr400) {
    dec_pic_info->output_picture_chroma_bus_address = 0;
  } else {
    dec_pic_info->output_picture_chroma_bus_address =
        display_mem->bus_address +
        h->dec_info.frame_width * h->dec_info.frame_width;
    dec_pic_info->chroma_size = chroma_size;
  }
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
 * @brief jpeg_dec_check_res_change(), check if resolution changed.
 * @param v4l2_dec_inst* h: daemon instance.
 * @param JpegDecImageInfo image_info
 * @return int: 0: no change; 1: changed.
 */
static u32 jpeg_dec_check_res_change(v4l2_dec_inst *h,
                                     JpegDecImageInfo *image_info,
                                     u32 buf_size) {
  u32 res_change = 0;
  h->dec_info.bit_depth = 8;
  if ((image_info->output_width != h->dec_info.frame_width) ||
      (image_info->output_height != h->dec_info.frame_height) ||
      (image_info->display_width != h->dec_info.visible_rect.width) ||
      (image_info->display_height != h->dec_info.visible_rect.height) ||
      (buf_size != h->dec_info.dpb_buffer_size)) {
    HANTRO_LOG(HANTRO_LEVEL_INFO, "Res-resolution Change <%dx%d> -> <%dx%d>\n",
               h->dec_info.frame_width, h->dec_info.frame_height,
               image_info->output_width, image_info->output_height);
    h->dec_info.frame_width = image_info->output_width;
    h->dec_info.frame_height = image_info->output_height;
    h->dec_info.visible_rect.left = 0;
    h->dec_info.visible_rect.width = image_info->display_width;
    h->dec_info.visible_rect.top = 0;
    h->dec_info.visible_rect.height = image_info->display_height;
    h->dec_info.needed_dpb_nums = 1;
    h->dec_info.src_pix_fmt =
        jpeg_dec_get_src_pixfmt(image_info->output_format);
    h->dec_info.dpb_buffer_size = buf_size;
    h->dec_info.pic_wstride = h->dec_info.frame_width;
    res_change = 1;
  }
  return res_change;
}

/**
 * @brief jpeg_update_input(), update in_mem & dec_input according to dec_output
 * info.
 * @param struct DWLLinearMem *in_mem.
 * @param JpegDecInput *dec_input.
 * @param JpegDecOutput *dec_output.
 * @return
 */
static void jpeg_update_input(struct DWLLinearMem *in_mem,
                              JpegDecInput *dec_input,
                              JpegDecOutput *dec_output) {}

/**
 * @brief jpeg_dec_init(), jpeg decoder init functioin
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event jpeg_dec_init(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  JpegDecApiVersion dec_api;
  JpegDecBuild dec_build;
  JpegDecRet re;
  /* Print API version number */
  dec_api = JpegGetAPIVersion();
  dec_build = JpegDecGetBuild();
  HANTRO_LOG(
      HANTRO_LEVEL_INFO,
      "\nX170 JPEG Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
      dec_api.major, dec_api.minor, dec_build.sw_build >> 16,
      dec_build.sw_build & 0xFFFF, dec_build.hw_build);

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_JPEG_DEC;
  h->dwl_inst = DWLInit(&dwl_init);
  ASSERT(h->dwl_inst);

  re = JpegDecInit((JpegDecInst *)(&h->decoder_inst));
  if (re != JPEGDEC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "JpegDecInit failed\n");
    goto end;
  }

  h->priv_data = malloc(sizeof(JpegDecPrivData));
  JpegDecPrivData *jpeg_priv_data = (JpegDecPrivData *)h->priv_data;
  ASSERT(jpeg_priv_data);
  jpeg_init_buf_queue(&jpeg_priv_data->out_buffer_for_decoding);
  jpeg_init_buf_queue(&jpeg_priv_data->out_buffer_for_dequeue);
  memset(&jpeg_priv_data->jpg_img_info, 0, sizeof(JpegDecImageInfo));
  return DEC_EMPTY_EVENT;
end:
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "failed\n");
  return DEC_FATAL_ERROR_EVENT;
}

static void jpeg_set_dec_output_addr(JpegDecInput *dec_input,
                                     struct DWLLinearMem *input_mem,
                                     JpegDecImageInfo *image_info,
                                     u32 slice_index) {
  u32 luma_size = image_info->output_width * image_info->output_height;
  u32 slice_luma_offset = 0;
  u32 slice_chroma_offset = 0;
  if (dec_input->slice_mb_set > 0) {
    u32 slice_height = dec_input->slice_mb_set * MCU_SIZE;
    slice_luma_offset = slice_height * image_info->output_width * slice_index;
    if (image_info->output_format == JPEGDEC_YCbCr420_SEMIPLANAR ||
        image_info->output_format == JPEGDEC_YCbCr411_SEMIPLANAR)
      slice_chroma_offset =
          slice_height * image_info->output_width * slice_index / 2;
    else if (image_info->output_format == JPEGDEC_YCbCr422_SEMIPLANAR ||
             image_info->output_format == JPEGDEC_YCbCr440)
      slice_chroma_offset =
          slice_height * image_info->output_width * slice_index;
    else if (image_info->output_format == JPEGDEC_YCbCr444_SEMIPLANAR)
      slice_chroma_offset =
          slice_height * image_info->output_width * slice_index * 2;
    else if (image_info->output_format == JPEGDEC_YCbCr400) {
      slice_chroma_offset = 0;
    } else {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Unkonwn image format 0x%x\n",
                 image_info->output_format);
      ASSERT(0);
      slice_chroma_offset = 0;
    }
  }

  dec_input->picture_buffer_y.virtual_address =
      (u32 *)((u8 *)input_mem->virtual_address + slice_luma_offset);
  dec_input->picture_buffer_y.bus_address =
      input_mem->bus_address + slice_luma_offset;

  if (image_info->output_format == JPEGDEC_YCbCr400) {
    dec_input->picture_buffer_cb_cr.virtual_address = 0;
    dec_input->picture_buffer_cb_cr.bus_address = 0;
  } else {
    dec_input->picture_buffer_cb_cr.virtual_address =
        (u32 *)((u8 *)input_mem->virtual_address + luma_size +
                slice_chroma_offset);
    dec_input->picture_buffer_cb_cr.bus_address =
        input_mem->bus_address + luma_size + slice_chroma_offset;
  }
}

/**
 * @brief jpeg_dec_decode(), jpeg decoding functioin
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event jpeg_dec_decode(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  v4l2_inst_dec_event re_dec_event = DEC_EMPTY_EVENT;
  JpegDecRet ret = JPEGDEC_OK;
  int buf_size = 0;
  int timeout_count = 0;
  u32 slice_index = 0;
  JpegDecInput dec_input = {0};
  JpegDecOutput dec_output = {0};
  JpegDecImageInfo image_info = {0};
  struct DWLLinearMem *in_mem = &h->stream_in_mem;
  struct DWLLinearMem *out_buffer = NULL;
  JpegDecInst dec_inst = h->decoder_inst;

  dec_input.stream_buffer.virtual_address = (u32 *)in_mem->virtual_address;
  dec_input.stream_buffer.bus_address = in_mem->bus_address;
  dec_input.stream_length = in_mem->logical_size;
  JpegDecRet info_ret = JPEGDEC_OK;
  JpegDecPrivData *jpeg_priv_data = (JpegDecPrivData *)h->priv_data;

  if (jpeg_is_empty_buf_queue(&jpeg_priv_data->out_buffer_for_decoding) &&
      h->existed_dpb_nums) {
    re_dec_event = DEC_NO_DECODING_BUFFER_EVENT;
    goto end;
  }
  if (h->dec_in_new_packet) {
    info_ret = JpegDecGetImageInfo(dec_inst, &dec_input, &image_info);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "JpegDecGetImageInfo ret %d\n", info_ret);
    if (info_ret != JPEGDEC_OK) {
      re_dec_event = DEC_EMPTY_EVENT;
      goto error;
    }
    jpeg_priv_data->jpg_img_info = image_info;
    buf_size = jpeg_get_output_size(&image_info);
    if (buf_size == 0) {
      re_dec_event = DEC_FATAL_ERROR_EVENT;
      goto error;
    }
    if (jpeg_dec_check_res_change(h, &image_info, buf_size)) {
      jpeg_init_buf_queue(&jpeg_priv_data->out_buffer_for_decoding);
      jpeg_init_buf_queue(&jpeg_priv_data->out_buffer_for_dequeue);
      return DEC_SOURCE_CHANGE_EVENT;
    }
  }

  image_info = jpeg_priv_data->jpg_img_info;
  if (image_info.output_width / MCU_SIZE * image_info.output_height / MCU_SIZE >
      MCU_MAX_COUNT) {
    dec_input.slice_mb_set =
        MCU_MAX_COUNT / (image_info.output_width / MCU_SIZE);
  } else {
    dec_input.slice_mb_set = 0;
  }

  if (jpeg_buffer_dequeue(&jpeg_priv_data->out_buffer_for_decoding,
                          &out_buffer, NULL) == -1) {
    ASSERT(0);
  }

  jpeg_set_dec_output_addr(&dec_input, out_buffer, &image_info, slice_index);
  HANTRO_LOG(HANTRO_LEVEL_INFO, "input: %p %lx stream len: %d\n",
             dec_input.stream_buffer.virtual_address,
             dec_input.stream_buffer.bus_address, dec_input.stream_length);

  do {
    ret = JpegDecDecode(dec_inst, &dec_input, &dec_output);
    HANTRO_LOG(HANTRO_LEVEL_INFO, "JpegDecDecode ret %d\n", ret);
    switch (ret) {
      case JPEGDEC_HW_TIMEOUT:
        timeout_count++;
        if (timeout_count < 100) {
          slice_index = 0;
          jpeg_set_dec_output_addr(&dec_input, out_buffer, &image_info,
                                  slice_index);
          break;
        }
        ret = JPEGDEC_FRAME_READY;

      case JPEGDEC_FRAME_READY:
        jpeg_buf_enqueue(&jpeg_priv_data->out_buffer_for_dequeue, out_buffer, h->dec_in_pic_id);
        h->dec_pic_id++;
        re_dec_event = DEC_PIC_DECODED_EVENT;
        h->consumed_len += dec_input.stream_length;
        break;

      case JPEGDEC_SLICE_READY:
        slice_index++;
        jpeg_set_dec_output_addr(&dec_input, out_buffer, &image_info,
                                 slice_index);
        break;
      case JPEGDEC_SCAN_PROCESSED:
      case JPEGDEC_STRM_PROCESSED:
        break;

      case JPEGDEC_STRM_ERROR:
        HANTRO_LOG(HANTRO_LEVEL_INFO, "stream error\n");
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto error;

#if 0
        case JPEGDEC_HW_TIMEOUT:
            HANTRO_LOG(HANTRO_LEVEL_INFO, "Timeout\n");
            re_dec_event = DEC_FATAL_ERROR_EVENT;
            goto end;
#endif

      default:
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "FATAL ERROR: %d\n", ret);
        re_dec_event = DEC_FATAL_ERROR_EVENT;
        goto error;
    }

  } while (ret != JPEGDEC_FRAME_READY);
error:
  in_mem->logical_size = 0;
end:
  return re_dec_event;
}

/**
 * @brief jpeg_dec_destroy(), jpeg decoder destroy
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event jpeg_dec_destroy(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  JpegDecRelease(h->decoder_inst);
  DWLRelease(h->dwl_inst);
  if (h->priv_data) {
    free(h->priv_data);
    h->priv_data = NULL;
  }
  HANTRO_LOG(HANTRO_LEVEL_INFO, "finish \n");
  return DEC_EMPTY_EVENT;
}

/**
 * @brief jpeg_dec_drain(), jpeg draing
 * @param v4l2_dec_inst* h: daemon instance.
 * @return int: value of JpegDecEndOfStream return.
 */
static int jpeg_dec_drain(v4l2_dec_inst *h) { return 0; }

/**
 * @brief jpeg_dec_seek(), jpeg seek
 * @param v4l2_dec_inst* h: daemon instance.
 * @param vsi_v4l2_msg* v4l2_msg: message from v4l2 driver.
 * @return v4l2_inst_dec_event: event for state machine.
 */
v4l2_inst_dec_event jpeg_dec_seek(v4l2_dec_inst *h, vsi_v4l2_msg *v4l2_msg) {
  return DEC_EMPTY_EVENT;
}

VSIM2MDEC(jpeg, V4L2_DAEMON_CODEC_DEC_JPEG, sizeof(struct DWLLinearMem),
          DEC_ATTR_NONE);  // vsidaemon_jpeg_v4l2m2m_decoder
