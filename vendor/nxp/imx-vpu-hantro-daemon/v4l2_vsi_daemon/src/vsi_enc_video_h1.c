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
 * @file vsi_enc_video.c
 * @brief V4L2 Daemon video process header file.
 * @version 0.10- Initial version
 */
#include "vsi_enc_video_h1.h"
#include <math.h>
#include "buffer_list.h"
#include "fifo.h"
#include "vsi_daemon_debug.h"
#include "vsi_enc.h"
#include "vsi_enc_video_h1_priv.h"

#define H264ENC_MIN_ENC_WIDTH 144  // confirm with YiZhong, 144x96
#define H264ENC_MAX_ENC_WIDTH 4080
#define H264ENC_MIN_ENC_HEIGHT 96
#define H264ENC_MAX_ENC_HEIGHT 4080
#define H264ENC_MAX_MBS_PER_PIC 65025 /* 4080x4080 */

#define VP8ENC_MIN_ENC_WIDTH 144
#define VP8ENC_MAX_ENC_WIDTH 4080
#define VP8ENC_MIN_ENC_HEIGHT 96
#define VP8ENC_MAX_ENC_HEIGHT 4080
#define VP8ENC_MAX_MBS_PER_PIC 65025 /* 4080x4080 */

i32 H264EncSetVuiColorDescription(
    H264EncInst inst, u32 vuiVideoSignalTypePresentFlag, u32 vuiVideoFormat,
    u32 vuiColorDescripPresentFlag, u32 vuiColorPrimaries,
    u32 vuiTransferCharacteristics, u32 vuiMatrixCoefficients);
//#define OUTPUT_VP8_HEADER
#ifdef OUTPUT_VP8_HEADER
#define IVF_HDR_BYTES 32
#define IVF_FRM_BYTES 12
void IvfHeader(u8 *dst, uint32_t width, uint32_t height,
               uint32_t outputRateNumer, uint32_t outputRateDenom,
               uint64_t frameCntTotal) {
  u8 data[IVF_HDR_BYTES] = {0};

  /* File header signature */
  data[0] = 'D';
  data[1] = 'K';
  data[2] = 'I';
  data[3] = 'F';

  /* File format version and file header size */
  data[6] = 32;

  /* Video data FourCC */
  data[8] = 'V';
  data[9] = 'P';
  data[10] = '8';
  data[11] = '0';

  /* Video Image width and height */
  data[12] = width & 0xff;
  data[13] = (width >> 8) & 0xff;
  data[14] = height & 0xff;
  data[15] = (height >> 8) & 0xff;

  /* Frame rate rate */
  data[16] = outputRateNumer & 0xff;
  data[17] = (outputRateNumer >> 8) & 0xff;
  data[18] = (outputRateNumer >> 16) & 0xff;
  data[19] = (outputRateNumer >> 24) & 0xff;

  /* Frame rate scale */
  data[20] = outputRateDenom & 0xff;
  data[21] = (outputRateDenom >> 8) & 0xff;
  data[22] = (outputRateDenom >> 16) & 0xff;
  data[23] = (outputRateDenom >> 24) & 0xff;

  /* Video length in frames */
  data[24] = frameCntTotal & 0xff;
  data[25] = (frameCntTotal >> 8) & 0xff;
  data[26] = (frameCntTotal >> 16) & 0xff;
  data[27] = (frameCntTotal >> 24) & 0xff;

  memcpy(dst, data, IVF_HDR_BYTES);
}

/*------------------------------------------------------------------------------
    IvfFrame
------------------------------------------------------------------------------*/
void IvfFrame(u8 *dst, uint32_t frameSize, uint64_t frameCntTotal) {
  uint32_t byteCnt = 0;
  u8 data[IVF_FRM_BYTES];

  /* Frame size (4 bytes) */
  byteCnt = frameSize;
  data[0] = byteCnt & 0xff;
  data[1] = (byteCnt >> 8) & 0xff;
  data[2] = (byteCnt >> 16) & 0xff;
  data[3] = (byteCnt >> 24) & 0xff;

  /* Timestamp (8 bytes) */
  data[4] = (frameCntTotal)&0xff;
  data[5] = ((frameCntTotal) >> 8) & 0xff;
  data[6] = ((frameCntTotal) >> 16) & 0xff;
  data[7] = ((frameCntTotal) >> 24) & 0xff;
  data[8] = ((frameCntTotal) >> 32) & 0xff;
  data[9] = ((frameCntTotal) >> 40) & 0xff;
  data[10] = ((frameCntTotal) >> 48) & 0xff;
  data[11] = ((frameCntTotal) >> 56) & 0xff;

  memcpy(dst, data, IVF_FRM_BYTES);
}
#endif

static H264EncLevel getLevelH264(i32 levelIdx)
{
  switch (levelIdx)
  {
    case 0:
      return H264ENC_LEVEL_1;
    case 1:
      return H264ENC_LEVEL_1_b;
    case 2:
      return H264ENC_LEVEL_1_1;
    case 3:
      return H264ENC_LEVEL_1_2;
    case 4:
      return H264ENC_LEVEL_1_3;
    case 5:
      return H264ENC_LEVEL_2;
    case 6:
      return H264ENC_LEVEL_2_1;
    case 7:
      return H264ENC_LEVEL_2_2;
    case 8:
      return H264ENC_LEVEL_3;
    case 9:
      return H264ENC_LEVEL_3_1;
    case 10:
      return H264ENC_LEVEL_3_2;
    case 11:
      return H264ENC_LEVEL_4;
    case 12:
      return H264ENC_LEVEL_4_1;
    case 13:
      return H264ENC_LEVEL_4_2;
    case 14:
      return H264ENC_LEVEL_5;
    case 15:
      return H264ENC_LEVEL_5_1;

    default:
      return H264ENC_LEVEL_5_1;
  }
}

static int32_t getLevelIdxH264(H264EncLevel level)
{
  switch (level)
  {
    case H264ENC_LEVEL_1:
      return 0;
    case H264ENC_LEVEL_1_b:
      return 1;
    case H264ENC_LEVEL_1_1:
      return 2;
    case H264ENC_LEVEL_1_2:
      return 3;
    case H264ENC_LEVEL_1_3:
      return 4;
    case H264ENC_LEVEL_2:
      return 5;
    case H264ENC_LEVEL_2_1:
      return 6;
    case H264ENC_LEVEL_2_2:
      return 7;
    case H264ENC_LEVEL_3:
      return 8;
    case H264ENC_LEVEL_3_1:
      return 9;
    case H264ENC_LEVEL_3_2:
      return 10;
    case H264ENC_LEVEL_4:
      return 11;
    case H264ENC_LEVEL_4_1:
      return 12;
    case H264ENC_LEVEL_4_2:
      return 13;
    case H264ENC_LEVEL_5:
      return 14;
    case H264ENC_LEVEL_5_1:
      return 15;
    default:
      ASSERT(0);
  }
  return 0;
}

