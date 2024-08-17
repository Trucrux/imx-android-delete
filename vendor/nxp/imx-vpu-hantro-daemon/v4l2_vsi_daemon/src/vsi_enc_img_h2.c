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
#ifndef NXP

#include "vsi_enc_img_h2.h"
#include "buffer_list.h"
#include "fifo.h"
#include "vsi_enc_img_h2_priv.h"

#define USER_DEFINED_QTABLE 10

/* An example of user defined quantization table */
const u8 qTable[64] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

int32_t jpeg_encode_cmd_processor(v4l2_enc_inst *h,
                                  struct vsi_v4l2_msg *v4l2_msg) {
  BUFFER *p_buffer_input = NULL;
  BUFFER *p_buffer_get_output = NULL;
  BUFFER *p_buffer_output = NULL;
  BUFFER *p_buffer_get_input = NULL;
  BUFFER *p_buffer_reorder = NULL;
  uint32_t list_num = 0xffffffff;

  switch (v4l2_msg->cmd_id) {
    case V4L2_DAEMON_VIDIOC_BUF_RDY:
      if (v4l2_msg->params.enc_params.io_buffer.inbufidx != -1)  // input
      {
        p_buffer_input = (BUFFER *)malloc(sizeof(BUFFER));
        p_buffer_input->frame_display_id = h->input_frame_cnt;
        memcpy(&p_buffer_input->enc_cmd, &v4l2_msg->params.enc_params,
               sizeof(struct v4l2_daemon_enc_params));

        /*if input buffer comes, check the output bufferlist, if size>0, a full
        inout parameters can be a pair.
        pair to one params, and push into reorder buffer list. Change the state
        to encode if it is H26X_ENC_INIT now.*/
        if (bufferlist_get_size(h->bufferlist_output) > 0)  // paired
        {
          p_buffer_get_output = bufferlist_find_buffer(
              h->bufferlist_output, p_buffer_input->frame_display_id,
              &list_num);
          if (p_buffer_get_output == NULL) {
            HANTRO_LOG(HANTRO_LEVEL_ERROR, "bug in buffer list.\n");
            ASSERT(0);
          } else  // pair to one params, and push into reorder buffer list.
          {
            p_buffer_input->enc_cmd.io_buffer.busOutBuf =
                p_buffer_get_output->enc_cmd.io_buffer.busOutBuf;
            p_buffer_input->enc_cmd.io_buffer.outBufSize =
                p_buffer_get_output->enc_cmd.io_buffer.outBufSize;

            p_buffer_reorder = (BUFFER *)malloc(sizeof(BUFFER));
            memcpy(&p_buffer_reorder->enc_cmd, &p_buffer_input->enc_cmd,
                   sizeof(struct v4l2_daemon_enc_params));
            p_buffer_reorder->frame_display_id =
                h->input_frame_cnt;  // output num is more than input num, pick
                                     // the small one.
            bufferlist_push_buffer(h->bufferlist_reorder, p_buffer_reorder);
          }
          free(p_buffer_get_output);
          bufferlist_remove(h->bufferlist_output, list_num);

          if (((h->state == JPEG_ENC_INIT) || (h->state == JPEG_ENC_ENCODING) ||
               (h->state == JPEG_ENC_PAIRING)) &&
              (h->already_streamon == 1)) {
            h->state = JPEG_ENC_ENCODING;
          }
        } else  // not paired.
        {
          bufferlist_push_buffer(h->bufferlist_input, p_buffer_input);
          h->state = JPEG_ENC_PAIRING;
        }
        h->input_frame_cnt++;
      } else if (v4l2_msg->params.enc_params.io_buffer.outbufidx !=
                 -1)  // output
      {
        p_buffer_output = (BUFFER *)malloc(sizeof(BUFFER));
        p_buffer_output->frame_display_id = h->output_frame_cnt;
        memcpy(&p_buffer_output->enc_cmd, &v4l2_msg->params.enc_params,
               sizeof(struct v4l2_daemon_enc_params));

        if (bufferlist_get_size(h->bufferlist_input) > 0)  // paired
        {
          p_buffer_get_input = bufferlist_find_buffer(
              h->bufferlist_input, p_buffer_output->frame_display_id,
              &list_num);
          if (p_buffer_get_input == NULL) {
            HANTRO_LOG(HANTRO_LEVEL_ERROR, "bug in buffer list.\n");
            ASSERT(0);
          } else  // pair to one params, and push into reorder buffer list.
          {
            p_buffer_get_input->enc_cmd.io_buffer.busOutBuf =
                p_buffer_output->enc_cmd.io_buffer.busOutBuf;
            p_buffer_get_input->enc_cmd.io_buffer.outBufSize =
                p_buffer_output->enc_cmd.io_buffer.outBufSize;

            p_buffer_reorder = (BUFFER *)malloc(sizeof(BUFFER));
            memcpy(&p_buffer_reorder->enc_cmd, &p_buffer_get_input->enc_cmd,
                   sizeof(struct v4l2_daemon_enc_params));
            p_buffer_reorder->frame_display_id =
                h->output_frame_cnt;  // input num is more than output num, pick
                                      // the small one.
            bufferlist_push_buffer(h->bufferlist_reorder, p_buffer_reorder);
          }
          free(p_buffer_get_output);
          bufferlist_remove(h->bufferlist_input, list_num);

          if (((h->state == JPEG_ENC_INIT) || (h->state == JPEG_ENC_ENCODING) ||
               (h->state == JPEG_ENC_PAIRING)) &&
              (h->already_streamon == 1)) {
            h->state = JPEG_ENC_ENCODING;
          }

          h->output_frame_cnt++;
        } else  // not paired.
        {
          bufferlist_push_buffer(h->bufferlist_output, p_buffer_output);
          h->state = JPEG_ENC_PAIRING;
        }
        // to do
      } else {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "error no io buffers.\n");
        ASSERT(0);
      }
      break;

    case V4L2_DAEMON_VIDIOC_STREAMON_CAPTURE:
      if (v4l2_msg->param_type == 0)  // no parameter
      {
      } else if (v4l2_msg->param_type ==
                 1)  // all parameters. include input output and parameters.
      {
        HANTRO_LOG(HANTRO_LEVEL_DEBUG, "Has Param when streamon.\n");
        int32_t frame_display_id = (h->input_frame_cnt >= h->output_frame_cnt)
                                       ? (h->output_frame_cnt - 1)
                                       : (h->input_frame_cnt - 1);
        p_buffer_reorder = bufferlist_find_buffer(h->bufferlist_reorder,
                                                  frame_display_id, &list_num);
        if (p_buffer_reorder == NULL) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "should not be null.\n");
          ASSERT(0);
        }
        bufferlist_remove(h->bufferlist_reorder, list_num);
        memcpy(&p_buffer_reorder->enc_cmd.general,
               &v4l2_msg->params.enc_params.general,
               sizeof(struct v4l2_daemon_enc_general_cmd));
        memcpy(&p_buffer_reorder->enc_cmd.specific.enc_h26x_cmd,
               &v4l2_msg->params.enc_params.specific.enc_h26x_cmd,
               sizeof(struct v4l2_daemon_enc_h26x_cmd));
        bufferlist_push_buffer(h->bufferlist_reorder, p_buffer_reorder);
      }

      h->state = JPEG_ENC_INIT;
      h->already_streamon = 1;
      break;

    case V4L2_DAEMON_VIDIOC_CMD_STOP:
    case V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE:
      h->state = JPEG_ENC_STOPPED;
      break;

    case V4L2_DAEMON_VIDIOC_FAKE:
      break;
    default:
      break;
  }
  return 0;
}
static void close_jpeg_encoder(v4l2_enc_inst *h) {}
static int32_t set_jpeg_parameters(v4l2_enc_inst *h,
                                   v4l2_daemon_enc_params *enc_params,
                                   int32_t if_config) {
  JpegEncIn *encIn = &h->params.jpeg_enc_param.jpeg_in;

  /*--------------------------------- 1.
   * JpegEncCfg----------------------------------------*/
  if (if_config) {
    JpegEncCfg *cfg = &h->params.jpeg_enc_param.jpeg_config;

    /* Encoder initialization */
    if (enc_params->general.width == -1)
      enc_params->general.width = enc_params->general.lumWidthSrc;

    if (enc_params->general.height == -1)
      enc_params->general.height = enc_params->general.lumHeightSrc;

    if (enc_params->specific.enc_jpeg_cmd.exp_of_input_alignment == 7) {
      if (enc_params->specific.enc_jpeg_cmd.frameType == JPEGENC_YUV420_PLANAR)
        enc_params->general.horOffsetSrc =
            ((enc_params->general.horOffsetSrc + 256 - 1) & (~(256 - 1)));
      else
        enc_params->general.horOffsetSrc =
            ((enc_params->general.horOffsetSrc + 128 - 1) & (~(128 - 1)));

      if ((enc_params->general.lumWidthSrc - enc_params->general.horOffsetSrc) <
          enc_params->general.width)
        enc_params->general.horOffsetSrc = 0;
    }

    /* lossless mode */
    if (enc_params->specific.enc_jpeg_cmd.predictMode != 0) {
      cfg->losslessEn = 1;
      cfg->predictMode = enc_params->specific.enc_jpeg_cmd.predictMode;
      cfg->ptransValue = enc_params->specific.enc_jpeg_cmd.ptransValue;
    } else {
      cfg->losslessEn = 0;
    }

    cfg->rotation = (JpegEncPictureRotation)enc_params->general.rotation;
    cfg->inputWidth =
        (enc_params->general.lumWidthSrc + 15) & (~15); /* API limitation */
    if (cfg->inputWidth != (u32)enc_params->general.lumWidthSrc)
      HANTRO_LOG(HANTRO_LEVEL_WARNING,
                 "Warning: Input width must be multiple of 16!\n");
    cfg->inputHeight = enc_params->general.lumHeightSrc;

    if (cfg->rotation && cfg->rotation != JPEGENC_ROTATE_180) {
      /* full */
      cfg->xOffset = enc_params->general.verOffsetSrc;
      cfg->yOffset = enc_params->general.horOffsetSrc;

      cfg->codingWidth = enc_params->general.height;
      cfg->codingHeight = enc_params->general.width;
      cfg->xDensity = enc_params->specific.enc_jpeg_cmd.ydensity;
      cfg->yDensity = enc_params->specific.enc_jpeg_cmd.xdensity;
    } else {
      /* full */
      cfg->xOffset = enc_params->general.horOffsetSrc;
      cfg->yOffset = enc_params->general.verOffsetSrc;

      cfg->codingWidth = enc_params->general.width;
      cfg->codingHeight = enc_params->general.height;
      cfg->xDensity = enc_params->specific.enc_jpeg_cmd.xdensity;
      cfg->yDensity = enc_params->specific.enc_jpeg_cmd.ydensity;
    }
    cfg->mirror = enc_params->general.mirror;

    if (enc_params->specific.enc_jpeg_cmd.qLevel == USER_DEFINED_QTABLE) {
      cfg->qTableLuma = qTable;
      cfg->qTableChroma = qTable;
    } else
      cfg->qLevel = enc_params->specific.enc_jpeg_cmd.qLevel;

    cfg->restartInterval = enc_params->specific.enc_jpeg_cmd.restartInterval;
    cfg->codingType =
        (JpegEncCodingType)enc_params->specific.enc_jpeg_cmd.partialCoding;
    cfg->frameType =
        (JpegEncFrameType)enc_params->specific.enc_jpeg_cmd.frameType;
    cfg->unitsType =
        (JpegEncAppUnitsType)enc_params->specific.enc_jpeg_cmd.unitsType;
    cfg->markerType =
        (JpegEncTableMarkerType)enc_params->specific.enc_jpeg_cmd.markerType;
    cfg->colorConversion.type =
        (JpegEncColorConversionType)enc_params->general.colorConversion;
    if (cfg->colorConversion.type == JPEGENC_RGBTOYUV_USER_DEFINED) {
      /* User defined RGB to YCbCr conversion coefficients, scaled by 16-bits */
      cfg->colorConversion.coeffA = 20000;
      cfg->colorConversion.coeffB = 44000;
      cfg->colorConversion.coeffC = 5000;
      cfg->colorConversion.coeffE = 35000;
      cfg->colorConversion.coeffF = 38000;
      cfg->colorConversion.coeffG = 35000;
      cfg->colorConversion.coeffH = 38000;
      cfg->colorConversion.LumaOffset = 0;
    }
    // writeOutput = enc_params->specific.enc_jpeg_cmd.write;
    cfg->codingMode =
        (JpegEncCodingMode)enc_params->specific.enc_jpeg_cmd.codingMode;

    /* constant chroma control */
    cfg->constChromaEn = enc_params->specific.enc_jpeg_cmd.constChromaEn;
    cfg->constCb = enc_params->specific.enc_jpeg_cmd.constCb;
    cfg->constCr = enc_params->specific.enc_jpeg_cmd.constCr;

    /* jpeg rc*/
    cfg->targetBitPerSecond = enc_params->specific.enc_jpeg_cmd.bitPerSecond;
    cfg->frameRateNum = 1;
    cfg->frameRateDenom = 1;

    // framerate valid only when RC enabled
    if (enc_params->specific.enc_jpeg_cmd.bitPerSecond) {
      cfg->frameRateNum = enc_params->general.outputRateNumer;
      cfg->frameRateDenom = enc_params->general.outputRateDenom;
    }
    cfg->qpmin = enc_params->specific.enc_jpeg_cmd.qpmin;
    cfg->qpmax = enc_params->specific.enc_jpeg_cmd.qpmax;
    cfg->fixedQP = enc_params->specific.enc_jpeg_cmd.fixedQP;
    cfg->rcMode = enc_params->specific.enc_jpeg_cmd.rcMode;
    cfg->picQpDeltaMax = enc_params->specific.enc_jpeg_cmd.picQpDeltaMax;
    cfg->picQpDeltaMin = enc_params->specific.enc_jpeg_cmd.picQpDeltaMin;

    /*stride*/
    cfg->exp_of_input_alignment =
        enc_params->specific.enc_jpeg_cmd.exp_of_input_alignment;

    if (enc_params->specific.enc_jpeg_cmd.thumbnail < 0 ||
        enc_params->specific.enc_jpeg_cmd.thumbnail > 3) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Not valid thumbnail format!\n");
      return -1;
    }
  }

  /*--------------------------------- 2.
   * JpegEncIn----------------------------------------*/
  encIn->busLum = enc_params->io_buffer.busLuma;
  encIn->busCb = enc_params->io_buffer.busChromaU;
  encIn->busCr = enc_params->io_buffer.busChromaV;
  encIn->pOutBuf[0] = (u8 *)enc_params->io_buffer.busOutBuf;
  encIn->busOutBuf[0] = enc_params->io_buffer.busOutBuf;
  encIn->outBufSize[0] = enc_params->io_buffer.outBufSize;

  encIn->frameHeader = 1;
  return 0;
}

