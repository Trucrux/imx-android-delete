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

#ifndef __VSI_ENC_VIDEO_H2_PRIV_H__
#define __VSI_ENC_VIDEO_H2_PRIV_H__
#include "ewl.h"
#include "h264encapi.h"
#include "vp8encapi.h"

typedef struct {
  H264EncIn video_in;
  H264EncOut video_out;

  H264EncConfig video_config;
  H264EncRateCtrl video_rate_ctrl;
  H264EncCodingCtrl video_coding_ctrl;
  H264EncPreProcessingCfg video_prep;
} h1_h264_enc_params;

typedef struct {
  VP8EncIn vp8_enc_in;
  VP8EncOut vp8_enc_out;

  VP8EncConfig vp8_config;
  VP8EncRateCtrl vp8_rate_ctrl;
  VP8EncCodingCtrl vp8_coding_ctrl;
  VP8EncPreProcessingCfg vp8_prep;
} h1_vp8_enc_params;

typedef struct {
  union {
    h1_h264_enc_params h264;
    h1_vp8_enc_params vp8;
  } param;

  int32_t intraPeriodCnt;
  uint64_t frameCntTotal;

  int (*init_encoder)(v4l2_enc_inst *h, v4l2_daemon_enc_params *params);
  int (*set_rate_ctrl)(v4l2_enc_inst *h, v4l2_daemon_enc_params *params);
  int (*set_pre_process)(v4l2_enc_inst *h, v4l2_daemon_enc_params *params);
  int (*set_coding_ctrl)(v4l2_enc_inst *h, v4l2_daemon_enc_params *params);
  int (*set_vui)(v4l2_enc_inst *h, v4l2_daemon_enc_params *params);

} h1_enc_params;

#endif /*__VSI_ENC_VIDEO_H2_PRIV_H__*/