/**
 * @brief init_encoder_h264(), create & init h264 enc instance.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int init_encoder_h264(v4l2_enc_inst *h,
                             v4l2_daemon_enc_params *enc_params) {
  H264EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;

  H264EncConfig *cfg = (void *)&h264_params->video_config;
  if (enc_params->general.rotation && enc_params->general.rotation != 3) {
    cfg->width = enc_params->general.height;
    cfg->height = enc_params->general.width;
  } else {
    cfg->width = enc_params->general.width;
    cfg->height = enc_params->general.height;
  }
  if (cfg->width < H264ENC_MIN_ENC_WIDTH || (cfg->width & 0x3) != 0 ||
      cfg->height < H264ENC_MIN_ENC_HEIGHT || (cfg->height & 0x1) != 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
               cfg->width, cfg->height);
    return DAEMON_ERR_ENC_PARA;
  }
  if (cfg->width > h->configure_t.max_width_h264 ||
      cfg->height > H264ENC_MAX_ENC_HEIGHT) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
               cfg->width, cfg->height);
    return DAEMON_ERR_ENC_NOT_SUPPORT;
  }
  /* total macroblocks per picture limit */
  if (((cfg->height + 15) / 16) * ((cfg->width + 15) / 16) >
      H264ENC_MAX_MBS_PER_PIC) {
    return DAEMON_ERR_ENC_NOT_SUPPORT;
  }
  cfg->frameRateDenom = enc_params->general.outputRateDenom;
  cfg->frameRateNum = enc_params->general.outputRateNumer;
  if ((cfg->frameRateNum == 0 || cfg->frameRateDenom == 0) ||
      (cfg->frameRateDenom > cfg->frameRateNum &&
       !(cfg->frameRateDenom == 1001 &&
         cfg->frameRateNum == 1000))) { /* special allowal of 1000/1001, 0.99
                                           fps by customer request */
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Framerate is not set properly, set to default (30fps).\n");
    cfg->frameRateDenom = 1;
    cfg->frameRateNum = 30;
  }
  /* intra tools in sps and pps */
  cfg->streamType = (enc_params->specific.enc_h26x_cmd.byteStream)
                        ? H264ENC_BYTE_STREAM
                        : H264ENC_NAL_UNIT_STREAM;
  cfg->level = H264ENC_LEVEL_5_1;
  if (enc_params->specific.enc_h26x_cmd.avclevel != DEFAULTLEVEL)
    cfg->level = (H264EncLevel)enc_params->specific.enc_h26x_cmd.avclevel;

  int32_t levelIdx = getLevelIdxH264(cfg->level), minLvl;

  minLvl = calculate_level(h->codec_fmt, cfg->width, cfg->height,
                 cfg->frameRateNum, cfg->frameRateDenom, enc_params->general.bitPerSecond,
                 enc_params->specific.enc_h26x_cmd.profile);
   if(levelIdx < minLvl) {
      cfg->level = getLevelH264(minLvl);
      send_warning_orphan_msg(h->instance_id, WARN_LEVEL);
  }

  /* Find the max number of reference frame */
  if (enc_params->specific.enc_h26x_cmd.intraPicRate == 1)
    cfg->refFrameAmount = 1;
  else
    cfg->refFrameAmount = 1;

  ret = H264EncInit(cfg, &h->inst);
  if (ret != H264ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncInit() failed. vsi_ret=%d\n",
                ret);
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief init_encoder_vp8(), create & init h264 enc instance.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int init_encoder_vp8(v4l2_enc_inst *h,
                            v4l2_daemon_enc_params *enc_params) {
  VP8EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;

  VP8EncConfig *cfg = &vp8_params->vp8_config;
  if (enc_params->general.rotation && enc_params->general.rotation != 3) {
    cfg->width = enc_params->general.height;
    cfg->height = enc_params->general.width;
  } else {
    cfg->width = enc_params->general.width;
    cfg->height = enc_params->general.height;
  }
  if (cfg->width < VP8ENC_MIN_ENC_WIDTH || (cfg->width & 0x3) != 0 ||
      cfg->height < VP8ENC_MIN_ENC_HEIGHT || (cfg->height & 0x1) != 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
               cfg->width, cfg->height);
    return DAEMON_ERR_ENC_PARA;
  }
  if (cfg->width > h->configure_t.max_width_vp8 ||
      cfg->height > VP8ENC_MAX_ENC_HEIGHT) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
               cfg->width, cfg->height);
    return DAEMON_ERR_ENC_NOT_SUPPORT;
  }
  /* total macroblocks per picture limit */
  if (((cfg->height + 15) / 16) * ((cfg->width + 15) / 16) >
      VP8ENC_MAX_MBS_PER_PIC) {
    return DAEMON_ERR_ENC_NOT_SUPPORT;
  }
  cfg->frameRateDenom = enc_params->general.outputRateDenom;
  cfg->frameRateNum = enc_params->general.outputRateNumer;
  if ((cfg->frameRateNum == 0 || cfg->frameRateDenom == 0) ||
      (cfg->frameRateDenom > cfg->frameRateNum &&
       !(cfg->frameRateDenom == 1001 &&
         cfg->frameRateNum == 1000))) { /* special allowal of 1000/1001, 0.99
                                           fps by customer request */
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Framerate is not set properly, set to default (30fps).\n");
    cfg->frameRateDenom = 1;
    cfg->frameRateNum = 30;
  }
  /* Find the max number of reference frame */
  if (enc_params->specific.enc_h26x_cmd.intraPicRate == 1) {
    cfg->refFrameAmount = 1;
  } else {
    cfg->refFrameAmount = 1;
  }

  ret = VP8EncInit(cfg, &h->inst);
  if (ret != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncInit() failed. vsi_ret=%d\n",
                ret);
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief set_rate_control_h264(), set rate control.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_rate_control_h264(v4l2_enc_inst *h,
                                 v4l2_daemon_enc_params *enc_params) {
  H264EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;

  H264EncRateCtrl *rcCfg = &h264_params->video_rate_ctrl;
  H264EncConfig *cfg = &h264_params->video_config;

  if ((ret = H264EncGetRateCtrl(h->inst, rcCfg)) != H264ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncGetRateCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  if (enc_params->specific.enc_h26x_cmd.qpHdr != -1)
    rcCfg->qpHdr = enc_params->specific.enc_h26x_cmd.qpHdr;
  else
    rcCfg->qpHdr = -1;

  if (!h->next_pic_type &&
      enc_params->specific.enc_h26x_cmd.qpHdrI_h26x != -1) {  // I
    rcCfg->qpHdr = rcCfg->fixedIntraQp =
        enc_params->specific.enc_h26x_cmd.qpHdrI_h26x;
  } else if (h->next_pic_type &&
             enc_params->specific.enc_h26x_cmd.qpHdrP_h26x != -1) {  // P
    rcCfg->qpHdr = enc_params->specific.enc_h26x_cmd.qpHdrP_h26x;
  }

  if (enc_params->specific.enc_h26x_cmd.picSkip != -1)
    rcCfg->pictureSkip = enc_params->specific.enc_h26x_cmd.picSkip;
  else
    rcCfg->pictureSkip = 0;
  if (enc_params->specific.enc_h26x_cmd.picRc != -1)
    rcCfg->pictureRc = enc_params->specific.enc_h26x_cmd.picRc;
  else
    rcCfg->pictureRc = 0;
  if (enc_params->general.bitPerSecond != -1)
    rcCfg->bitPerSecond = enc_params->general.bitPerSecond;
  if (enc_params->specific.enc_h26x_cmd.hrdConformance != -1)
    rcCfg->hrd = enc_params->specific.enc_h26x_cmd.hrdConformance;
  if (enc_params->specific.enc_h26x_cmd.picRc == 0) rcCfg->hrd = 0;

  enc_params->specific.enc_h26x_cmd.cpbSize = -1;
  if (enc_params->specific.enc_h26x_cmd.cpbSize > 0)
    rcCfg->hrdCpbSize = enc_params->specific.enc_h26x_cmd.cpbSize;
  else {//cpbSize default value.
    int32_t levelIdx = getLevelIdxH264(cfg->level);
    rcCfg->hrdCpbSize = getMaxCpbSize(h->codec_fmt, levelIdx);
  }
  if (enc_params->specific.enc_h26x_cmd.intraQpDelta != -1)
    rcCfg->intraQpDelta = enc_params->specific.enc_h26x_cmd.intraQpDelta;
  rcCfg->fixedIntraQp = enc_params->specific.enc_h26x_cmd.fixedIntraQp;
  rcCfg->gopLen = enc_params->specific.enc_h26x_cmd.intraPicRate;
  if(rcCfg->gopLen > 300)
      rcCfg->gopLen = 300;
  if (rcCfg->hrd == 1 && rcCfg->pictureRc == 1) {  // cbr mode
    h->cbr_last_bitrate = enc_params->general.bitPerSecond;
  }
  if ((enc_params->specific.enc_h26x_cmd.qpMin_h26x >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMin_h26x <= 51))
    rcCfg->qpMin = enc_params->specific.enc_h26x_cmd.qpMin_h26x;
  else
    rcCfg->qpMin = 0;
  if ((enc_params->specific.enc_h26x_cmd.qpMax_h26x >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMax_h26x <= 51))
    rcCfg->qpMax = enc_params->specific.enc_h26x_cmd.qpMax_h26x;
  else
    rcCfg->qpMax = 51;

  if ((ret = H264EncSetRateCtrl(
            h->inst, rcCfg)) != H264ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncSetRateCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief set_rate_control_vp8(), set rate control.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_rate_control_vp8(v4l2_enc_inst *h,
                                v4l2_daemon_enc_params *enc_params) {
  VP8EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;

  VP8EncRateCtrl *rcCfg = &vp8_params->vp8_rate_ctrl;

  if ((ret = VP8EncGetRateCtrl(h->inst, rcCfg)) != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncGetRateCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  if (enc_params->specific.enc_h26x_cmd.qpHdr != -1)
    rcCfg->qpHdr = enc_params->specific.enc_h26x_cmd.qpHdr;
  else
    rcCfg->qpHdr = -1;
  if (!h->next_pic_type &&
      enc_params->specific.enc_h26x_cmd.qpHdrI_vpx != -1) {  // I
    rcCfg->qpHdr = rcCfg->fixedIntraQp =
        enc_params->specific.enc_h26x_cmd.qpHdrI_vpx;
  } else if (h->next_pic_type &&
             enc_params->specific.enc_h26x_cmd.qpHdrP_vpx != -1) {  // P
    rcCfg->qpHdr = enc_params->specific.enc_h26x_cmd.qpHdrP_vpx;
  }

  if (enc_params->specific.enc_h26x_cmd.picSkip != -1)
    rcCfg->pictureSkip = enc_params->specific.enc_h26x_cmd.picSkip;
  else
    rcCfg->pictureSkip = 0;
  if (enc_params->specific.enc_h26x_cmd.picRc != -1)
    rcCfg->pictureRc = enc_params->specific.enc_h26x_cmd.picRc;
  else
    rcCfg->pictureRc = 0;
  if (enc_params->general.bitPerSecond != -1)
    rcCfg->bitPerSecond = enc_params->general.bitPerSecond;
  if (enc_params->specific.enc_h26x_cmd.intraQpDelta != -1)
    rcCfg->intraQpDelta = enc_params->specific.enc_h26x_cmd.intraQpDelta;
  rcCfg->fixedIntraQp = enc_params->specific.enc_h26x_cmd.fixedIntraQp;

  rcCfg->intraPictureRate = enc_params->specific.enc_h26x_cmd.intraPicRate;

  rcCfg->bitrateWindow = enc_params->specific.enc_h26x_cmd.intraPicRate;
  if (rcCfg->bitrateWindow > 300)
    rcCfg->bitrateWindow = 300;

  if ((enc_params->specific.enc_h26x_cmd.qpMin_vpx >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMin_vpx <= 127))
    rcCfg->qpMin = enc_params->specific.enc_h26x_cmd.qpMin_vpx;
  else
    rcCfg->qpMin = 0;
  if ((enc_params->specific.enc_h26x_cmd.qpMax_vpx >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMax_vpx <= 127))
    rcCfg->qpMax = enc_params->specific.enc_h26x_cmd.qpMax_vpx;
  else
    rcCfg->qpMax = 127;

  if ((ret = VP8EncSetRateCtrl(h->inst, rcCfg)) != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncSetRateCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  if (rcCfg->pictureRc)
    h->cbr_last_bitrate = rcCfg->bitPerSecond;

  return 0;
}

/**
 * @brief set_pre_process_h264(), set pre-processing config.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_pre_process_h264(v4l2_enc_inst *h,
                                 v4l2_daemon_enc_params *enc_params) {
  H264EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;

  H264EncPreProcessingCfg *preProcCfg = &h264_params->video_prep;

  if ((ret = H264EncGetPreProcessing(h->inst, preProcCfg)) != H264ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncGetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  preProcCfg->inputType = enc_params->general.inputFormat;
  preProcCfg->inputType = (H264EncPictureType)enc_params->general.inputFormat;
  preProcCfg->rotation = (H264EncPictureRotation)enc_params->general.rotation;
  preProcCfg->origWidth = enc_params->general.lumWidthSrc;
  preProcCfg->origHeight = enc_params->general.lumHeightSrc;
  if (enc_params->specific.enc_h26x_cmd.interlacedFrame)
    preProcCfg->origHeight /= 2;
  if (enc_params->general.horOffsetSrc != -1)
    preProcCfg->xOffset = enc_params->general.horOffsetSrc;
  if (enc_params->general.verOffsetSrc != -1)
    preProcCfg->yOffset = enc_params->general.verOffsetSrc;
  if (enc_params->general.colorConversion != -1)
    preProcCfg->colorConversion.type =
        (H264EncColorConversionType)enc_params->general.colorConversion;
  if (preProcCfg->colorConversion.type == H264ENC_RGBTOYUV_USER_DEFINED) {
    preProcCfg->colorConversion.coeffA = 20000;
    preProcCfg->colorConversion.coeffB = 44000;
    preProcCfg->colorConversion.coeffC = 5000;
    preProcCfg->colorConversion.coeffE = 35000;
    preProcCfg->colorConversion.coeffF = 38000;
  }
  if (enc_params->general.scaledWidth * enc_params->general.scaledHeight > 0)
    preProcCfg->scaledOutput = 1;

  if ((ret = H264EncSetPreProcessing(h->inst, preProcCfg)) != H264ENC_OK) {
    // close_h26x_encoder(h);
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncSetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief set_pre_process_vp8(), set pre-processing config.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_pre_process_vp8(v4l2_enc_inst *h,
                                v4l2_daemon_enc_params *enc_params) {
  VP8EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;
  VP8EncPreProcessingCfg *preProcCfg = &vp8_params->vp8_prep;

  if ((ret = VP8EncGetPreProcessing(h->inst, preProcCfg)) != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncGetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  preProcCfg->inputType = enc_params->general.inputFormat;
  preProcCfg->inputType = (VP8EncPictureType)enc_params->general.inputFormat;
  preProcCfg->rotation = (VP8EncPictureRotation)enc_params->general.rotation;
  preProcCfg->origWidth = enc_params->general.lumWidthSrc;
  preProcCfg->origHeight = enc_params->general.lumHeightSrc;
  if (enc_params->specific.enc_h26x_cmd.interlacedFrame)
    preProcCfg->origHeight /= 2;
  if (enc_params->general.horOffsetSrc != -1)
    preProcCfg->xOffset = enc_params->general.horOffsetSrc;
  if (enc_params->general.verOffsetSrc != -1)
    preProcCfg->yOffset = enc_params->general.verOffsetSrc;
  if (enc_params->general.colorConversion != -1)
    preProcCfg->colorConversion.type =
        (VP8EncColorConversionType)enc_params->general.colorConversion;
  if (preProcCfg->colorConversion.type == VP8ENC_RGBTOYUV_USER_DEFINED) {
    preProcCfg->colorConversion.coeffA = 20000;
    preProcCfg->colorConversion.coeffB = 44000;
    preProcCfg->colorConversion.coeffC = 5000;
    preProcCfg->colorConversion.coeffE = 35000;
    preProcCfg->colorConversion.coeffF = 38000;
  }
  if (enc_params->general.scaledWidth * enc_params->general.scaledHeight > 0)
    preProcCfg->scaledOutput = 1;

  if ((ret = VP8EncSetPreProcessing(h->inst, preProcCfg)) != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncSetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

static int32_t check_area_h264(v4l2_enc_inst *hinst, H264EncPictureArea *area, int32_t width,
                        int32_t height) {
  int32_t w = (width + 15) / 16;
  int32_t h = (height + 15) / 16;

  if ((area->left < w) && (area->right < w) && (area->top < h) &&
      (area->bottom < h))
    return 0;

  HANTRO_LOG(HANTRO_LEVEL_ERROR, "ROI area check error.\n");
  send_warning_orphan_msg(hinst->instance_id, WARN_ROIREGION);
  return -1;
}

static int32_t check_area_vp8(v4l2_enc_inst *hinst, VP8EncPictureArea *area, int32_t width, int32_t height)
{
    int32_t w = (width+15)/16;
    int32_t h = (height+15)/16;

    if ((area->left < w) && (area->right < w) &&
        (area->top < h) && (area->bottom < h)) return 0;

    HANTRO_LOG(HANTRO_LEVEL_ERROR, "ROI area check error.\n");
    send_warning_orphan_msg(hinst->instance_id, WARN_ROIREGION);
    return -1;
}

/**
 * @brief set_coding_control_h264(), set coding control.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_coding_control_h264(v4l2_enc_inst *h,
                                    v4l2_daemon_enc_params *enc_params) {
  H264EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;
  H264EncCodingCtrl *codingCfg = &h264_params->video_coding_ctrl;

  if ((ret = H264EncGetCodingCtrl(h->inst, codingCfg)) != H264ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncGetCodingCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  codingCfg->idrHeader = enc_params->specific.enc_h26x_cmd.idrHdr;
  if (enc_params->specific.enc_h26x_cmd.sliceSize != -1)
    codingCfg->sliceSize = enc_params->specific.enc_h26x_cmd.sliceSize;
  if (enc_params->specific.enc_h26x_cmd.enableCabac != -1)
    codingCfg->enableCabac = enc_params->specific.enc_h26x_cmd.enableCabac;
  codingCfg->videoFullRange = 0;
  if (enc_params->specific.enc_h26x_cmd.videoRange != -1)
    codingCfg->videoFullRange = enc_params->specific.enc_h26x_cmd.videoRange;
  codingCfg->disableDeblockingFilter =
      (enc_params->specific.enc_h26x_cmd.disableDeblocking != 0);
  if (enc_params->specific.enc_h26x_cmd.sei)
    codingCfg->seiMessages = 1;
  else
    codingCfg->seiMessages = 0;
  codingCfg->gdrDuration = enc_params->specific.enc_h26x_cmd.gdrDuration;
  codingCfg->fieldOrder = enc_params->specific.enc_h26x_cmd.fieldOrder;
  codingCfg->cirStart = enc_params->specific.enc_h26x_cmd.cirStart;
  codingCfg->cirInterval = enc_params->specific.enc_h26x_cmd.cirInterval;

  int32_t encode_width = enc_params->general.width;
  int32_t encode_height = enc_params->general.height;

  if (codingCfg->gdrDuration == 0) {
    codingCfg->intraArea.top = enc_params->specific.enc_h26x_cmd.intraAreaTop;
    codingCfg->intraArea.left = enc_params->specific.enc_h26x_cmd.intraAreaLeft;
    codingCfg->intraArea.bottom =
        enc_params->specific.enc_h26x_cmd.intraAreaBottom;
    codingCfg->intraArea.right =
        enc_params->specific.enc_h26x_cmd.intraAreaRight;
    codingCfg->intraArea.enable =
        enc_params->specific.enc_h26x_cmd.intraAreaEnable;
  } else {
    // intraArea will be used by GDR, customer can not use intraArea when GDR is
    // enabled.
    codingCfg->intraArea.enable = 0;
  }
  if (codingCfg->gdrDuration == 0) {
    codingCfg->roi1Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[0];
    codingCfg->roi1Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[0];
    codingCfg->roi1Area.bottom =
        enc_params->specific.enc_h26x_cmd.roiAreaBottom[0];
    codingCfg->roi1Area.right =
        enc_params->specific.enc_h26x_cmd.roiAreaRight[0];
    codingCfg->roi1Area.enable =
        enc_params->specific.enc_h26x_cmd.roiAreaEnable[0];
    codingCfg->roi1DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[0];
    if (check_area_h264(h, &codingCfg->roi1Area, encode_width, encode_height) !=
            0 ||
        codingCfg->roi1DeltaQp == 0)
      codingCfg->roi1Area.enable = 0;
#if 0  // hard code for test
        codingCfg->roi1Area.top = 0;
        codingCfg->roi1Area.left = 0;
        codingCfg->roi1Area.bottom = 3;
        codingCfg->roi1Area.right = 3;
        codingCfg->roi1Area.enable = 1;
        codingCfg->roi1DeltaQp = -5;
#endif
  } else {
    codingCfg->roi1Area.enable = 0;
  }

  codingCfg->roi2Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[1];
  codingCfg->roi2Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[1];
  codingCfg->roi2Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[1];
  codingCfg->roi2Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[1];
  codingCfg->roi2Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[1];
  codingCfg->roi2DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[1];
  if (check_area_h264(h, &codingCfg->roi2Area, encode_width, encode_height) != 0 ||
      codingCfg->roi2DeltaQp == 0)
    codingCfg->roi2Area.enable = 0;

  codingCfg->chroma_qp_offset = enc_params->specific.enc_h26x_cmd.chromaQpOffset;

  if ((ret = H264EncSetCodingCtrl(
            h->inst, &h264_params->video_coding_ctrl)) != H264ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncSetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief set_coding_control_vp8(), set coding control.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_coding_control_vp8(v4l2_enc_inst *h,
                                   v4l2_daemon_enc_params *enc_params) {
  VP8EncRet ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;

  VP8EncCodingCtrl *codingCfg = &vp8_params->vp8_coding_ctrl;
  int32_t encode_width = enc_params->general.width;
  int32_t encode_height = enc_params->general.height;

  if ((ret = VP8EncGetCodingCtrl(h->inst, codingCfg)) != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncGetCodingCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  codingCfg->cirStart = enc_params->specific.enc_h26x_cmd.cirStart;
  codingCfg->cirInterval = enc_params->specific.enc_h26x_cmd.cirInterval;

  codingCfg->roi1Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[0];
  codingCfg->roi1Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[0];
  codingCfg->roi1Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[0];
  codingCfg->roi1Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[0];
  codingCfg->roi1Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[0];
  codingCfg->roi1DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[0];
  if (check_area_vp8(h, &codingCfg->roi1Area, encode_width, encode_height) != 0 ||
      codingCfg->roi1DeltaQp == 0)
    codingCfg->roi1Area.enable = 0;

  codingCfg->roi2Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[1];
  codingCfg->roi2Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[1];
  codingCfg->roi2Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[1];
  codingCfg->roi2Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[1];
  codingCfg->roi2Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[1];
  codingCfg->roi2DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[1];
  if (check_area_vp8(h, &codingCfg->roi2Area, encode_width, encode_height) != 0 ||
      codingCfg->roi2DeltaQp == 0)
    codingCfg->roi2Area.enable = 0;

  if ((ret = VP8EncSetCodingCtrl(h->inst, codingCfg)) != VP8ENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncSetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief set_vui_h264(), set vui info to encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_vui_h264(v4l2_enc_inst *h, v4l2_daemon_enc_params *enc_params) {
  H264EncRet ret;
  ret = H264EncSetVuiColorDescription(
      h->inst,
      enc_params->specific.enc_h26x_cmd.vuiVideoSignalTypePresentFlag,
      enc_params->specific.enc_h26x_cmd.vuiVideoFormat,
      enc_params->specific.enc_h26x_cmd.vuiColorDescripPresentFlag,
      enc_params->specific.enc_h26x_cmd.vuiColorPrimaries,
      enc_params->specific.enc_h26x_cmd.vuiTransferCharacteristics,
      enc_params->specific.enc_h26x_cmd.vuiMatrixCoefficients);

  if (ret != H264ENC_OK) return DAEMON_ERR_ENC_INTERNAL;

  return 0;
}

/**
 * @brief set_vui_vp8(), set vui info to encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return int: 0: succeed, Others: failed
 */
static int set_vui_vp8(v4l2_enc_inst *h, v4l2_daemon_enc_params *enc_params) {
  (void)h;
  (void)enc_params;
  return 0;
}


/**
 * @brief encoder_init_h1(), init H1 data structures.
 * @param v4l2_enc_inst* h, encoder instance.
 * @return .
 */
void encoder_init_h1(v4l2_enc_inst *h) {
  h1_enc_params *params = (h1_enc_params *)h->enc_params;

  // do nothing, as the memory has been set to 0 when create encoder instance.
  if(h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    params->init_encoder = init_encoder_h264;
    params->set_coding_ctrl = set_coding_control_h264;
    params->set_rate_ctrl = set_rate_control_h264;
    params->set_pre_process = set_pre_process_h264;
    params->set_vui = set_vui_h264;
  } else if(h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    params->init_encoder = init_encoder_vp8;
    params->set_coding_ctrl = set_coding_control_vp8;
    params->set_rate_ctrl = set_rate_control_vp8;
    params->set_pre_process = set_pre_process_vp8;
    params->set_vui = set_vui_vp8;
  }
}

/**
 * @brief encoder_check_codec_format_h1(), before encode, confirm if hw support
 * this format.
 * @param v4l2_enc_inst* h, encoder instance.
 * @return int, 0 is supported, -1 is unsupported.
 */
int encoder_check_codec_format_h1(v4l2_enc_inst *h) {
  EWLHwConfig_t configure_t;

#if (defined(NXP) && defined(USE_H1))  // h1 single core
  configure_t = EWLReadAsicConfig();
  h->configure_t.has_h264 |= configure_t.h264Enabled;
  h->configure_t.has_jpeg |= configure_t.jpegEnabled;
  h->configure_t.has_vp8 |= configure_t.vp8Enabled;
  if (h->configure_t.has_h264) {
    h->configure_t.max_width_h264 = configure_t.maxEncodedWidth;
  }
  if (h->configure_t.has_vp8) {
    h->configure_t.max_width_vp8 = configure_t.maxEncodedWidth;
  }
#endif
  if ((h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) &&
      (h->configure_t.has_vp8 == 1))
    return 0;
  else if ((h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) &&
           (h->configure_t.has_h264 == 1))
    return 0;
  else
    return -1;
}

/**
 * @brief encoder_get_input_h1(), write message from V4l2 parameter to encoder
 * instance.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param int32_t if_config: if init gop config of encIn.
 * @return void.
 */
void encoder_get_input_h1(v4l2_enc_inst *h, v4l2_daemon_enc_params *enc_params,
                          int32_t if_config) {
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;
  uint32_t size_lum = 0;
  uint32_t size_ch = 0;

  H264EncIn *h264EncIn = NULL;
  VP8EncIn *vp8EncIn = NULL;

  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    h264EncIn = &h264_params->video_in;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    vp8EncIn = &vp8_params->vp8_enc_in;
  }
  size_lum = ((enc_params->general.lumWidthSrc + 15) & (~15)) *
             enc_params->general.lumHeightSrc;  // Now 8 bit yuv only
  size_ch = size_lum / 2;

#ifndef USE_HW
  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    h264EncIn->busLuma =
        (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busLuma,
                                    enc_params->io_buffer.busLumaSize);
    if (enc_params->io_buffer.busChromaU != 0)
      h264EncIn->busChromaU =
          (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busChromaU,
                                      enc_params->io_buffer.busChromaUSize);
    else
      h264EncIn->busChromaU = h264EncIn->busLuma + size_lum;

    if (enc_params->io_buffer.busChromaV != 0)
      h264EncIn->busChromaV =
          (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busChromaV,
                                      enc_params->io_buffer.busChromaVSize);
    else
      h264EncIn->busChromaV = h264EncIn->busChromaU + size_ch / 2;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    vp8EncIn->busLuma =
        (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busLuma,
                                    enc_params->io_buffer.busLumaSize);
    if (enc_params->io_buffer.busChromaU != 0)
      vp8EncIn->busChromaU =
          (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busChromaU,
                                      enc_params->io_buffer.busChromaUSize);
    else
      vp8EncIn->busChromaU = vp8EncIn->busLuma + size_lum;

    if (enc_params->io_buffer.busChromaV != 0)
      vp8EncIn->busChromaV =
          (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busChromaV,
                                      enc_params->io_buffer.busChromaVSize);
    else
      vp8EncIn->busChromaV = vp8EncIn->busChromaU + size_ch / 2;
  }
#else
  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    h264EncIn->busLuma = enc_params->io_buffer.busLuma;
    if (enc_params->io_buffer.busChromaU != 0)
      h264EncIn->busChromaU = enc_params->io_buffer.busChromaU;
    else
      h264EncIn->busChromaU = h264EncIn->busLuma + size_lum;
    if (enc_params->io_buffer.busChromaV != 0)
      h264EncIn->busChromaV = enc_params->io_buffer.busChromaV;
    else
      h264EncIn->busChromaV = h264EncIn->busChromaU + size_ch / 2;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    vp8EncIn->busLuma = enc_params->io_buffer.busLuma;
    if (enc_params->io_buffer.busChromaU != 0)
      vp8EncIn->busChromaU = enc_params->io_buffer.busChromaU;
    else
      vp8EncIn->busChromaU = vp8EncIn->busLuma + size_lum;
    if (enc_params->io_buffer.busChromaV != 0)
      vp8EncIn->busChromaV = enc_params->io_buffer.busChromaV;
    else
      vp8EncIn->busChromaV = vp8EncIn->busChromaU + size_ch / 2;
  }
#endif
  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    h->saveOriOutBuf = h264EncIn->pOutBuf =
        mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busOutBuf,
                             enc_params->io_buffer.outBufSize);
#ifdef USE_HW
    h264EncIn->busOutBuf = enc_params->io_buffer.busOutBuf;
#else
    h264EncIn->busOutBuf = (ptr_t)h264EncIn->pOutBuf;
#endif
    h264EncIn->outBufSize = enc_params->io_buffer.outBufSize;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    h->saveOriOutBuf = vp8EncIn->pOutBuf =
        mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busOutBuf,
                             enc_params->io_buffer.outBufSize);
#ifdef USE_HW
    vp8EncIn->busOutBuf = enc_params->io_buffer.busOutBuf;
#else
    vp8EncIn->busOutBuf = (ptr_t)vp8EncIn->pOutBuf;
#endif
    vp8EncIn->outBufSize = enc_params->io_buffer.outBufSize;
  }

  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    if (enc_params->general.outputRateDenom != 0)
      h264EncIn->timeIncrement = enc_params->general.outputRateDenom;
    else
      h264EncIn->timeIncrement = 1;
    if (enc_params->specific.enc_h26x_cmd.force_idr) {
      h264EncIn->codingType = H264ENC_INTRA_FRAME;
      h->next_pic_type = (int)H264ENC_INTRA_FRAME;
    }
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    if (enc_params->general.outputRateDenom != 0)
      vp8EncIn->timeIncrement = enc_params->general.outputRateDenom;
    else
      vp8EncIn->timeIncrement = 1;

    if (enc_params->specific.enc_h26x_cmd.force_idr) {
      vp8EncIn->codingType = VP8ENC_INTRA_FRAME;
      h->next_pic_type = (int)VP8ENC_INTRA_FRAME;
    }
  }
}

