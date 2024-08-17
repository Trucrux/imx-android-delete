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
#include "hevcencapi.h"

typedef struct {
  int8_t *gopCfg;
  uint32_t gopLowdelay;
  int32_t codecFormat; /* enable H264 encoding instead of HEVC */
  int32_t interlacedFrame;
  uint32_t longTermGap;
  uint32_t longTermGapOffset;
  uint32_t ltrInterval;
  int32_t intraPicRate; /* IDR interval */
  int32_t bFrameQpDelta;
  uint32_t ctuPerCol;
  uint32_t ctuPerRow;
  uint32_t gopSize;
  int32_t longTermQpDelta;
  uint32_t pass;
  int32_t gdrDuration;
} commandLine_local_s;

typedef struct {
  VCEncIn video_in;
  VCEncExtParaIn video_ext_in;
  VCEncOut video_out;

  VCEncConfig video_config;
  VCEncRateCtrl video_rate_ctrl;
  VCEncCodingCtrl video_coding_ctrl;
  VCEncPreProcessingCfg video_prep;

  uint8_t gopCfgOffset[MAX_GOP_SIZE + 1];
  VCEncGopPicConfig gopPicCfg[MAX_GOP_PIC_CONFIG_NUM];
  // VCEncGopPicConfig gopPicCfgPass2[MAX_GOP_PIC_CONFIG_NUM];
  VCEncGopPicSpecialConfig gopPicSpecialCfg[MAX_GOP_SPIC_CONFIG_NUM];
  commandLine_local_s cml;
} h2_enc_params;

#endif /*__VSI_ENC_VIDEO_H2_PRIV_H__*/