int32_t jpeg_encode_state_processor(v4l2_enc_inst *h,
                                    struct vsi_v4l2_msg *v4l2_msg) {
  vsi_v4l2_msg msg;
  JpegEncRet ret;
  BUFFER *p_buffer = NULL;
  uint32_t list_num = 0xffffffff;

  switch (h->state) {
    case JPEG_ENC_INIT:
      p_buffer = bufferlist_find_buffer(h->bufferlist_reorder, 0,
                                        &list_num);  // match 0
      if (p_buffer == NULL) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR,
                   "error, state H26X_ENC_INIT should get one buffer.\n");
        ASSERT(0);
      } else {
        bufferlist_remove(h->bufferlist_reorder, list_num);
      }

      set_jpeg_parameters(h, &p_buffer->enc_cmd, 1);
      if ((ret = JpegEncInit(&h->params.jpeg_enc_param.jpeg_config,
                             &h->inst.jpeg_enc_inst, NULL)) != JPEGENC_OK) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR,
                   "Failed to initialize the encoder. Error code: %8i\n", ret);
        return (int32_t)ret;
      }
      ret = JpegEncSetPictureSize(h->inst.jpeg_enc_inst,
                                  &h->params.jpeg_enc_param.jpeg_config);
      if (ret != JPEGENC_OK) {
        ASSERT(0);
      }

      ret = JpegEncEncode(h->inst.jpeg_enc_inst,
                          &h->params.jpeg_enc_param.jpeg_in,
                          &h->params.jpeg_enc_param.jpeg_out);
      /*inform driver.*/
      msg.error = (int)ret;
      msg.inst_id = h->instance_id;
      if (pipe_fd) {
        send_notif_to_v4l2(pipe_fd, &msg, sizeof(struct vsi_v4l2_msg));
      }
      break;

    case JPEG_ENC_ENCODING:
      ret = JpegEncSetPictureSize(&h->inst.jpeg_enc_inst,
                                  &h->params.jpeg_enc_param.jpeg_config);
      if (ret != JPEGENC_OK) {
        ASSERT(0);
      }

      ret = JpegEncEncode(h->inst.jpeg_enc_inst,
                          &h->params.jpeg_enc_param.jpeg_in,
                          &h->params.jpeg_enc_param.jpeg_out);
      break;

    case JPEG_ENC_STOPPED:
      close_jpeg_encoder(h);
      return JPEG_ENC_STOPPED;
    default:
      break;
  }
  return 0;
}
#endif