/**
 * @brief encoder_set_parameter_h1(), set encoder parameters.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param int32_t if_config: if configure the encoder.
 * @return int: api return value.
 */
int encoder_set_parameter_h1(v4l2_enc_inst *h,
                             v4l2_daemon_enc_params *enc_params,
                             int32_t if_config) {
  int ret;
  h1_enc_params *params = (h1_enc_params *)h->enc_params;

  encoder_get_input_h1(h, enc_params, if_config);

  /* Encoder setup: init instance.*/
  if (if_config == 1) {
    ret = params->init_encoder(h, enc_params);
    if (ret) return ret;

    h->total_frames = 0;
  }

  /* Encoder setup: rate control */
  if ((if_config == 1) || (enc_params->specific.enc_h26x_cmd.picRc == 0) ||
      (enc_params->specific.enc_h26x_cmd.hrdConformance != 1 &&
       if_config != 1)) {
    ret = params->set_rate_ctrl(h, enc_params);
    if (ret) return ret;
  }

  /*Check if the cbr bit rate is changed.*/
  check_if_cbr_bitrate_changed(h, enc_params);

  /*Encoder setup: PreP setup */
  ret = params->set_pre_process(h, enc_params);
  if (ret) return ret;

  /* Encoder setup: coding control */
  ret = params->set_coding_ctrl(h, enc_params);
  if (ret) return ret;

  /* Encoder setup: color description */
  ret = params->set_vui(h, enc_params);
  if (ret) return ret;

  return 0;
}

/**
 * @brief encoder_start_h1(), start encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @return int: api return value.
 */
int encoder_start_h1(v4l2_enc_inst *h, uint32_t *stream_size) {
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;

  H264EncIn *h264EncIn = NULL;
  H264EncOut *h264EncOut = NULL;
  VP8EncIn *vp8EncIn = NULL;
  u8 *pOutBufTmp = NULL;
  int ret = 0;

  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    int encRet;
    h264EncIn = &h264_params->video_in;
    h264EncOut = &h264_params->video_out;
    pOutBufTmp = (u8 *)h264EncIn->pOutBuf;
    encRet = H264EncStrmStart(h->inst, h264EncIn, h264EncOut);
    *stream_size = h264EncOut->streamSize;

    ret = (encRet == H264ENC_OK) ? 0 : DAEMON_ERR_ENC_INTERNAL;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    vp8EncIn = &vp8_params->vp8_enc_in;
    pOutBufTmp = (u8 *)vp8EncIn->pOutBuf;
#ifdef OUTPUT_VP8_HEADER
    IvfHeader(pOutBufTmp, vp8_params->vp8_config.width,
              vp8_params->vp8_config.height,
              vp8_params->vp8_config.frameRateNum,
              vp8_params->vp8_config.frameRateDenom, 0);
    *stream_size = IVF_HDR_BYTES + IVF_FRM_BYTES;
#else
    *stream_size = 0;
#endif
    ret = 0;
  }
  memset(pOutBufTmp + *stream_size, 0,
         (*stream_size + 7) / 8 * 8 - *stream_size);
  *stream_size = (*stream_size + 7) / 8 * 8;  // vc8000nanoe need 8 allign
  return ret;
}

/**
 * @brief encoder_encode_h1(), encoder encode.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @param uint32_t* codingType: coding type.
 * @return int: api return value.
 */
int encoder_encode_h1(v4l2_enc_inst *h, uint32_t *stream_size,
                      uint32_t *codingType) {
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;

  H264EncIn *h264EncIn = NULL;
  H264EncOut *h264EncOut = NULL;
  VP8EncIn *vp8EncIn = NULL;
  VP8EncOut *vp8EncOut = NULL;
  int ret = 0;
  int encRet;
#ifdef OUTPUT_VP8_HEADER
  u8 *pOutBufTmp = NULL;
  uint32_t payload_size = 0;
#endif

  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    h264EncIn = &h264_params->video_in;
    h264EncOut = &h264_params->video_out;
    h264EncIn->pOutBuf += *stream_size / 4;
    h264EncIn->busOutBuf += *stream_size;
    h264EncIn->outBufSize -= *stream_size;
    if (h264EncIn->busOutBuf % 8 != 0) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Input address not alligned to 8\n");
    }
    encRet = H264EncStrmEncode(h->inst, h264EncIn, h264EncOut, NULL, NULL, NULL);
    *stream_size += h264EncOut->streamSize;
    if ((h264EncOut->codingType == H264ENC_INTRA_FRAME) ||
        ((h264EncIn->codingType == H264ENC_INTRA_FRAME) &&
         (h264_params->video_coding_ctrl.gdrDuration > 0))) {
      params->intraPeriodCnt = 0;
    }
    if (h264_params->video_out.codingType != H264ENC_NOTCODED_FRAME) {
      params->intraPeriodCnt++;
    }
    *codingType = h264EncIn->codingType;
    if (encRet == H264ENC_FRAME_READY) ret = DAEMON_ENC_FRAME_READY;
    else ret = DAEMON_ERR_ENC_INTERNAL;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    vp8EncIn = &vp8_params->vp8_enc_in;
    vp8EncOut = &vp8_params->vp8_enc_out;
#ifdef OUTPUT_VP8_HEADER
    pOutBufTmp = (u8 *)vp8EncIn->pOutBuf;
#endif
    vp8EncIn->pOutBuf += *stream_size / 4;
    vp8EncIn->busOutBuf += *stream_size;
    vp8EncIn->outBufSize -= *stream_size;
    if (vp8EncIn->busOutBuf % 8 != 0) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Input address not alligned to 8\n");
    }
    encRet = VP8EncStrmEncode(h->inst, vp8EncIn, vp8EncOut);
    for (int itmp = 0; itmp < 9; itmp++) {
      if (itmp > 0 && vp8EncOut->streamSize[itmp] > 0)
        memcpy(vp8EncIn->pOutBuf, vp8EncOut->pOutBuf[itmp],
               vp8EncOut->streamSize[itmp]);
      vp8EncIn->pOutBuf =
          (uint32_t *)((u8 *)vp8EncIn->pOutBuf + vp8EncOut->streamSize[itmp]);
      *stream_size += vp8EncOut->streamSize[itmp];
#ifdef OUTPUT_VP8_HEADER
      payload_size += vp8EncOut->streamSize[itmp];
#endif
    }
    if ((vp8EncIn->codingType == VP8ENC_INTRA_FRAME ||
        vp8EncOut->codingType == VP8ENC_INTRA_FRAME) &&
        (vp8_params->vp8_rate_ctrl.intraPictureRate))
            params->intraPeriodCnt = 0;

    if (vp8EncOut->codingType != VP8ENC_NOTCODED_FRAME)
        params->intraPeriodCnt++;
    *codingType = vp8EncIn->codingType;
#ifdef OUTPUT_VP8_HEADER
    IvfFrame(pOutBufTmp + IVF_HDR_BYTES, payload_size, params->frameCntTotal);
    memmove(pOutBufTmp + IVF_HDR_BYTES + IVF_FRM_BYTES,
            pOutBufTmp + *stream_size - payload_size, payload_size);
    *stream_size -= *stream_size - payload_size - IVF_HDR_BYTES - IVF_FRM_BYTES;
    params->frameCntTotal++;
#endif
    if (encRet == VP8ENC_FRAME_READY) ret = DAEMON_ENC_FRAME_READY;
    else ret = DAEMON_ERR_ENC_INTERNAL;
  }
  return ret;
}

/**
 * @brief encoder_find_next_pic_h1(), find buffer and gop structure of next
 * picture.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param BUFFER** p_buffer: buffer to encode
 * @param uint32_t* list_num: buffer id in buffer list.
 * @return int: api return value.
 */
int encoder_find_next_pic_h1(v4l2_enc_inst *h, BUFFER **p_buffer,
                             uint32_t *list_num) {
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;
  h1_vp8_enc_params *vp8_params = &params->param.vp8;

  *p_buffer = bufferlist_get_buffer(h->bufferlist_reorder);
  *list_num = 0;
  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    /* Select frame type */
    if ((h264_params->video_rate_ctrl.gopLen != 0) &&
        (params->intraPeriodCnt >= h264_params->video_rate_ctrl.gopLen)) {
      h264_params->video_in.codingType = H264ENC_INTRA_FRAME;
      h->next_pic_type = (int)H264ENC_INTRA_FRAME;
    } else {
      h264_params->video_in.codingType = H264ENC_PREDICTED_FRAME;
      h264_params->video_in.ipf = H264ENC_REFERENCE_AND_REFRESH;
      h264_params->video_in.ltrf = H264ENC_REFERENCE;
      h->next_pic_type = (int)H264ENC_PREDICTED_FRAME;
    }
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
      if (vp8_params->vp8_rate_ctrl.intraPictureRate &&
          params->intraPeriodCnt >= vp8_params->vp8_rate_ctrl.intraPictureRate) {
          vp8_params->vp8_enc_in.codingType = VP8ENC_INTRA_FRAME;
          h->next_pic_type = (int)VP8ENC_INTRA_FRAME;
      } else {
          vp8_params->vp8_enc_in.codingType = VP8ENC_PREDICTED_FRAME;
          vp8_params->vp8_enc_in.ipf = VP8ENC_REFERENCE_AND_REFRESH;
          h->next_pic_type = (int)VP8ENC_PREDICTED_FRAME;
      }
  }
  return 0;
}

/**
 * @brief reset_enc_h1(), reset encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int: api return value.
 */
void reset_enc_h1(v4l2_enc_inst *h)
{
  h->input_frame_cnt = 0;
  h->output_frame_cnt = 0;
}

/**
* @brief encoder_end_h1(), stream end, write eos.
* @param v4l2_enc_inst* h: encoder instance.
* @param uint32_t* stream_size: srteam size.
* @return int: api return value.
*/
int encoder_end_h1(v4l2_enc_inst *h, uint32_t *stream_size) {
  h1_enc_params *params = (h1_enc_params *)h->enc_params;
  h1_h264_enc_params *h264_params = &params->param.h264;
  //    h1_vp8_enc_params *vp8_params = &params->param.vp8;

  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    H264EncRet ret;
    ret = H264EncStrmEnd(h->inst, &h264_params->video_in,
                         &h264_params->video_out);
    if (ret != H264ENC_OK) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncStrmEnd() failed. vsi_ret=%d\n",
                 ret);
      return (int)ret;
    }
    return (int)ret;
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
    return 0;  // no eos
  }
  return 0;
}

/**
 * @brief encoder_close_h1(), close encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int: api return value.
 */
int encoder_close_h1(v4l2_enc_inst *h) {
  int ret = 0;
  if (h->inst != NULL) {
    if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
      if ((ret = H264EncRelease(h->inst)) != H264ENC_OK) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "H264EncRelease() failed. vsi_ret=%d\n",
                   ret);
      }
    } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_VP8) {
      if ((ret = VP8EncRelease(h->inst)) != VP8ENC_OK) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "VP8EncRelease() failed. vsi_ret=%d\n",
                   ret);
      }
    }
    h->inst = NULL;
  }
  if(h->cbr_bitrate_change) return ret;
  if (h->bufferlist_input) bufferlist_flush(h->bufferlist_input);
  if (h->bufferlist_output) bufferlist_flush(h->bufferlist_output);
  if (h->bufferlist_reorder) bufferlist_flush(h->bufferlist_reorder);
  return ret;
}

/**
 * @brief encoder_get_attr_h1(), get encoder attributes.
 * @param v4l2_daemon_codec_fmt fmt: codec format.
 * @return
 */
void encoder_get_attr_h1(v4l2_daemon_codec_fmt fmt, enc_inst_attr *attr) {
  ASSERT(attr);

  attr->param_size = sizeof(h1_enc_params);
}
