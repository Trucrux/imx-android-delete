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
#include <math.h>

#include "buffer_list.h"
#include "fifo.h"
#include "vsi_daemon_debug.h"
#include "vsi_enc.h"
#include "vsi_enc_img_h2.h"
#include "vsi_enc_img_h2_priv.h"
#include "vsi_enc_video_h2.h"
#include "vsi_enc_video_h2_priv.h"

//#include "rate_control_picture.h"

#define LEAST_MONITOR_FRAME 3
#define MOVING_AVERAGE_FRAMES 120
#define MAX_GOP_LEN 8

#define VCENC_MIN_ENC_WIDTH 132  // confirm with ZhangKe, 132x128
#define VCENC_MAX_ENC_WIDTH 8192
#define VCENC_MIN_ENC_HEIGHT 128
#define VCENC_MAX_ENC_HEIGHT 8192
#define VCENC_MAX_ENC_HEIGHT_EXT 8640
// No limits of MAX_MBS_PER_PIC

i32 VCEncSetVuiColorDescription(
    VCEncInst inst, u32 vuiVideoSignalTypePresentFlag, u32 vuiVideoFormat,
    u32 vuiColorDescripPresentFlag, u32 vuiColorPrimaries,
    u32 vuiTransferCharacteristics, u32 vuiMatrixCoefficients);
/**
 * @brief Below are the default GOP configures. From left to right is:
          Type POC QPoffset  QPfactor  num_ref_pics ref_pics  used_by_cur
 */
int8_t *RpsDefault_GOPSize_1[] = {
    "Frame1:  P    1   0        0.4     0      1        -1         1", NULL,
};
int8_t *RpsDefault_H264_GOPSize_1[] = {
    "Frame1:  P    1   0        0.4     0      1        -1         1", NULL,
};

int8_t *RpsDefault_GOPSize_2[] = {
    "Frame1:  P    2   0        0.6     0      1        -2         1",
    "Frame2:  B    1   0        0.5     0      2        -1 1       1 1", NULL,
};

int8_t *RpsDefault_GOPSize_3[] = {
    "Frame1:  P    3   0        0.5     0      1        -3         1   ",
    "Frame2:  B    1   0        0.442     0      2        -1 2       1 1 ",
    "Frame3:  B    2   0        0.442   0      2        -1 1       1 1 ", NULL,
};

int8_t *RpsDefault_GOPSize_4[] = {
    "Frame1:  P    4   0        0.5     0      1        -4         1   ",
    "Frame2:  B    1   0        0.442   0      2        -1 3       1 1 ",
    "Frame3:  B    2   0        0.442   0      2        -2 2       1 1 ",
    "Frame4:  B    3   0        0.442   0      2        -3 1       1 1 ",
    NULL,
};

int8_t *RpsDefault_GOPSize_5[] = {
    "Frame1:  P    5   0        0.442   0      1        -5         1   ",
    "Frame2:  B    1   0        0.3536  0      2        -1 4       1 1 ",
    "Frame3:  B    2   0        0.3536  0      2        -2 3       1 1 ",
    "Frame4:  B    3   0        0.3536  0      2        -3 2       1 1 ",
    "Frame5:  B    4   0        0.3536  0      2        -4 1       1 1 ",
    NULL,
};

int8_t *RpsDefault_GOPSize_6[] = {
    "Frame1:  P    6   0        0.442   0      1        -6         1   ",
    "Frame2:  B    1   0        0.3536  0      2        -1 5       1 1 ",
    "Frame3:  B    2   0        0.3536  0      2        -2 4       1 1 ",
    "Frame4:  B    3   0        0.3536  0      2        -3 3       1 1 ",
    "Frame5:  B    4   0        0.3536  0      2        -4 2       1 1 ",
    "Frame6:  B    5   0        0.3536  0      2        -5 1       1 1 ",
    NULL,
};

int8_t *RpsDefault_GOPSize_7[] = {
    "Frame1:  P    7   0        0.442   0      1        -7         1   ",
    "Frame2:  B    1   0        0.3536  0      2        -1 6       1 1 ",
    "Frame3:  B    2   0        0.3536  0      2        -2 5       1 1 ",
    "Frame4:  B    3   0        0.3536  0      2        -3 4       1 1 ",
    "Frame5:  B    4   0        0.3536  0      2        -4 3       1 1 ",
    "Frame6:  B    5   0        0.3536  0      2        -5 2       1 1 ",
    "Frame7:  B    6   0        0.3536  0      2        -6 1       1 1 ",
    NULL,
};

int8_t *RpsDefault_GOPSize_8[] = {
    "Frame1:  P    8   0        0.442   0      1        -8         1   ",
    "Frame2:  B    1   0        0.221  0      2        -1 7       1 1 ",
    "Frame3:  B    2   0        0.221  0      2        -2 6       1 1 ",
    "Frame4:  B    3   0        0.221  0      2        -3 5       1 1 ",
    "Frame5:  B    4   0        0.221  0      2        -4 4       1 1 ",
    "Frame6:  B    5   0        0.221  0      2        -5 3       1 1 ",
    "Frame7:  B    6   0        0.221  0      2        -6 2       1 1 ",
    "Frame8:  B    7   0        0.221  0      2        -7 1       1 1",
    NULL,
};

int8_t *RpsDefault_Interlace_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8       0   2           -1 -2     0 1", NULL,
};

int8_t *RpsLowdelayDefault_GOPSize_1[] = {
    "Frame1:  B    1   0        0.65      0     2       -1 -2         1 1",
    NULL,
};

int8_t *RpsLowdelayDefault_GOPSize_2[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -3         1 1",
    "Frame2:  B    2   0        0.578     0     2       -1 -2         1 1",
    NULL,
};

int8_t *RpsLowdelayDefault_GOPSize_3[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -4         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.578     0     2       -1 -3         1 1",
    NULL,
};

int8_t *RpsLowdelayDefault_GOPSize_4[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -5         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.4624    0     2       -1 -3         1 1",
    "Frame4:  B    4   0        0.578     0     2       -1 -4         1 1",
    NULL,
};

int8_t *RpsPass2_GOPSize_4[] = {
    "Frame1:  B    4   0        0.5      0     2       -4 -8      1 1",
    "Frame2:  B    2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  B    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  B    3   0        0.5      0     3       -1 -3 1    1 0 1",
    NULL,
};

int8_t *RpsPass2_GOPSize_8[] = {
    "Frame1:  B    8   0        0.442    0  2           -8 -16    1 1",
    "Frame2:  B    4   0        0.3536   0  2           -4 4      1 1",
    "Frame3:  B    2   0        0.3536   0  3           -2 2 6    1 1 0",
    "Frame4:  B    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  B    3   0        0.68     0  4           -1 -3 1 5 1 0 1 0",
    "Frame6:  B    6   0        0.3536   0  3           -2 -6 2   1 0 1",
    "Frame7:  B    5   0        0.68     0  4           -1 -5 1 3 1 0 1 0",
    "Frame8:  B    7   0        0.68     0  3           -1 -7 1   1 0 1",
    NULL,
};

int8_t *RpsPass2_GOPSize_2[] = {
    "Frame1:  B    2   0        0.6     0      2        -2 -4      1 1",
    "Frame2:  B    1   0        0.68    0      2        -1 1       1 1", NULL,
};

/**
 * @brief init_cmdl(), init command line parameters.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param VCEncIn* encIn: encoder input parameters.
 * @return void.
 */
static void init_cmdl(v4l2_enc_inst *h, VCEncIn *encIn) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;

  memset(&params->cml, 0, sizeof(params->cml));
  params->cml.bFrameQpDelta = 0;
  params->cml.codecFormat = 0;
  params->cml.interlacedFrame = 0;
  params->cml.intraPicRate = 30;
  params->cml.gdrDuration = 0;
  params->cml.pass = 0;
  params->cml.gopSize = encIn->gopSize;
  params->cml.ltrInterval = -1;
}

/**
 * @brief nextToken(), string parse.
 * @param int8_t *str: input string.
 * @return int8_t*.
 */
static int8_t *nextToken(const char *str) {
  int8_t *p = strchr(str, ' ');
  if (p) {
    while (*p == ' ') p++;
    if (*p == '\0') p = NULL;
  }
  return p;
}

/**
 * @brief ParseGopConfigString(), string parse.
 * @param int8_t *line: input string line.
 * @param VCEncGopConfig *gopCfg: gop config.
 * @param int32_t frame_idx: frame idx.
 * @param int32_t gopSize: gop size.
 * @return int32_t: 0 successful, other unsuccessful.
 */
static int32_t ParseGopConfigString(const char *line, VCEncGopConfig *gopCfg,
                                    int32_t frame_idx, int32_t gopSize) {
  if (!line) return -1;

  // format: FrameN Type POC QPoffset QPfactor  num_ref_pics ref_pics
  // used_by_cur
  int32_t frameN, poc, num_ref_pics, i;
  int8_t type;
  VCEncGopPicConfig *cfg = NULL;
  VCEncGopPicSpecialConfig *scfg = NULL;

  // frame idx
  sscanf(line, "Frame%d", &frameN);
  if ((frameN != (frame_idx + 1)) && (frameN != 0)) return -1;

  if (frameN > gopSize) return 0;

  if (0 == frameN) {
    // format: FrameN Type  QPoffset  QPfactor   TemporalId  num_ref_pics
    // ref_pics  used_by_cur  LTR    Offset   Interval
    scfg = &(gopCfg->pGopPicSpecialCfg[gopCfg->special_size++]);

    // frame type
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%c", &type);
    if (type == 'I' || type == 'i')
      scfg->codingType = VCENC_INTRA_FRAME;
    else if (type == 'P' || type == 'p')
      scfg->codingType = VCENC_PREDICTED_FRAME;
    else if (type == 'B' || type == 'b')
      scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
    else
      scfg->codingType = FRAME_TYPE_RESERVED;

    // qp offset
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &(scfg->QpOffset));

    // qp factor
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%lf", &(scfg->QpFactor));
    scfg->QpFactor = sqrt(scfg->QpFactor);

    // temporalId factor
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &(scfg->temporalId));

    // num_ref_pics
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &num_ref_pics);
    if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES) /* NUMREFPICS_RESERVED -1 */
    {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "GOP Config: Error, num_ref_pic can not be more than %d \n",
                 VCENC_MAX_REF_FRAMES);
      return -1;
    }
    scfg->numRefPics = num_ref_pics;

    if ((scfg->codingType == VCENC_INTRA_FRAME) && (0 == num_ref_pics))
      num_ref_pics = 1;
    // ref_pics
    for (i = 0; i < num_ref_pics; i++) {
      line = nextToken(line);
      if (!line) return -1;
      if ((strncmp(line, "L", 1) == 0) || (strncmp(line, "l", 1) == 0)) {
        sscanf(line, "%c%d", &type, &(scfg->refPics[i].ref_pic));
        scfg->refPics[i].ref_pic =
            LONG_TERM_REF_ID2DELTAPOC(scfg->refPics[i].ref_pic - 1);
      } else {
        sscanf(line, "%d", &(scfg->refPics[i].ref_pic));
      }
    }
    if (i < num_ref_pics) return -1;

    // used_by_cur
    for (i = 0; i < num_ref_pics; i++) {
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%u", &(scfg->refPics[i].used_by_cur));
    }
    if (i < num_ref_pics) return -1;

    // LTR
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &scfg->i32Ltr);
    if (VCENC_MAX_LT_REF_FRAMES < scfg->i32Ltr) return -1;

    // Offset
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &scfg->i32Offset);

    // Interval
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &scfg->i32Interval);

    if (0 != scfg->i32Ltr) {
      gopCfg->u32LTR_idx[gopCfg->ltrcnt] =
          LONG_TERM_REF_ID2DELTAPOC(scfg->i32Ltr - 1);
      gopCfg->ltrcnt++;
      if (VCENC_MAX_LT_REF_FRAMES < gopCfg->ltrcnt) return -1;
    }

    // short_change
    scfg->i32short_change = 0;
    if (0 == scfg->i32Ltr) {
      /* not long-term ref */
      scfg->i32short_change = 1;
      for (i = 0; i < num_ref_pics; i++) {
        if (IS_LONG_TERM_REF_DELTAPOC(scfg->refPics[i].ref_pic) &&
            (0 != scfg->refPics[i].used_by_cur)) {
          scfg->i32short_change = 0;
          break;
        }
      }
    }
  } else {
    // format: FrameN Type  POC  QPoffset    QPfactor   TemporalId  num_ref_pics
    // ref_pics  used_by_cur
    cfg = &(gopCfg->pGopPicCfg[gopCfg->size++]);

    // frame type
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%c", &type);
    if (type == 'P' || type == 'p')
      cfg->codingType = VCENC_PREDICTED_FRAME;
    else if (type == 'B' || type == 'b')
      cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
    else
      return -1;

    // poc
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &poc);
    if (poc < 1 || poc > gopSize) return -1;
    cfg->poc = poc;

    // qp offset
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &(cfg->QpOffset));

    // qp factor
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%lf", &(cfg->QpFactor));
    // sqrt(QpFactor) is used in calculating lambda
    cfg->QpFactor = sqrt(cfg->QpFactor);

    // temporalId factor
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &(cfg->temporalId));

    // num_ref_pics
    line = nextToken(line);
    if (!line) return -1;
    sscanf(line, "%d", &num_ref_pics);
    if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "GOP Config: Error, num_ref_pic can not be more than %d \n",
                 VCENC_MAX_REF_FRAMES);
      return -1;
    }

    // ref_pics
    for (i = 0; i < num_ref_pics; i++) {
      line = nextToken(line);
      if (!line) return -1;
      if ((strncmp(line, "L", 1) == 0) || (strncmp(line, "l", 1) == 0)) {
        sscanf(line, "%c%d", &type, &(cfg->refPics[i].ref_pic));
        cfg->refPics[i].ref_pic =
            LONG_TERM_REF_ID2DELTAPOC(cfg->refPics[i].ref_pic - 1);
      } else {
        sscanf(line, "%d", &(cfg->refPics[i].ref_pic));
      }
    }
    if (i < num_ref_pics) return -1;

    // used_by_cur
    for (i = 0; i < num_ref_pics; i++) {
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%u", &(cfg->refPics[i].used_by_cur));
    }
    if (i < num_ref_pics) return -1;

    cfg->numRefPics = num_ref_pics;
  }

  return 0;
}

/**
 * @brief ParseGopConfigFile(), parse gop config file.
 * @param int32_t gopSize: gop size.
 * @param int8_t *fname: input file name.
 * @param VCEncGopConfig *gopCfg: gop config.
 * @return int32_t: 0 successful, other unsuccessful.
 */
static int32_t ParseGopConfigFile(int32_t gopSize, int8_t *fname,
                                  VCEncGopConfig *gopCfg) {
#define MAX_LINE_LENGTH 1024
  int32_t frame_idx = 0, line_idx = 0, addTmp;
  int8_t achParserBuffer[MAX_LINE_LENGTH];
  FILE *fIn = fopen(fname, "r");
  if (fIn == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "GOP Config: Error, Can Not Open File %s\n",
               fname);
    return -1;
  }

  while (0 == feof(fIn)) {
    if (feof(fIn)) break;
    line_idx++;
    achParserBuffer[0] = '\0';
    // Read one line
    int8_t *line = fgets((char *)achParserBuffer, MAX_LINE_LENGTH, fIn);
    if (!line) break;
    // handle line end
    int8_t *s = strpbrk(line, "#\n");
    if (s) *s = '\0';

    addTmp = 1;
    line = strstr(line, "Frame");
    if (line) {
      if (0 == strncmp(line, "Frame0", 6)) addTmp = 0;

      if (ParseGopConfigString(line, gopCfg, frame_idx, gopSize) < 0) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid gop configure!\n");
        break;
      }

      frame_idx += addTmp;
    }
  }

  fclose(fIn);
  if (frame_idx != gopSize) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "GOP Config: Error, Parsing File %s Failed at Line %d\n", fname,
               line_idx);
    return -1;
  }
  return 0;
}

/**
 * @brief HEVCReadGopConfig(), parse gop config.
 * @param int8_t *fname: input file name.
 * @param int8_t **config: input config string.
 * @param VCEncGopConfig *gopCfg: gop config.
 * @param int32_t gopSize: gop size.
 * @param uint8_t *gopCfgOffset: gop config offset.
 * @return int32_t: 0 successful, other unsuccessful.
 */
static int32_t HEVCReadGopConfig(int8_t *fname, int8_t **config,
                                 VCEncGopConfig *gopCfg, int32_t gopSize,
                                 uint8_t *gopCfgOffset) {
  int32_t ret = -1;

  if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM) return -1;

  if (gopCfgOffset) gopCfgOffset[gopSize] = gopCfg->size;

  if (config) {
    int32_t id = 0;
    while (config[id]) {
      ParseGopConfigString(config[id], gopCfg, id, gopSize);
      id++;
    }
    ret = 0;
  }
  return ret;
}

/**
 * @brief VCEncInitGopConfigs(), parse gop config.
 * @param int32_t gopSize: gop size.
 * @param commandLine_local_s *cml: input command line compatible with api.
 * @param VCEncGopConfig *gopCfg: gop config.
 * @param uint8_t *gopCfgOffset: gop config offset.
 * @param bool bPass2: if pass2.
 * @return int32_t: 0 successful, other unsuccessful.
 */
static int32_t VCEncInitGopConfigs(int32_t gopSize, commandLine_local_s *cml,
                                   VCEncGopConfig *gopCfg,
                                   uint8_t *gopCfgOffset, bool bPass2) {
  int32_t i, pre_load_num;
  int8_t *fname = cml->gopCfg;
  int8_t **default_configs[8] = {
      cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_1
                       : (cml->codecFormat ? RpsDefault_H264_GOPSize_1
                                           : RpsDefault_GOPSize_1),
      cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_2 : RpsDefault_GOPSize_2,
      cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_3 : RpsDefault_GOPSize_3,
      cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_4 : RpsDefault_GOPSize_4,
      RpsDefault_GOPSize_5, RpsDefault_GOPSize_6, RpsDefault_GOPSize_7,
      RpsDefault_GOPSize_8};

  if (gopSize < 0 || gopSize > MAX_GOP_SIZE) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "GOP Config: Error, Invalid GOP Size\n");
    return -1;
  }

  if (bPass2) {
    default_configs[1] = RpsPass2_GOPSize_2;
    default_configs[3] = RpsPass2_GOPSize_4;
    default_configs[7] = RpsPass2_GOPSize_8;
  }

  // Handle Interlace
  if (cml->interlacedFrame && gopSize == 1) {
    default_configs[0] = RpsDefault_Interlace_GOPSize_1;
  }

// GOP size in rps array for gopSize=N
// N<=4:      GOP1, ..., GOPN
// 4<N<=8:   GOP1, GOP2, GOP3, GOP4, GOPN
// N > 8:       GOP1, GOPN
// Adaptive:  GOP1, GOP2, GOP3, GOP4, GOP6, GOP8
#if 0
  if (gopSize > 8)
    pre_load_num = 1;
  else if (gopSize>=4 || gopSize==0)
    pre_load_num = 4;
  else
#endif
  pre_load_num = gopSize;

  if (pre_load_num == 0) pre_load_num = 4;

  gopCfg->special_size = 0;
  gopCfg->ltrcnt = 0;

  for (i = 1; i <= pre_load_num; i++) {
    if (HEVCReadGopConfig(gopSize == i ? fname : NULL, default_configs[i - 1],
                          gopCfg, i, gopCfgOffset))
      return -1;
  }
  // HANTRO_LOG(HANTRO_LEVEL_INFO, "1 gop size=%d\n",gopCfg->size);
  if (gopSize == 0) {
    // gop6
    if (HEVCReadGopConfig(NULL, default_configs[5], gopCfg, 6, gopCfgOffset))
      return -1;
    // HANTRO_LOG(HANTRO_LEVEL_INFO, "2 gop size=%d\n",gopCfg->size);
    // gop8
    if (HEVCReadGopConfig(NULL, default_configs[7], gopCfg, 8, gopCfgOffset))
      return -1;
    // HANTRO_LOG(HANTRO_LEVEL_INFO, "3 gop size=%d\n",gopCfg->size);
  } else if (gopSize > 4) {
    // gopSize
    if (HEVCReadGopConfig(fname, default_configs[gopSize - 1], gopCfg, gopSize,
                          gopCfgOffset))
      return -1;
    // HANTRO_LOG(HANTRO_LEVEL_INFO, "4 gop size=%d\n",gopCfg->size);
  }

  if ((-1 != cml->ltrInterval) && (gopCfg->special_size == 0)) {
    if (cml->gopSize != 1) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "GOP Config: Error, when using --LTR configure option, the "
                 "gopsize alse should be set to 1!\n");
      return -1;
    }
    gopCfg->pGopPicSpecialCfg[0].poc = 0;
    gopCfg->pGopPicSpecialCfg[0].QpOffset = cml->longTermQpDelta;
    gopCfg->pGopPicSpecialCfg[0].QpFactor = QPFACTOR_RESERVED;
    gopCfg->pGopPicSpecialCfg[0].temporalId = TEMPORALID_RESERVED;
    gopCfg->pGopPicSpecialCfg[0].codingType = FRAME_TYPE_RESERVED;
    gopCfg->pGopPicSpecialCfg[0].numRefPics = NUMREFPICS_RESERVED;
    gopCfg->pGopPicSpecialCfg[0].i32Ltr = 1;
    gopCfg->pGopPicSpecialCfg[0].i32Offset = 0;
    gopCfg->pGopPicSpecialCfg[0].i32Interval = cml->ltrInterval;
    gopCfg->pGopPicSpecialCfg[0].i32short_change = 0;
    gopCfg->u32LTR_idx[0] = LONG_TERM_REF_ID2DELTAPOC(0);

    gopCfg->pGopPicSpecialCfg[1].poc = 0;
    gopCfg->pGopPicSpecialCfg[1].QpOffset = QPOFFSET_RESERVED;
    gopCfg->pGopPicSpecialCfg[1].QpFactor = QPFACTOR_RESERVED;
    gopCfg->pGopPicSpecialCfg[1].temporalId = TEMPORALID_RESERVED;
    gopCfg->pGopPicSpecialCfg[1].codingType = FRAME_TYPE_RESERVED;
    gopCfg->pGopPicSpecialCfg[1].numRefPics = 2;
    gopCfg->pGopPicSpecialCfg[1].refPics[0].ref_pic = -1;
    gopCfg->pGopPicSpecialCfg[1].refPics[0].used_by_cur = 1;
    gopCfg->pGopPicSpecialCfg[1].refPics[1].ref_pic =
        LONG_TERM_REF_ID2DELTAPOC(0);
    gopCfg->pGopPicSpecialCfg[1].refPics[1].used_by_cur = 1;
    gopCfg->pGopPicSpecialCfg[1].i32Ltr = 0;
    gopCfg->pGopPicSpecialCfg[1].i32Offset = cml->longTermGapOffset;
    gopCfg->pGopPicSpecialCfg[1].i32Interval = cml->longTermGap;
    gopCfg->pGopPicSpecialCfg[1].i32short_change = 0;

    gopCfg->special_size = 0;
    gopCfg->ltrcnt = 0;
  }

  if (0)
    for (i = 0; i < (gopSize == 0 ? gopCfg->size : gopCfgOffset[gopSize]);
         i++) {
      // when use long-term, change P to B in default configs (used for last
      // gop)
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_PREDICTED_FRAME)
        cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
    }

  // Compatible with old bFrameQpDelta setting
  if (cml->bFrameQpDelta >= 0 && fname == NULL) {
    for (i = 0; i < gopCfg->size; i++) {
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_BIDIR_PREDICTED_FRAME)
        cfg->QpOffset = cml->bFrameQpDelta;
    }
  }
  // HANTRO_LOG(HANTRO_LEVEL_INFO, "5 gop size=%d\n",gopCfg->size);

  // lowDelay auto detection
  VCEncGopPicConfig *cfgStart = &(gopCfg->pGopPicCfg[gopCfgOffset[gopSize]]);
  if (gopSize == 1) {
    cml->gopLowdelay = 1;
  } else if ((gopSize > 1) && (cml->gopLowdelay == 0)) {
    cml->gopLowdelay = 1;
    for (i = 1; i < gopSize; i++) {
      if (cfgStart[i].poc < cfgStart[i - 1].poc) {
        cml->gopLowdelay = 0;
        break;
      }
    }
  }
  // HANTRO_LOG(HANTRO_LEVEL_INFO, "6 gop size=%d\n",gopCfg->size);

  {
    int32_t i32LtrPoc[VCENC_MAX_LT_REF_FRAMES];

    for (i = 0; i < VCENC_MAX_LT_REF_FRAMES; i++) i32LtrPoc[i] = -1;
    for (i = 0; i < gopCfg->special_size; i++) {
      if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > VCENC_MAX_LT_REF_FRAMES) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR,
                   "GOP Config: Error, Invalid long-term index\n");
        return -1;
      }
      if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > 0)
        i32LtrPoc[i] = gopCfg->pGopPicSpecialCfg[i].i32Ltr - 1;
    }

    //      gopCfg->ltrcnt = 1; //force ltrcnt = 1
    //      i32LtrPoc[0] = 0;   //idr is ltr

    for (i = 0; i < gopCfg->ltrcnt; i++) {
      if ((0 != i32LtrPoc[0]) || (-1 == i32LtrPoc[i]) ||
          ((i > 0) && i32LtrPoc[i] != (i32LtrPoc[i - 1] + 1))) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR,
                   "GOP Config: Error, Invalid long-term index\n");
        return -1;
      }
    }
  }
  // HANTRO_LOG(HANTRO_LEVEL_INFO, "7 gop size=%d\n",gopCfg->size);

  // For lowDelay, Handle the first few frames that miss reference frame
  if (cml->gopLowdelay) {
    int32_t nGop;
    int32_t idx = 0;
    int32_t maxErrFrame = 0;
    VCEncGopPicConfig *cfg;

    // Find the max frame number that will miss its reference frame defined in
    // rps
    while ((idx - maxErrFrame) < gopSize) {
      nGop = (idx / gopSize) * gopSize;
      cfg = &(cfgStart[idx % gopSize]);

      for (i = 0; i < cfg->numRefPics; i++) {
        // POC of this reference frame
        int32_t refPoc = cfg->refPics[i].ref_pic + cfg->poc + nGop;
        if (refPoc < 0) {
          maxErrFrame = idx + 1;
        }
      }
      idx++;
    }

    // Try to config a new rps for each "error" frame by modifying its original
    // rps
    for (idx = 0; idx < maxErrFrame; idx++) {
      int32_t j, iRef, nRefsUsedByCur, nPoc;
      VCEncGopPicConfig *cfgCopy;

      if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM) break;

      // Add to array end
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size]);
      cfgCopy = &(cfgStart[idx % gopSize]);
      memcpy(cfg, cfgCopy, sizeof(VCEncGopPicConfig));
      gopCfg->size++;

      // Copy reference pictures
      nRefsUsedByCur = iRef = 0;
      nPoc = cfgCopy->poc + ((idx / gopSize) * gopSize);
      for (i = 0; i < cfgCopy->numRefPics; i++) {
        int32_t newRef = 1;
        int32_t used_by_cur = cfgCopy->refPics[i].used_by_cur;
        int32_t ref_pic = cfgCopy->refPics[i].ref_pic;
        // Clip the reference POC
        if ((cfgCopy->refPics[i].ref_pic + nPoc) < 0) ref_pic = 0 - (nPoc);

        // Check if already have this reference
        for (j = 0; j < iRef; j++) {
          if (cfg->refPics[j].ref_pic == ref_pic) {
            newRef = 0;
            if (used_by_cur) cfg->refPics[j].used_by_cur = used_by_cur;
            break;
          }
        }

        // Copy this reference
        if (newRef) {
          cfg->refPics[iRef].ref_pic = ref_pic;
          cfg->refPics[iRef].used_by_cur = used_by_cur;
          iRef++;
        }
      }
      cfg->numRefPics = iRef;
      // If only one reference frame, set P type.
      for (i = 0; i < cfg->numRefPics; i++) {
        if (cfg->refPics[i].used_by_cur) nRefsUsedByCur++;
      }
      if (nRefsUsedByCur == 1) cfg->codingType = VCENC_PREDICTED_FRAME;
    }
  }
// HANTRO_LOG(HANTRO_LEVEL_INFO, "8 gop size=%d\n",gopCfg->size);
#if 0
      //print for debug
      int32_t idx;
      HANTRO_LOG(HANTRO_LEVEL_DEBUG, "====== REF PICTURE SETS from %s ======\n",fname ? fname : "DEFAULT");
      for (idx = 0; idx < gopCfg->size; idx ++)
      {
        int32_t i;
        VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[idx]);
        char type = cfg->codingType==VCENC_PREDICTED_FRAME ? 'P' : cfg->codingType == VCENC_INTRA_FRAME ? 'I' : 'B';
        HANTRO_LOG(HANTRO_LEVEL_DEBUG, " FRAME%2d:  %c %d %d %f %d", idx, type, cfg->poc, cfg->QpOffset, cfg->QpFactor, cfg->numRefPics);
        for (i = 0; i < cfg->numRefPics; i ++)
          HANTRO_LOG(HANTRO_LEVEL_DEBUG, " %d", cfg->refPics[i].ref_pic);
        for (i = 0; i < cfg->numRefPics; i ++)
          HANTRO_LOG(HANTRO_LEVEL_DEBUG, " %d", cfg->refPics[i].used_by_cur);
        HANTRO_LOG(HANTRO_LEVEL_DEBUG, "\n");
      }
      HANTRO_LOG(HANTRO_LEVEL_DEBUG, "===========================================\n");
#endif
  return 0;
}

/**
 * @brief init_gop_config(), init gop config.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param VCEncIn* encIn: encoder input parameters.
 * @param int32_t intraPicRate: intra picture rate.
 * @return int: error number.
 */
static int32_t init_gop_config(v4l2_enc_inst *h, int32_t intraPicRate) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;
  memset(params->gopCfgOffset, 0, sizeof(params->gopCfgOffset));
  memset(params->gopPicCfg, 0, sizeof(params->gopPicCfg));
  encIn->gopConfig.idr_interval = intraPicRate;
  if (VCEncInitGopConfigs(encIn->gopSize, &params->cml, &encIn->gopConfig,
                          params->gopCfgOffset, HANTRO_FALSE) != 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "error when config gop configs for 2 pass.\n");
    return -18;  // this is not defined in api, indicate gop init error.
  }
  memcpy(encIn->gopConfig.gopCfgOffset, params->gopCfgOffset,
         sizeof(params->gopCfgOffset));
  return 0;
}

static VCEncLevel getLevelHevc(i32 levelIdx)
{
  switch (levelIdx)
  {
    /*Hevc*/
    case 0:
      return VCENC_HEVC_LEVEL_1;
    case 1:
      return VCENC_HEVC_LEVEL_2;
    case 2:
      return VCENC_HEVC_LEVEL_2_1;
    case 3:
      return VCENC_HEVC_LEVEL_3;
    case 4:
      return VCENC_HEVC_LEVEL_3_1;
    case 5:
      return VCENC_HEVC_LEVEL_4;
    case 6:
      return VCENC_HEVC_LEVEL_4_1;
    case 7:
      return VCENC_HEVC_LEVEL_5;
    case 8:
      return VCENC_HEVC_LEVEL_5_1;
    case 9:
      return VCENC_HEVC_LEVEL_5_2;
    case 10:
      return VCENC_HEVC_LEVEL_6;
    case 11:
      return VCENC_HEVC_LEVEL_6_1;
    default:
      return VCENC_HEVC_LEVEL_6_2;
  }
}

static VCEncLevel getLevelH264(i32 levelIdx)
{
  switch (levelIdx)
  {
    case 0:
      return VCENC_H264_LEVEL_1;
    case 1:
      return VCENC_H264_LEVEL_1_b;
    case 2:
      return VCENC_H264_LEVEL_1_1;
    case 3:
      return VCENC_H264_LEVEL_1_2;
    case 4:
      return VCENC_H264_LEVEL_1_3;
    case 5:
      return VCENC_H264_LEVEL_2;
    case 6:
      return VCENC_H264_LEVEL_2_1;
    case 7:
      return VCENC_H264_LEVEL_2_2;
    case 8:
      return VCENC_H264_LEVEL_3;
    case 9:
      return VCENC_H264_LEVEL_3_1;
    case 10:
      return VCENC_H264_LEVEL_3_2;
    case 11:
      return VCENC_H264_LEVEL_4;
    case 12:
      return VCENC_H264_LEVEL_4_1;
    case 13:
      return VCENC_H264_LEVEL_4_2;
    case 14:
      return VCENC_H264_LEVEL_5;
    case 15:
      return VCENC_H264_LEVEL_5_1;
    case 16:
      return VCENC_H264_LEVEL_5_2;
    case 17:
      return VCENC_H264_LEVEL_6;
    case 18:
      return VCENC_H264_LEVEL_6_1;
    default:
      return VCENC_H264_LEVEL_6_2;
  }
}

static i32 getlevelIdxHevc(VCEncLevel level)
{
  switch (level)
  {
    case VCENC_HEVC_LEVEL_1:
      return 0;
    case VCENC_HEVC_LEVEL_2:
      return 1;
    case VCENC_HEVC_LEVEL_2_1:
      return 2;
    case VCENC_HEVC_LEVEL_3:
      return 3;
    case VCENC_HEVC_LEVEL_3_1:
      return 4;
    case VCENC_HEVC_LEVEL_4:
      return 5;
    case VCENC_HEVC_LEVEL_4_1:
      return 6;
    case VCENC_HEVC_LEVEL_5:
      return 7;
    case VCENC_HEVC_LEVEL_5_1:
      return 8;
    case VCENC_HEVC_LEVEL_5_2:
      return 9;
    case VCENC_HEVC_LEVEL_6:
      return 10;
    case VCENC_HEVC_LEVEL_6_1:
      return 11;
    case VCENC_HEVC_LEVEL_6_2:
      return 12;
    default:
      ASSERT(0);
  }
  return 0;
}
static i32 getlevelIdxH264(VCEncLevel level)
{
  switch (level)
  {
    case VCENC_H264_LEVEL_1:
      return 0;
    case VCENC_H264_LEVEL_1_b:
      return 1;
    case VCENC_H264_LEVEL_1_1:
      return 2;
    case VCENC_H264_LEVEL_1_2:
      return 3;
    case VCENC_H264_LEVEL_1_3:
      return 4;
    case VCENC_H264_LEVEL_2:
      return 5;
    case VCENC_H264_LEVEL_2_1:
      return 6;
    case VCENC_H264_LEVEL_2_2:
      return 7;
    case VCENC_H264_LEVEL_3:
      return 8;
    case VCENC_H264_LEVEL_3_1:
      return 9;
    case VCENC_H264_LEVEL_3_2:
      return 10;
    case VCENC_H264_LEVEL_4:
      return 11;
    case VCENC_H264_LEVEL_4_1:
      return 12;
    case VCENC_H264_LEVEL_4_2:
      return 13;
    case VCENC_H264_LEVEL_5:
      return 14;
    case VCENC_H264_LEVEL_5_1:
      return 15;
    case VCENC_H264_LEVEL_5_2:
      return 16;
    case VCENC_H264_LEVEL_6:
      return 17;
    case VCENC_H264_LEVEL_6_1:
      return 18;
    case VCENC_H264_LEVEL_6_2:
      return 19;
    default:
      ASSERT(0);
  }
  return 0;
}

static VCEncLevel getLevel(v4l2_daemon_codec_fmt codecFormat, i32 levelIdx)
{
  switch(codecFormat)
  {
    case V4L2_DAEMON_CODEC_ENC_HEVC:
      return getLevelHevc( levelIdx );

    case V4L2_DAEMON_CODEC_ENC_H264:
      return getLevelH264( levelIdx );

    case V4L2_DAEMON_CODEC_ENC_AV1:
      return 1;

    case V4L2_DAEMON_CODEC_ENC_VP9:
      return 1;

    default:
      ASSERT(0);
  }
  return -1;
}

/**
 * @brief InitPicConfig(), init encIn.
 * @param VCEncIn *pEncIn: input parameters.
 * @return void.
 */
static void InitPicConfig(VCEncIn *pEncIn) {
  int32_t i, j, k, i32Poc;
  int32_t i32MaxpicOrderCntLsb = 1 << 16;

  ASSERT(pEncIn != NULL);

  pEncIn->gopCurrPicConfig.codingType = FRAME_TYPE_RESERVED;
  pEncIn->gopCurrPicConfig.numRefPics = NUMREFPICS_RESERVED;
  pEncIn->gopCurrPicConfig.poc = -1;
  pEncIn->gopCurrPicConfig.QpFactor = QPFACTOR_RESERVED;
  pEncIn->gopCurrPicConfig.QpOffset = QPOFFSET_RESERVED;
  pEncIn->gopCurrPicConfig.temporalId = TEMPORALID_RESERVED;
  pEncIn->i8SpecialRpsIdx = -1;
  for (k = 0; k < VCENC_MAX_REF_FRAMES; k++) {
    pEncIn->gopCurrPicConfig.refPics[k].ref_pic = INVALITED_POC;
    pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = 0;
  }

  for (k = 0; k < VCENC_MAX_LT_REF_FRAMES; k++)
    pEncIn->long_term_ref_pic[k] = INVALITED_POC;

  pEncIn->bIsPeriodUsingLTR = false;
  pEncIn->bIsPeriodUpdateLTR = false;

  for (i = 0; i < pEncIn->gopConfig.special_size; i++) {
    if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Interval <= 0) continue;

    if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr == 0)
      pEncIn->bIsPeriodUsingLTR = true;
    else {
      pEncIn->bIsPeriodUpdateLTR = true;

      for (k = 0;
           k < (int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[i].numRefPics;
           k++) {
        int32_t i32LTRIdx =
            pEncIn->gopConfig.pGopPicSpecialCfg[i].refPics[k].ref_pic;
        if ((IS_LONG_TERM_REF_DELTAPOC(i32LTRIdx)) &&
            ((pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr - 1) ==
             LONG_TERM_REF_DELTAPOC2ID(i32LTRIdx))) {
          pEncIn->bIsPeriodUsingLTR = true;
        }
      }
    }
  }

  memset(pEncIn->bLTR_need_update, 0,
         sizeof(uint32_t) * VCENC_MAX_LT_REF_FRAMES);
  pEncIn->bIsIDR = true;

  i32Poc = 0;
  /* check current picture encoded as LTR*/
  pEncIn->u8IdxEncodedAsLTR = 0;
  for (j = 0; j < pEncIn->gopConfig.special_size; j++) {
    if (pEncIn->bIsPeriodUsingLTR == false) break;

    if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) ||
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0))
      continue;

    i32Poc = i32Poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

    if (i32Poc < 0) {
      i32Poc += i32MaxpicOrderCntLsb;
      if (i32Poc > (i32MaxpicOrderCntLsb >> 1)) i32Poc = -1;
    }

    if ((i32Poc >= 0) &&
        (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0)) {
      /* more than one LTR at the same frame position */
      if (0 != pEncIn->u8IdxEncodedAsLTR) {
        // reuse the same POC LTR
        pEncIn->bLTR_need_update[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr -
                                 1] = true;
        continue;
      }

      pEncIn->gopCurrPicConfig.codingType =
          ((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
           FRAME_TYPE_RESERVED)
              ? pEncIn->gopCurrPicConfig.codingType
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
      pEncIn->gopCurrPicConfig.numRefPics =
          ((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
           NUMREFPICS_RESERVED)
              ? pEncIn->gopCurrPicConfig.numRefPics
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
      pEncIn->gopCurrPicConfig.QpFactor =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED)
              ? pEncIn->gopCurrPicConfig.QpFactor
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
      pEncIn->gopCurrPicConfig.QpOffset =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED)
              ? pEncIn->gopCurrPicConfig.QpOffset
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
      pEncIn->gopCurrPicConfig.temporalId =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId ==
           TEMPORALID_RESERVED)
              ? pEncIn->gopCurrPicConfig.temporalId
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

      if (((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics !=
           NUMREFPICS_RESERVED)) {
        for (k = 0; k < (int32_t)pEncIn->gopCurrPicConfig.numRefPics; k++) {
          pEncIn->gopCurrPicConfig.refPics[k].ref_pic =
              pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
          pEncIn->gopCurrPicConfig.refPics[k].used_by_cur =
              pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
        }
      }

      pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
      pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = true;
    }
  }
}

/**
 * @brief getAlignedPicSizebyFormat(), get picture size for input format.
 * @param VCEncPictureType type: input picture format.
 * @param uint32_t width: width of input picture.
 * @param uint32_t height: height of input picture.
 * @param uint32_t alignment: alignment of input picture.
 * @param uint32_t *luma_Size: output luma size.
 * @param uint32_t *chroma_Size: output chroma size.
 * @param uint32_t *picture_Size: output picture(luma+chroma) size.
 * @return void.
 */
static void getAlignedPicSizebyFormat(VCEncPictureType type, uint32_t width,
                                      uint32_t height, uint32_t alignment,
                                      uint32_t *luma_Size,
                                      uint32_t *chroma_Size,
                                      uint32_t *picture_Size) {
  uint32_t luma_stride = 0, chroma_stride = 0;
  uint32_t lumaSize = 0, chromaSize = 0, pictureSize = 0;

  VCEncGetAlignedStride(width, type, &luma_stride, &chroma_stride, alignment);
  switch (type) {
    case VCENC_YUV420_PLANAR:
      lumaSize = luma_stride * height;
      chromaSize = chroma_stride * height / 2 * 2;
      break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
      lumaSize = luma_stride * height;
      chromaSize = chroma_stride * height / 2;
      break;
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
      lumaSize = luma_stride * height;
      chromaSize = 0;
      break;
    case VCENC_YUV420_PLANAR_10BIT_I010:
      lumaSize = luma_stride * height;
      chromaSize = chroma_stride * height / 2 * 2;
      break;
    case VCENC_YUV420_PLANAR_10BIT_P010:
      lumaSize = luma_stride * height;
      chromaSize = chroma_stride * height / 2;
      break;
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
      lumaSize = luma_stride * 10 / 8 * height;
      chromaSize = chroma_stride * 10 / 8 * height / 2 * 2;
      break;
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
      lumaSize = luma_stride * 2 * 2 * height / 2;
      chromaSize = 0;
      break;
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC:
      lumaSize = luma_stride * ((height + 32 - 1) & (~(32 - 1)));
      chromaSize = lumaSize / 2;
      break;
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_H264:
      lumaSize = luma_stride * height * 2 * 12 / 8;
      chromaSize = 0;
      break;
    case VCENC_YUV420_SEMIPLANAR_8BIT_FB:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB:
      lumaSize = luma_stride * ((height + 3) / 4);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case VCENC_YUV420_PLANAR_10BIT_P010_FB:
      lumaSize = luma_stride * ((height + 3) / 4);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case VCENC_YUV420_SEMIPLANAR_101010:
      lumaSize = luma_stride * height;
      chromaSize = chroma_stride * height / 2;
      break;
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
      lumaSize = luma_stride * ((height + 3) / 4);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case VCENC_YUV420_10BIT_TILE_32_4:
      lumaSize = luma_stride * ((height + 3) / 4);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
      lumaSize = luma_stride * ((height + 3) / 4);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
      lumaSize = luma_stride * ((height + 1) / 2);
      chromaSize = chroma_stride * (((height / 2) + 1) / 2);
      break;
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
      lumaSize = luma_stride * ((height + 1) / 2);
      chromaSize = chroma_stride * (((height / 2) + 1) / 2);
      break;
    case VCENC_YUV420_8BIT_TILE_8_8:
      lumaSize = luma_stride * ((height + 7) / 8);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case VCENC_YUV420_10BIT_TILE_8_8:
      lumaSize = luma_stride * ((height + 7) / 8);
      chromaSize = chroma_stride * (((height / 2) + 3) / 4);
      break;
    default:
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "not support this format\n");
      chromaSize = lumaSize = 0;
      break;
  }

  pictureSize = lumaSize + chromaSize;
  if (luma_Size != NULL) *luma_Size = lumaSize;
  if (chroma_Size != NULL) *chroma_Size = chromaSize;
  if (picture_Size != NULL) *picture_Size = pictureSize;
}

/**
 * @brief get_config_from_cmd_h2(), write message from V4l2 parameter to
 * VCEncConfig pointer.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return void.
 */
static int get_config_from_cmd_h2(v4l2_enc_inst *h,
                                  v4l2_daemon_enc_params *enc_params) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;
  VCEncConfig *cfg = &params->video_config;

  if (enc_params->general.rotation && enc_params->general.rotation != 3) {
    cfg->width = enc_params->general.height;
    cfg->height = enc_params->general.width;
  } else {
    cfg->width = enc_params->general.width;
    cfg->height = enc_params->general.height;
  }

  if (cfg->width < VCENC_MIN_ENC_WIDTH || (cfg->width & 0x1) != 0 ||
      cfg->height < VCENC_MIN_ENC_HEIGHT || (cfg->height & 0x1) != 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
               cfg->width, cfg->height);
    return -1;
  }

  if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) {
    if (cfg->width > h->configure_t.max_width_h264 ||
        cfg->height > VCENC_MAX_ENC_HEIGHT) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
                 cfg->width, cfg->height);
      return -1;
    }
  } else if (h->codec_fmt == V4L2_DAEMON_CODEC_ENC_HEVC) {
    if (cfg->width > h->configure_t.max_width_hevc ||
        cfg->height > VCENC_MAX_ENC_HEIGHT) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "Invalid parameters width=%d, height=%d\n",
                 cfg->width, cfg->height);
      return -1;
    }
  }

  cfg->frameRateDenom = enc_params->general.outputRateDenom;
  cfg->frameRateNum = enc_params->general.outputRateNumer;

  if ((cfg->frameRateNum == 0 ||
       cfg->frameRateDenom ==
           0) || /* special allowal of 1000/1001, 0.99 fps by customer request
                    */
      (cfg->frameRateDenom > cfg->frameRateNum &&
       !(cfg->frameRateDenom == 1001 && cfg->frameRateNum == 1000))) {
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Framerate is not set properly, set to default (30fps).\n");
    cfg->frameRateDenom = 1;
    cfg->frameRateNum = 30;
  }

  /* intra tools in sps and pps */
  cfg->strongIntraSmoothing =
      enc_params->specific.enc_h26x_cmd.strong_intra_smoothing_enabled_flag;
  cfg->streamType = (enc_params->specific.enc_h26x_cmd.byteStream)
                        ? VCENC_BYTE_STREAM
                        : VCENC_NAL_UNIT_STREAM;

  cfg->level = (IS_H264(enc_params->general.codecFormat) ? VCENC_H264_LEVEL_5_1
                                                         : VCENC_HEVC_LEVEL_6);
  if (enc_params->specific.enc_h26x_cmd.avclevel != DEFAULTLEVEL &&
      IS_H264(enc_params->general.codecFormat))
    cfg->level = (VCEncLevel)enc_params->specific.enc_h26x_cmd.avclevel;
  else if (enc_params->specific.enc_h26x_cmd.hevclevel != DEFAULTLEVEL &&
           IS_HEVC(enc_params->general.codecFormat))
    cfg->level = (VCEncLevel)enc_params->specific.enc_h26x_cmd.hevclevel;

  int32_t levelIdx = 0, minLvl;
  if(IS_HEVC(enc_params->general.codecFormat)) {
      levelIdx = getlevelIdxHevc(cfg->level);
  }
  else if(IS_H264(enc_params->general.codecFormat)) {
      levelIdx = getlevelIdxH264(cfg->level);
  }

  cfg->tier = VCENC_HEVC_MAIN_TIER;
  if (enc_params->specific.enc_h26x_cmd.tier != -1)
    cfg->tier = (VCEncTier)enc_params->specific.enc_h26x_cmd.tier;

  cfg->profile =
      (IS_H264(enc_params->general.codecFormat) ? VCENC_H264_HIGH_PROFILE
                                                : VCENC_HEVC_MAIN_PROFILE);
  if (enc_params->specific.enc_h26x_cmd.profile != -1 &&
      enc_params->specific.enc_h26x_cmd.profile != 0)
    cfg->profile = (VCEncProfile)enc_params->specific.enc_h26x_cmd.profile;
  if (IS_H264(enc_params->general.codecFormat)) {
    if ((int32_t)cfg->profile >= VCENC_HEVC_MAIN_PROFILE &&
        cfg->profile < VCENC_HEVC_MAIN_10_PROFILE)
      cfg->profile = VCENC_H264_HIGH_PROFILE;
  } else {
    if (cfg->profile >= VCENC_H264_BASE_PROFILE &&
        cfg->profile <= VCENC_H264_HIGH_PROFILE)
      cfg->profile = VCENC_HEVC_MAIN_PROFILE;
  }
  minLvl = calculate_level(h->codec_fmt, cfg->width, cfg->height,
                  cfg->frameRateNum, cfg->frameRateDenom,
                  enc_params->general.bitPerSecond, cfg->profile);
  if(levelIdx < minLvl) {
      cfg->level = getLevel(h->codec_fmt, minLvl);
      send_warning_orphan_msg(h->instance_id, WARN_LEVEL);
  }

  cfg->codecFormat = enc_params->general.codecFormat;
  cfg->bitDepthLuma = 8;
  if (enc_params->specific.enc_h26x_cmd.bitDepthLuma != -1)
    cfg->bitDepthLuma = enc_params->specific.enc_h26x_cmd.bitDepthLuma;
  cfg->bitDepthChroma = 8;
  if (enc_params->specific.enc_h26x_cmd.bitDepthChroma != -1)
    cfg->bitDepthChroma = enc_params->specific.enc_h26x_cmd.bitDepthChroma;
  if ((enc_params->specific.enc_h26x_cmd.interlacedFrame &&
       enc_params->specific.enc_h26x_cmd.gopSize != 1) ||
      IS_H264(enc_params->general.codecFormat)) {
    // HANTRO_LOG(HANTRO_LEVEL_INFO, "OpenEncoder: treat interlace to
    // progressive for gopSize!=1 case.\n");
    enc_params->specific.enc_h26x_cmd.interlacedFrame = 0;
  }
  cfg->maxTLayers = 1;  // default maxTLayer
  /* Find the max number of reference frame */
  if (enc_params->specific.enc_h26x_cmd.intraPicRate == 1) {
    cfg->refFrameAmount = 1; //workaround for gst parser.
  } else {
    uint32_t maxRefPics = 0;
    uint32_t maxTemporalId = 0;
    int32_t idx;
    for (idx = 0; idx < encIn->gopConfig.size; idx++) {
      VCEncGopPicConfig *cfg = &(encIn->gopConfig.pGopPicCfg[idx]);
      if (cfg->codingType != VCENC_INTRA_FRAME) {
        if (maxRefPics < cfg->numRefPics) maxRefPics = cfg->numRefPics;

        if (maxTemporalId < cfg->temporalId) maxTemporalId = cfg->temporalId;
      }
    }
    cfg->refFrameAmount = maxRefPics +
                          enc_params->specific.enc_h26x_cmd.interlacedFrame +
                          encIn->gopConfig.ltrcnt;
    cfg->maxTLayers = maxTemporalId + 1;
  }

  cfg->compressor = enc_params->specific.enc_h26x_cmd.compressor;
  cfg->interlacedFrame = enc_params->specific.enc_h26x_cmd.interlacedFrame;
  cfg->enableOutputCuInfo =
      (enc_params->specific.enc_h26x_cmd.enableOutputCuInfo > 0) ? 1 : 0;
  // cfg->rdoLevel = CLIP3(1, 3, cml->rdoLevel) - 1;
  cfg->rdoLevel = 1;
  cfg->verbose = 0;  // enc_params->generel.verbose;
  // cfg->exp_of_input_alignment = enc_params->general.exp_of_input_alignment;
  // cfg->exp_of_ref_alignment = cml->exp_of_ref_alignment;
  // cfg->exp_of_ref_ch_alignment = cml->exp_of_ref_ch_alignment;
  cfg->exteralReconAlloc = 0;
  cfg->P010RefEnable = enc_params->specific.enc_h26x_cmd.P010RefEnable;
  cfg->enableSsim = enc_params->specific.enc_h26x_cmd.ssim;
  cfg->ctbRcMode = (enc_params->specific.enc_h26x_cmd.ctbRc != -1)
                       ? enc_params->specific.enc_h26x_cmd.ctbRc
                       : 0;
  cfg->parallelCoreNum = enc_params->specific.enc_h26x_cmd.parallelCoreNum;
  cfg->pass = (enc_params->specific.enc_h26x_cmd.lookaheadDepth ? 2 : 0);
  cfg->bPass1AdaptiveGop = (enc_params->specific.enc_h26x_cmd.gopSize == 0);
  cfg->picOrderCntType = enc_params->specific.enc_h26x_cmd.picOrderCntType;
  cfg->dumpRegister = 0;
  cfg->rasterscan = 0;
  cfg->log2MaxPicOrderCntLsb =
      enc_params->specific.enc_h26x_cmd.log2MaxPicOrderCntLsb;
  cfg->log2MaxFrameNum = enc_params->specific.enc_h26x_cmd.log2MaxFrameNum;
  cfg->lookaheadDepth = enc_params->specific.enc_h26x_cmd.lookaheadDepth;
  cfg->extDSRatio = (enc_params->specific.enc_h26x_cmd.lookaheadDepth &&
                             enc_params->specific.enc_h26x_cmd.halfDsInput
                         ? 1
                         : 0);
  cfg->cuInfoVersion = enc_params->specific.enc_h26x_cmd.cuInfoVersion;
  if (enc_params->specific.enc_h26x_cmd.parallelCoreNum > 1 &&
      cfg->width * cfg->height < 256 * 256) {
    HANTRO_LOG(HANTRO_LEVEL_INFO,
               "Disable multicore for small resolution (< 255*255).\n");
    cfg->parallelCoreNum = enc_params->specific.enc_h26x_cmd.parallelCoreNum =
        1;
  }
#if defined(NXP)
#else
  cfg->writeReconToDDR = 1;
#endif
  return 0;
}

/**
 * @brief get_rate_control_from_cmd_h2(), write message from V4l2 parameter to
 * VCEncRateCtrl pointer.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param void* rcCfg: rate control parameters.
 * @return void.
 */
static void get_rate_control_from_cmd_h2(v4l2_enc_inst *h,
                                         v4l2_daemon_enc_params *enc_params) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncRateCtrl *rcCfg = &params->video_rate_ctrl;
  VCEncConfig *cfg = &params->video_config;

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
  if ((enc_params->specific.enc_h26x_cmd.qpMin_h26x >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMin_h26x <= 51))
    rcCfg->qpMinPB = enc_params->specific.enc_h26x_cmd.qpMin_h26x;
  else
    rcCfg->qpMinPB = 0;
  if ((enc_params->specific.enc_h26x_cmd.qpMax_h26x >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMax_h26x <= 51))
    rcCfg->qpMaxPB = enc_params->specific.enc_h26x_cmd.qpMax_h26x;
  else
    rcCfg->qpMaxPB = 51;
  if ((enc_params->specific.enc_h26x_cmd.qpMinI >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMinI <= 51))
    rcCfg->qpMinI = enc_params->specific.enc_h26x_cmd.qpMinI;
  else
    rcCfg->qpMinI = 0;
  if ((enc_params->specific.enc_h26x_cmd.qpMaxI >= 0) &&
      (enc_params->specific.enc_h26x_cmd.qpMaxI <= 51))
    rcCfg->qpMaxI = enc_params->specific.enc_h26x_cmd.qpMaxI;
  else
    rcCfg->qpMaxI = 51;
  if (enc_params->specific.enc_h26x_cmd.picSkip != -1)
    rcCfg->pictureSkip = enc_params->specific.enc_h26x_cmd.picSkip;
  else
    rcCfg->pictureSkip = 0;
  if (enc_params->specific.enc_h26x_cmd.picRc != -1)
    rcCfg->pictureRc = enc_params->specific.enc_h26x_cmd.picRc;
  else
    rcCfg->pictureRc = 0;
  if (enc_params->specific.enc_h26x_cmd.ctbRc != -1) {
    if (enc_params->specific.enc_h26x_cmd.ctbRc == 4 ||
        enc_params->specific.enc_h26x_cmd.ctbRc == 6) {
      rcCfg->ctbRc = enc_params->specific.enc_h26x_cmd.ctbRc - 3;
      rcCfg->ctbRcQpDeltaReverse = 1;
    } else {
      rcCfg->ctbRc = enc_params->specific.enc_h26x_cmd.ctbRc;
      rcCfg->ctbRcQpDeltaReverse = 0;
    }
  } else {
    rcCfg->ctbRc = enc_params->specific.enc_h26x_cmd.ctbRc;
    rcCfg->ctbRcQpDeltaReverse = 0;
  }
  if (enc_params->specific.enc_h26x_cmd.blockRCSize != -1)
    rcCfg->blockRCSize = enc_params->specific.enc_h26x_cmd.blockRCSize;
  else
    rcCfg->blockRCSize = 0;
  rcCfg->rcQpDeltaRange = 10;
  if (enc_params->specific.enc_h26x_cmd.rcQpDeltaRange != -1)
    rcCfg->rcQpDeltaRange = enc_params->specific.enc_h26x_cmd.rcQpDeltaRange;
  rcCfg->rcBaseMBComplexity = 15;
  if (enc_params->specific.enc_h26x_cmd.rcBaseMBComplexity != -1)
    rcCfg->rcBaseMBComplexity =
        enc_params->specific.enc_h26x_cmd.rcBaseMBComplexity;
  if (enc_params->specific.enc_h26x_cmd.picQpDeltaMax != -1)
    rcCfg->picQpDeltaMax = enc_params->specific.enc_h26x_cmd.picQpDeltaMax;
  if (enc_params->specific.enc_h26x_cmd.picQpDeltaMin != -1)
    rcCfg->picQpDeltaMin = enc_params->specific.enc_h26x_cmd.picQpDeltaMin;
  if (enc_params->general.bitPerSecond != -1)
    rcCfg->bitPerSecond = enc_params->general.bitPerSecond;
  if (enc_params->specific.enc_h26x_cmd.bitVarRangeI != -1)
    rcCfg->bitVarRangeI = enc_params->specific.enc_h26x_cmd.bitVarRangeI;
  if (enc_params->specific.enc_h26x_cmd.bitVarRangeP != -1)
    rcCfg->bitVarRangeP = enc_params->specific.enc_h26x_cmd.bitVarRangeP;
  if (enc_params->specific.enc_h26x_cmd.bitVarRangeB != -1)
    rcCfg->bitVarRangeB = enc_params->specific.enc_h26x_cmd.bitVarRangeB;
  if (enc_params->specific.enc_h26x_cmd.tolMovingBitRate != -1)
    rcCfg->tolMovingBitRate =
        enc_params->specific.enc_h26x_cmd.tolMovingBitRate;
  if (enc_params->specific.enc_h26x_cmd.tolCtbRcInter != -1)
    rcCfg->tolCtbRcInter = enc_params->specific.enc_h26x_cmd.tolCtbRcInter;
  if (enc_params->specific.enc_h26x_cmd.tolCtbRcIntra != -1)
    rcCfg->tolCtbRcIntra = enc_params->specific.enc_h26x_cmd.tolCtbRcIntra;
  if (enc_params->specific.enc_h26x_cmd.ctbRcRowQpStep != -1)
    rcCfg->ctbRcRowQpStep = enc_params->specific.enc_h26x_cmd.ctbRcRowQpStep;
  rcCfg->longTermQpDelta = enc_params->specific.enc_h26x_cmd.longTermQpDelta;
  if (enc_params->specific.enc_h26x_cmd.monitorFrames != -1)
    rcCfg->monitorFrames = enc_params->specific.enc_h26x_cmd.monitorFrames;
  else {
    rcCfg->monitorFrames = (enc_params->general.outputRateNumer +
                            enc_params->general.outputRateDenom - 1) /
                           enc_params->general.outputRateDenom;
    enc_params->specific.enc_h26x_cmd.monitorFrames =
        (enc_params->general.outputRateNumer +
         enc_params->general.outputRateDenom - 1) /
        enc_params->general.outputRateDenom;
  }
  if (rcCfg->monitorFrames > MOVING_AVERAGE_FRAMES)
    rcCfg->monitorFrames = MOVING_AVERAGE_FRAMES;
  if (rcCfg->monitorFrames < 10) {
    rcCfg->monitorFrames = (enc_params->general.outputRateNumer >
                            enc_params->general.outputRateDenom)
                               ? 10
                               : LEAST_MONITOR_FRAME;
  }
  if (enc_params->specific.enc_h26x_cmd.hrdConformance != -1)
    rcCfg->hrd = enc_params->specific.enc_h26x_cmd.hrdConformance;
  if (enc_params->specific.enc_h26x_cmd.picRc == 0) rcCfg->hrd = 0;
  if (enc_params->specific.enc_h26x_cmd.cpbSize > 0)
    rcCfg->hrdCpbSize = enc_params->specific.enc_h26x_cmd.cpbSize;
  else {//cpbSize default value.
    int32_t levelIdx;
    if(IS_HEVC(enc_params->general.codecFormat))
      levelIdx = getlevelIdxHevc(cfg->level);
    else
      levelIdx = getlevelIdxH264(cfg->level);
    rcCfg->hrdCpbSize = getMaxCpbSize(h->codec_fmt, levelIdx);
  }

  if (enc_params->specific.enc_h26x_cmd.bitrateWindow != -1)
    rcCfg->bitrateWindow = enc_params->specific.enc_h26x_cmd.bitrateWindow;
  else
    rcCfg->bitrateWindow = 150;
  if (enc_params->specific.enc_h26x_cmd.intraQpDelta != -1)
    rcCfg->intraQpDelta = enc_params->specific.enc_h26x_cmd.intraQpDelta;
  if (enc_params->specific.enc_h26x_cmd.vbr != -1)
    rcCfg->vbr = enc_params->specific.enc_h26x_cmd.vbr;
  rcCfg->fixedIntraQp = enc_params->specific.enc_h26x_cmd.fixedIntraQp;
  rcCfg->smoothPsnrInGOP = enc_params->specific.enc_h26x_cmd.smoothPsnrInGOP;
  rcCfg->u32StaticSceneIbitPercent =
      enc_params->specific.enc_h26x_cmd.u32StaticSceneIbitPercent;
  rcCfg->crf = -1;  // not support now.
#if defined(NXP)
#else
  rcCfg->frameRateNum = enc_params->general.outputRateNumer;
  rcCfg->frameRateDenom = enc_params->general.outputRateDenom;
  if (rcCfg->frameRateNum == 0 || rcCfg->frameRateDenom == 0) {
    rcCfg->frameRateNum = 30;
    rcCfg->frameRateDenom = 1;
  }
#endif
  if (rcCfg->hrd == 1 && rcCfg->pictureRc == 1) {  // cbr mode
    h->cbr_last_bitrate = enc_params->general.bitPerSecond;
  }
}

/**
 * @brief get_pre_process_from_cmd_h2(), write message from V4l2 parameter to
 * void pointer.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @return void.
 */
static void get_pre_process_from_cmd_h2(v4l2_enc_inst *h,
                                        v4l2_daemon_enc_params *enc_params) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncPreProcessingCfg *preProcCfg = &params->video_prep;

  preProcCfg->inputType = enc_params->general.inputFormat;
  preProcCfg->inputType = (VCEncPictureType)enc_params->general.inputFormat;
  preProcCfg->rotation = (VCEncPictureRotation)enc_params->general.rotation;
  preProcCfg->mirror = (VCEncPictureMirror)enc_params->general.mirror;
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
        (VCEncColorConversionType)enc_params->general.colorConversion;
  if (preProcCfg->colorConversion.type == VCENC_RGBTOYUV_USER_DEFINED) {
    preProcCfg->colorConversion.coeffA = 20000;
    preProcCfg->colorConversion.coeffB = 44000;
    preProcCfg->colorConversion.coeffC = 5000;
    preProcCfg->colorConversion.coeffE = 35000;
    preProcCfg->colorConversion.coeffF = 38000;
    preProcCfg->colorConversion.coeffG = 35000;
    preProcCfg->colorConversion.coeffH = 38000;
    preProcCfg->colorConversion.LumaOffset = 0;
  }
  if (enc_params->general.rotation && enc_params->general.rotation != 3) {
    preProcCfg->scaledWidth = enc_params->general.scaledHeight;
    preProcCfg->scaledHeight = enc_params->general.scaledWidth;
  } else {
    preProcCfg->scaledWidth = enc_params->general.scaledWidth;
    preProcCfg->scaledHeight = enc_params->general.scaledHeight;
  }
  if (enc_params->general.scaledWidth * enc_params->general.scaledHeight > 0)
    preProcCfg->scaledOutput = 1;
  preProcCfg->constChromaEn = enc_params->specific.enc_h26x_cmd.constChromaEn;
  if (enc_params->specific.enc_h26x_cmd.constCb != -1)
    preProcCfg->constCb = enc_params->specific.enc_h26x_cmd.constCb;
  if (enc_params->specific.enc_h26x_cmd.constCr != -1)
    preProcCfg->constCr = enc_params->specific.enc_h26x_cmd.constCr;
// ChangeToCustomizedFormat(cml,&preProcCfg);
#if defined(NXP)
#else

#ifndef MAX_OVERLAY_NUM
#define MAX_OVERLAY_NUM 8
#endif
  for (int i = 0; i < MAX_OVERLAY_NUM; i++) {
    preProcCfg->overlayArea[i].xoffset = 0;
    preProcCfg->overlayArea[i].cropXoffset = 0;
    preProcCfg->overlayArea[i].yoffset = 0;
    preProcCfg->overlayArea[i].cropYoffset = 0;
    preProcCfg->overlayArea[i].width = 0;
    preProcCfg->overlayArea[i].cropWidth = 0;
    preProcCfg->overlayArea[i].height = 0;
    preProcCfg->overlayArea[i].cropHeight = 0;
    preProcCfg->overlayArea[i].format = 0;
    preProcCfg->overlayArea[i].alpha = 0;
    preProcCfg->overlayArea[i].enable = 0;
    preProcCfg->overlayArea[i].Ystride = 0;
    preProcCfg->overlayArea[i].UVstride = 0;
    preProcCfg->overlayArea[i].bitmapY = 0;
    preProcCfg->overlayArea[i].bitmapU = 0;
    preProcCfg->overlayArea[i].bitmapV = 0;
  }
#endif
}

int32_t check_area(v4l2_enc_inst *hinst, VCEncPictureArea *area, int32_t width, int32_t height,
                   int32_t max_cu_size, int type) {
  int32_t w = (width + max_cu_size - 1) / max_cu_size;
  int32_t h = (height + max_cu_size - 1) / max_cu_size;

  if ((area->left < (uint32_t)w) && (area->right < (uint32_t)w) &&
      (area->top < (uint32_t)h) && (area->bottom < (uint32_t)h))
    return 0;

  if (type == 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "ROI area check error.\n");
    send_warning_orphan_msg(hinst->instance_id, WARN_ROIREGION);
  } else  {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "IPCM area check error.\n");
    send_warning_orphan_msg(hinst->instance_id, WARN_IPCMREGION);
  }
  return -1;
}

/**
 * @brief get_coding_control_from_cmd_h2(), write message from V4l2 parameter to
 * void pointer.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return void.
 */
static void get_coding_control_from_cmd_h2(v4l2_enc_inst *h,
                                           v4l2_daemon_enc_params *enc_params) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncCodingCtrl *codingCfg = &params->video_coding_ctrl;

  if (enc_params->specific.enc_h26x_cmd.sliceSize != -1)
    codingCfg->sliceSize = enc_params->specific.enc_h26x_cmd.sliceSize;
  if (enc_params->specific.enc_h26x_cmd.enableCabac != -1)
    codingCfg->enableCabac = enc_params->specific.enc_h26x_cmd.enableCabac;
  if (enc_params->specific.enc_h26x_cmd.cabacInitFlag != -1)
    codingCfg->cabacInitFlag = enc_params->specific.enc_h26x_cmd.cabacInitFlag;
  codingCfg->videoFullRange = 0;
  if (enc_params->specific.enc_h26x_cmd.videoRange != -1)
    codingCfg->videoFullRange = enc_params->specific.enc_h26x_cmd.videoRange;
  codingCfg->disableDeblockingFilter =
      (enc_params->specific.enc_h26x_cmd.disableDeblocking != 0);
  codingCfg->tc_Offset = enc_params->specific.enc_h26x_cmd.tc_Offset;
  codingCfg->beta_Offset = enc_params->specific.enc_h26x_cmd.beta_Offset;
  codingCfg->enableSao = enc_params->specific.enc_h26x_cmd.enableSao;
  codingCfg->enableDeblockOverride =
      enc_params->specific.enc_h26x_cmd.enableDeblockOverride;
  codingCfg->deblockOverride =
      enc_params->specific.enc_h26x_cmd.deblockOverride;
  if (enc_params->specific.enc_h26x_cmd.sei)
    codingCfg->seiMessages = 1;
  else
    codingCfg->seiMessages = 0;
  codingCfg->gdrDuration = enc_params->specific.enc_h26x_cmd.gdrDuration;
  codingCfg->fieldOrder = enc_params->specific.enc_h26x_cmd.fieldOrder;
  codingCfg->cirStart = enc_params->specific.enc_h26x_cmd.cirStart;
  codingCfg->cirInterval = enc_params->specific.enc_h26x_cmd.cirInterval;
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
    codingCfg->intraArea.enable = 0;  // intraArea will be used by GDR, customer
                                      // can not use intraArea when GDR is
                                      // enabled.
  }

  int32_t max_cu_size = IS_H264(enc_params->general.codecFormat) ? 16 : 64;
  int32_t encode_width = enc_params->general.width;
  int32_t encode_height = enc_params->general.height;

  codingCfg->pcm_loop_filter_disabled_flag =
      enc_params->specific.enc_h26x_cmd.pcm_loop_filter_disabled_flag;
  codingCfg->ipcm1Area.top = enc_params->specific.enc_h26x_cmd.ipcmAreaTop[0];
  codingCfg->ipcm1Area.left = enc_params->specific.enc_h26x_cmd.ipcmAreaLeft[0];
  codingCfg->ipcm1Area.bottom =
      enc_params->specific.enc_h26x_cmd.ipcmAreaBottom[0];
  codingCfg->ipcm1Area.right =
      enc_params->specific.enc_h26x_cmd.ipcmAreaRight[0];
  codingCfg->ipcm1Area.enable =
      enc_params->specific.enc_h26x_cmd.ipcmAreaEnable[0];
  if (check_area(h, &codingCfg->ipcm1Area, encode_width, encode_height,
                 max_cu_size, 1) != 0) {
    codingCfg->ipcm1Area.enable = 0;
  }

  codingCfg->ipcm2Area.top = enc_params->specific.enc_h26x_cmd.ipcmAreaTop[1];
  codingCfg->ipcm2Area.left = enc_params->specific.enc_h26x_cmd.ipcmAreaLeft[1];
  codingCfg->ipcm2Area.bottom =
      enc_params->specific.enc_h26x_cmd.ipcmAreaBottom[1];
  codingCfg->ipcm2Area.right =
      enc_params->specific.enc_h26x_cmd.ipcmAreaRight[1];
  codingCfg->ipcm2Area.enable =
      enc_params->specific.enc_h26x_cmd.ipcmAreaEnable[1];
  if (check_area(h, &codingCfg->ipcm2Area, encode_width, encode_height,
                 max_cu_size, 1) != 0) {
    codingCfg->ipcm2Area.enable = 0;
  }

  codingCfg->ipcmMapEnable = enc_params->specific.enc_h26x_cmd.ipcmMapEnable;
  codingCfg->pcm_enabled_flag =
      (codingCfg->ipcm1Area.enable || codingCfg->ipcm2Area.enable ||
       codingCfg->ipcmMapEnable);

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
    codingCfg->roi1Qp = enc_params->specific.enc_h26x_cmd.roiQp[0];
    if (check_area(h, &codingCfg->roi1Area, encode_width, encode_height,
                   max_cu_size, 0) != 0 ||
        (codingCfg->roi1Qp < 0 && codingCfg->roi1DeltaQp == 0)) {
      codingCfg->roi1Area.enable = 0;
    }
#if 0  // hard code for test
        codingCfg->roi1Area.top = 0;
        codingCfg->roi1Area.left = 0;
        codingCfg->roi1Area.bottom = 3;
        codingCfg->roi1Area.right = 3;
        codingCfg->roi1Area.enable = 1;
        codingCfg->roi1DeltaQp = -5;
        codingCfg->roi1Qp = 10;
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
  codingCfg->roi2Qp = enc_params->specific.enc_h26x_cmd.roiQp[1];
  if (check_area(h, &codingCfg->roi2Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi2Qp < 0 && codingCfg->roi2DeltaQp == 0)) {
    codingCfg->roi2Area.enable = 0;
  }

  codingCfg->roi3Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[2];
  codingCfg->roi3Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[2];
  codingCfg->roi3Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[2];
  codingCfg->roi3Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[2];
  codingCfg->roi3Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[2];
  codingCfg->roi3DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[2];
  codingCfg->roi3Qp = enc_params->specific.enc_h26x_cmd.roiQp[2];
  if (check_area(h, &codingCfg->roi3Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi3Qp < 0 && codingCfg->roi3DeltaQp == 0)) {
    codingCfg->roi3Area.enable = 0;
  }

  codingCfg->roi4Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[3];
  codingCfg->roi4Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[3];
  codingCfg->roi4Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[3];
  codingCfg->roi4Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[3];
  codingCfg->roi4Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[3];
  codingCfg->roi4DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[3];
  codingCfg->roi4Qp = enc_params->specific.enc_h26x_cmd.roiQp[3];
  if (check_area(h, &codingCfg->roi4Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi4Qp < 0 && codingCfg->roi4DeltaQp == 0)) {
    codingCfg->roi4Area.enable = 0;
  }

  codingCfg->roi5Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[4];
  codingCfg->roi5Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[4];
  codingCfg->roi5Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[4];
  codingCfg->roi5Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[4];
  codingCfg->roi5Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[4];
  codingCfg->roi5DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[4];
  codingCfg->roi5Qp = enc_params->specific.enc_h26x_cmd.roiQp[4];
  if (check_area(h, &codingCfg->roi5Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi5Qp < 0 && codingCfg->roi5DeltaQp == 0)) {
    codingCfg->roi5Area.enable = 0;
  }

  codingCfg->roi6Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[5];
  codingCfg->roi6Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[5];
  codingCfg->roi6Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[5];
  codingCfg->roi6Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[5];
  codingCfg->roi6Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[5];
  codingCfg->roi6DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[5];
  codingCfg->roi6Qp = enc_params->specific.enc_h26x_cmd.roiQp[5];
  if (check_area(h, &codingCfg->roi6Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi6Qp < 0 && codingCfg->roi6DeltaQp == 0)) {
    codingCfg->roi6Area.enable = 0;
  }

  codingCfg->roi7Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[6];
  codingCfg->roi7Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[6];
  codingCfg->roi7Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[6];
  codingCfg->roi7Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[6];
  codingCfg->roi7Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[6];
  codingCfg->roi7DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[6];
  codingCfg->roi7Qp = enc_params->specific.enc_h26x_cmd.roiQp[6];
  if (check_area(h, &codingCfg->roi7Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi7Qp < 0 && codingCfg->roi7DeltaQp == 0)) {
    codingCfg->roi7Area.enable = 0;
  }

  codingCfg->roi8Area.top = enc_params->specific.enc_h26x_cmd.roiAreaTop[7];
  codingCfg->roi8Area.left = enc_params->specific.enc_h26x_cmd.roiAreaLeft[7];
  codingCfg->roi8Area.bottom =
      enc_params->specific.enc_h26x_cmd.roiAreaBottom[7];
  codingCfg->roi8Area.right = enc_params->specific.enc_h26x_cmd.roiAreaRight[7];
  codingCfg->roi8Area.enable =
      enc_params->specific.enc_h26x_cmd.roiAreaEnable[7];
  codingCfg->roi8DeltaQp = enc_params->specific.enc_h26x_cmd.roiDeltaQp[7];
  codingCfg->roi8Qp = enc_params->specific.enc_h26x_cmd.roiQp[7];
  if (check_area(h, &codingCfg->roi8Area, encode_width, encode_height,
                 max_cu_size, 0) != 0 ||
      (codingCfg->roi8Qp < 0 && codingCfg->roi8DeltaQp == 0)) {
    codingCfg->roi8Area.enable = 0;
  }

  if (codingCfg->cirInterval)
    HANTRO_LOG(HANTRO_LEVEL_INFO, "  CIR: %d %d\n", codingCfg->cirStart,
               codingCfg->cirInterval);
  if (codingCfg->intraArea.enable)
    HANTRO_LOG(HANTRO_LEVEL_INFO, "  IntraArea: %dx%d-%dx%d\n",
               codingCfg->intraArea.left, codingCfg->intraArea.top,
               codingCfg->intraArea.right, codingCfg->intraArea.bottom);
  if (codingCfg->ipcm1Area.enable)
    HANTRO_LOG(HANTRO_LEVEL_INFO, "  IPCM1Area: %dx%d-%dx%d\n",
               codingCfg->ipcm1Area.left, codingCfg->ipcm1Area.top,
               codingCfg->ipcm1Area.right, codingCfg->ipcm1Area.bottom);
  if (codingCfg->ipcm2Area.enable)
    HANTRO_LOG(HANTRO_LEVEL_INFO, "  IPCM2Area: %dx%d-%dx%d\n",
               codingCfg->ipcm2Area.left, codingCfg->ipcm2Area.top,
               codingCfg->ipcm2Area.right, codingCfg->ipcm2Area.bottom);
  if (codingCfg->roi1Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 1: %s %d %dx%d-%dx%d\n",
        codingCfg->roi1Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi1Qp >= 0 ? codingCfg->roi1Qp : codingCfg->roi1DeltaQp,
        codingCfg->roi1Area.left, codingCfg->roi1Area.top,
        codingCfg->roi1Area.right, codingCfg->roi1Area.bottom);
  if (codingCfg->roi2Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 2: %s %d %dx%d-%dx%d\n",
        codingCfg->roi2Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi2Qp >= 0 ? codingCfg->roi2Qp : codingCfg->roi2DeltaQp,
        codingCfg->roi2Area.left, codingCfg->roi2Area.top,
        codingCfg->roi2Area.right, codingCfg->roi2Area.bottom);
  if (codingCfg->roi3Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 3: %s %d %dx%d-%dx%d\n",
        codingCfg->roi3Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi3Qp >= 0 ? codingCfg->roi3Qp : codingCfg->roi3DeltaQp,
        codingCfg->roi3Area.left, codingCfg->roi3Area.top,
        codingCfg->roi3Area.right, codingCfg->roi3Area.bottom);
  if (codingCfg->roi4Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 4: %s %d %dx%d-%dx%d\n",
        codingCfg->roi4Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi4Qp >= 0 ? codingCfg->roi4Qp : codingCfg->roi4DeltaQp,
        codingCfg->roi4Area.left, codingCfg->roi4Area.top,
        codingCfg->roi4Area.right, codingCfg->roi4Area.bottom);
  if (codingCfg->roi5Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 5: %s %d %dx%d-%dx%d\n",
        codingCfg->roi5Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi5Qp >= 0 ? codingCfg->roi5Qp : codingCfg->roi5DeltaQp,
        codingCfg->roi5Area.left, codingCfg->roi5Area.top,
        codingCfg->roi5Area.right, codingCfg->roi5Area.bottom);
  if (codingCfg->roi6Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 6: %s %d %dx%d-%dx%d\n",
        codingCfg->roi6Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi6Qp >= 0 ? codingCfg->roi6Qp : codingCfg->roi6DeltaQp,
        codingCfg->roi6Area.left, codingCfg->roi6Area.top,
        codingCfg->roi6Area.right, codingCfg->roi6Area.bottom);
  if (codingCfg->roi7Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 7: %s %d %dx%d-%dx%d\n",
        codingCfg->roi7Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi7Qp >= 0 ? codingCfg->roi7Qp : codingCfg->roi7DeltaQp,
        codingCfg->roi7Area.left, codingCfg->roi7Area.top,
        codingCfg->roi7Area.right, codingCfg->roi7Area.bottom);
  if (codingCfg->roi8Area.enable)
    HANTRO_LOG(
        HANTRO_LEVEL_INFO, "  ROI 8: %s %d %dx%d-%dx%d\n",
        codingCfg->roi8Qp >= 0 ? "QP" : "QP Delta",
        codingCfg->roi8Qp >= 0 ? codingCfg->roi8Qp : codingCfg->roi8DeltaQp,
        codingCfg->roi8Area.left, codingCfg->roi8Area.top,
        codingCfg->roi8Area.right, codingCfg->roi8Area.bottom);
  codingCfg->roiMapDeltaQpEnable =
      enc_params->specific.enc_h26x_cmd.roiMapDeltaQpEnable;
  codingCfg->roiMapDeltaQpBlockUnit =
      enc_params->specific.enc_h26x_cmd.roiMapDeltaQpBlockUnit;
  codingCfg->RoimapCuCtrl_ver = enc_params->specific.enc_h26x_cmd.RoiCuCtrlVer;
  codingCfg->RoiQpDelta_ver = enc_params->specific.enc_h26x_cmd.RoiQpDeltaVer;
  /* SKIP map */
  codingCfg->skipMapEnable = enc_params->specific.enc_h26x_cmd.skipMapEnable;
  codingCfg->enableScalingList =
      enc_params->specific.enc_h26x_cmd.enableScalingList;
  codingCfg->chroma_qp_offset =
      enc_params->specific.enc_h26x_cmd.chromaQpOffset;
  /* HDR10 */
  codingCfg->Hdr10Display.hdr10_display_enable =
      enc_params->specific.enc_h26x_cmd.hdr10_display_enable;
  if (enc_params->specific.enc_h26x_cmd.hdr10_display_enable) {
    codingCfg->Hdr10Display.hdr10_dx0 =
        enc_params->specific.enc_h26x_cmd.hdr10_dx0;
    codingCfg->Hdr10Display.hdr10_dy0 =
        enc_params->specific.enc_h26x_cmd.hdr10_dy0;
    codingCfg->Hdr10Display.hdr10_dx1 =
        enc_params->specific.enc_h26x_cmd.hdr10_dx1;
    codingCfg->Hdr10Display.hdr10_dy1 =
        enc_params->specific.enc_h26x_cmd.hdr10_dy1;
    codingCfg->Hdr10Display.hdr10_dx2 =
        enc_params->specific.enc_h26x_cmd.hdr10_dx2;
    codingCfg->Hdr10Display.hdr10_dy2 =
        enc_params->specific.enc_h26x_cmd.hdr10_dy2;
    codingCfg->Hdr10Display.hdr10_wx =
        enc_params->specific.enc_h26x_cmd.hdr10_wx;
    codingCfg->Hdr10Display.hdr10_wy =
        enc_params->specific.enc_h26x_cmd.hdr10_wy;
    codingCfg->Hdr10Display.hdr10_maxluma =
        enc_params->specific.enc_h26x_cmd.hdr10_maxluma;
    codingCfg->Hdr10Display.hdr10_minluma =
        enc_params->specific.enc_h26x_cmd.hdr10_minluma;
  }
  codingCfg->Hdr10LightLevel.hdr10_lightlevel_enable =
      enc_params->specific.enc_h26x_cmd.hdr10_lightlevel_enable;
  if (enc_params->specific.enc_h26x_cmd.hdr10_lightlevel_enable) {
    codingCfg->Hdr10LightLevel.hdr10_maxlight =
        enc_params->specific.enc_h26x_cmd.hdr10_maxlight;
    codingCfg->Hdr10LightLevel.hdr10_avglight =
        enc_params->specific.enc_h26x_cmd.hdr10_avglight;
  }
  codingCfg->RpsInSliceHeader =
      enc_params->specific.enc_h26x_cmd.RpsInSliceHeader;
}

/**
 * @brief get_color_description_from_cmd_h2(), empty function.
 * @param void.
 * @return void.
 */
static void get_color_description_from_cmd_h2(void) {
  // no need to get, just follow the code format.
}

/**
 * @brief encoder_init_h2(), init H2 data structures.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return .
 */
void encoder_init_h2(v4l2_enc_inst *h) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;

#if 1
  // do nothing, as the memory has been set to 0 when create encoder instance.
  (void)params;
#else
  for (int i = 0; i < MAX_GOP_SIZE + 1; i++) params->gopCfgOffset[i] = 0;

  memset(params->gopPicCfg, 0,
         sizeof(VCEncGopPicConfig) * MAX_GOP_PIC_CONFIG_NUM);
  memset(&params->video_in, 0, sizeof(VCEncIn));
#endif
}

/**
 * @brief encoder_check_codec_format_h2(), before encode, confirm if hw support
 * this format.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int, 0 is supported, -1 is unsupported.
 */
int encoder_check_codec_format_h2(v4l2_enc_inst *h) {
  int core_num = 0;

  EWLHwConfig_t configure_t;
#if (defined(NXP) && !defined(USE_H1))  // nxp v2
  core_num = EWLGetCoreNum();
  ASSERT(core_num != 0);

  for (int i = 0; i < core_num; i++) {
    configure_t = EWLReadAsicConfig(i);
    h->configure_t.has_h264 |= configure_t.h264Enabled;
    h->configure_t.has_hevc |= configure_t.hevcEnabled;
    h->configure_t.has_jpeg |= configure_t.jpegEnabled;
    if (h->configure_t.has_h264) {
      h->configure_t.max_width_h264 = configure_t.maxEncodedWidthH264;
    }
    if (h->configure_t.has_hevc) {
      h->configure_t.max_width_hevc = configure_t.maxEncodedWidthHEVC;
    }
    if (h->configure_t.has_jpeg) {
      h->configure_t.max_width_jpeg = configure_t.maxEncodedWidthJPEG;
    }
  }
#elif (!defined(NXP) && !defined(USE_H1))  // current v2
  core_num = EWLGetCoreNum(NULL);
  ASSERT(core_num != 0);

  for (int i = 0; i < core_num; i++) {
    configure_t = EWLReadAsicConfig(i, NULL);
    h->configure_t.has_h264 |= configure_t.h264Enabled;
    h->configure_t.has_hevc |= configure_t.hevcEnabled;
    h->configure_t.has_jpeg |= configure_t.jpegEnabled;
    if (h->configure_t.has_h264) {
      h->configure_t.max_width_h264 = configure_t.maxEncodedWidthH264;
    }
    if (h->configure_t.has_hevc) {
      h->configure_t.max_width_hevc = configure_t.maxEncodedWidthHEVC;
    }
    if (h->configure_t.has_jpeg) {
      h->configure_t.max_width_jpeg = configure_t.maxEncodedWidthJPEG;
    }
  }
#endif
  if ((h->codec_fmt == V4L2_DAEMON_CODEC_ENC_HEVC) &&
      (h->configure_t.has_hevc == 1))
    return 0;
  else if ((h->codec_fmt == V4L2_DAEMON_CODEC_ENC_H264) &&
           (h->configure_t.has_h264 == 1))
    return 0;
  else
    return -1;
}

/**
 * @brief encoder_get_input_h2(), write message from V4l2 parameter to VCEncIn
 * pointer.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param int32_t if_config: if init gop config of encIn.
 * @return void.
 */
void encoder_get_input_h2(v4l2_enc_inst *h, v4l2_daemon_enc_params *enc_params,
                          int32_t if_config) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;
  //    VCEncOut *encOut = &params->video_out;
  uint32_t size_lum = 0;
  uint32_t size_ch = 0;

  getAlignedPicSizebyFormat(
      enc_params->general.inputFormat, enc_params->general.lumWidthSrc,
      enc_params->general.lumHeightSrc, 0, &size_lum, &size_ch, NULL);
#ifndef USE_HW
  encIn->busLuma =
      (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busLuma,
                                  enc_params->io_buffer.busLumaSize);
  if (enc_params->io_buffer.busChromaU != 0)
    encIn->busChromaU =
        (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busChromaU,
                                    enc_params->io_buffer.busChromaUSize);
  else
    encIn->busChromaU = encIn->busLuma + size_lum;

  if (enc_params->io_buffer.busChromaV != 0)
    encIn->busChromaV =
        (ptr_t)mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busChromaV,
                                    enc_params->io_buffer.busChromaVSize);
  else
    encIn->busChromaV = encIn->busChromaU + size_ch / 2;
#else
  encIn->busLuma = enc_params->io_buffer.busLuma;
  if (enc_params->io_buffer.busChromaU != 0)
    encIn->busChromaU = enc_params->io_buffer.busChromaU;
  else
    encIn->busChromaU = encIn->busLuma + size_lum;
  if (enc_params->io_buffer.busChromaV != 0)
    encIn->busChromaV = enc_params->io_buffer.busChromaV;
  else
    encIn->busChromaV = encIn->busChromaU + size_ch / 2;
#endif

  h->saveOriOutBuf = encIn->pOutBuf[0] =
      mmap_phy_addr_daemon(mmap_fd, enc_params->io_buffer.busOutBuf,
                           enc_params->io_buffer.outBufSize);

#ifdef USE_HW
  encIn->busOutBuf[0] = enc_params->io_buffer.busOutBuf;
#else
  encIn->busOutBuf[0] = (ptr_t)encIn->pOutBuf[0];
#endif
  encIn->outBufSize[0] = enc_params->io_buffer.outBufSize;

  encIn->gopSize = enc_params->specific.enc_h26x_cmd.gopSize;
  encIn->gopConfig.pGopPicCfg = params->gopPicCfg;
  encIn->gopConfig.pGopPicSpecialCfg = params->gopPicSpecialCfg;
  encIn->gopConfig.firstPic = enc_params->general.firstPic;
  encIn->gopConfig.lastPic = enc_params->general.lastPic;
  encIn->gopConfig.outputRateDenom = enc_params->general.outputRateDenom;
  encIn->gopConfig.outputRateNumer = enc_params->general.outputRateNumer;
  encIn->gopConfig.inputRateDenom = enc_params->general.inputRateDenom;
  encIn->gopConfig.inputRateNumer = enc_params->general.inputRateNumer;

  encIn->vui_timing_info_enable = 1;

  if (encIn->codingType == VCENC_INTRA_FRAME && !if_config && enc_params->specific.enc_h26x_cmd.idrHdr)
    encIn->resendVPS = encIn->resendSPS = encIn->resendPPS = 1;  // default enabe this value
  else
    encIn->resendVPS = encIn->resendSPS = encIn->resendPPS = 0;

  if (if_config) {
    InitPicConfig(encIn);
  }
  if (enc_params->general.outputRateDenom != 0)
    encIn->timeIncrement = enc_params->general.outputRateDenom;
  else
    encIn->timeIncrement = 1;
  if (enc_params->specific.enc_h26x_cmd.force_idr) {
    encIn->poc = 0;
    encIn->last_idr_picture_cnt = encIn->picture_cnt;
    encIn->codingType = VCENC_INTRA_FRAME;
    encIn->bIsIDR = 1;
    h->next_pic_type = (int)VCENC_INTRA_FRAME;
  }
}

/**
 * @brief encoder_set_parameter_h2(), set encoder parameters.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param v4l2_daemon_enc_params* enc_params: encoder parameters.
 * @param int32_t if_config: if configure the VCEncConfig.
 * @return int: api return value.
 */
int encoder_set_parameter_h2(v4l2_enc_inst *h,
                             v4l2_daemon_enc_params *enc_params,
                             int32_t if_config) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;
  //    VCEncOut *encOut = &params->video_out;
  int ret;

  encoder_get_input_h2(h, enc_params, if_config);
  init_cmdl(h, encIn);

  /* Encoder setup: init instance.*/
  if (if_config == 1) {
    ret = init_gop_config(h, enc_params->specific.enc_h26x_cmd.intraPicRate);
    if (ret != 0) return DAEMON_ERR_ENC_PARA;

    if (get_config_from_cmd_h2(h, enc_params) == -1)
      return DAEMON_ERR_ENC_NOT_SUPPORT;
#if defined(NXP)
    ret = VCEncInit(&params->video_config, &h->inst);
#else
    ret = VCEncInit(&params->video_config, &h->inst, NULL);
#endif
    if (ret != VCENC_OK) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncInit() failed. vsi_ret=%d\n", ret);
      return DAEMON_ERR_ENC_INTERNAL;
    }
  }

  if (if_config == 1) h->total_frames = 0;

  /* Encoder setup: rate control */
  if ((if_config == 1) || (enc_params->specific.enc_h26x_cmd.picRc == 0) ||
      (enc_params->specific.enc_h26x_cmd.hrdConformance != 1 &&
       if_config != 1)) {
    if ((ret = VCEncGetRateCtrl(h->inst, &params->video_rate_ctrl)) !=
        VCENC_OK) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncGetRateCtrl error.\n");
      return DAEMON_ERR_ENC_INTERNAL;
    }
    get_rate_control_from_cmd_h2(h, enc_params);
    if ((ret = VCEncSetRateCtrl(h->inst, &params->video_rate_ctrl)) !=
        VCENC_OK) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncSetRateCtrl error.\n");
      return DAEMON_ERR_ENC_INTERNAL;
    }
  }

  /*Check if the cbr bit rate is changed.*/
  check_if_cbr_bitrate_changed(h, enc_params);

  /*Encoder setup: PreP setup */
  if ((ret = VCEncGetPreProcessing(h->inst, &params->video_prep)) != VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncGetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }
  get_pre_process_from_cmd_h2(h, enc_params);
  if ((ret = VCEncSetPreProcessing(h->inst, &params->video_prep)) != VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncSetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  /* Encoder setup: coding control */
  if ((ret = VCEncGetCodingCtrl(h->inst, &params->video_coding_ctrl)) !=
      VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncGetCodingCtrl error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }
  get_coding_control_from_cmd_h2(h, enc_params);
  if ((ret = VCEncSetCodingCtrl(h->inst, &params->video_coding_ctrl)) !=
      VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncSetPreProcessing error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  get_color_description_from_cmd_h2();
  if ((ret = VCEncSetVuiColorDescription(
           h->inst,
           enc_params->specific.enc_h26x_cmd.vuiVideoSignalTypePresentFlag,
           enc_params->specific.enc_h26x_cmd.vuiVideoFormat,
           enc_params->specific.enc_h26x_cmd.vuiColorDescripPresentFlag,
           enc_params->specific.enc_h26x_cmd.vuiColorPrimaries,
           enc_params->specific.enc_h26x_cmd.vuiTransferCharacteristics,
           enc_params->specific.enc_h26x_cmd.vuiMatrixCoefficients)) !=
      VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncSetVuiColorDescription error.\n");
    return DAEMON_ERR_ENC_INTERNAL;
  }

  return 0;
}

/**
 * @brief encoder_start_h2(), start encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @return int: api return value.
 */
int encoder_start_h2(v4l2_enc_inst *h, uint32_t *stream_size) {
  VCEncRet encRet;
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;
  VCEncOut *encOut = &params->video_out;
  int ret = 0;

  encRet = VCEncStrmStart(h->inst, encIn, encOut);
  if (encRet != VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncStrmStart() failed, ret=%d\n", encRet);
    ret = DAEMON_ERR_ENC_INTERNAL;
  }
  *stream_size = encOut->streamSize;
  return ret;
}

/**
 * @brief encoder_encode_h2(), start encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @param uint32_t* codingType: coding type.
 * @return int: api return value.
 */
int encoder_encode_h2(v4l2_enc_inst *h, uint32_t *stream_size,
                      uint32_t *codingType) {
  VCEncRet encRet;
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;
  VCEncOut *encOut = &params->video_out;
  int ret = 0;

  encIn->pOutBuf[0] = (uint32_t *)((uint8_t *)encIn->pOutBuf[0] + *stream_size);
  encIn->busOutBuf[0] += *stream_size;
  encIn->outBufSize[0] -= *stream_size;

  encRet = VCEncStrmEncodeExt(h->inst, encIn, NULL, encOut, NULL, NULL, 0);
  if (encRet != VCENC_FRAME_READY && encRet != VCENC_FRAME_ENQUEUE) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncStrmEncodeExt() failed, ret=%d\n",
               encRet);
    ret = DAEMON_ERR_ENC_INTERNAL;
  }
  *stream_size += encOut->streamSize;
  *codingType = encIn->codingType;
  if (encRet == VCENC_FRAME_READY) ret = DAEMON_ENC_FRAME_READY;
  else if (encRet == VCENC_FRAME_ENQUEUE) ret = DAEMON_ENC_FRAME_ENQUEUE;
  else ret = DAEMON_ERR_ENC_INTERNAL;
  return ret;
}

/**
 * @brief encoder_find_next_pic_h2(), find buffer and gop structure of next
 * picture.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param BUFFER** p_buffer: buffer to encode
 * @param uint32_t* buf_id: buffer id in buffer list.
 * @return int: api return value.
 */
int encoder_find_next_pic_h2(v4l2_enc_inst *h, BUFFER **p_buffer,
                             uint32_t *buf_id) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;

  if (h->bufferlist_reorder->size >= encIn->gopSize) {
    h->next_pic_type = VCEncFindNextPic(
        h->inst, encIn, encIn->gopSize,
        (const uint8_t *)(&encIn->gopConfig.gopCfgOffset), false);
    *p_buffer =
        bufferlist_find_buffer(h->bufferlist_reorder, encIn->picture_cnt,
                               buf_id);  // match picture_cnt
  }
  return 0;
}

/**
 * @brief reset_enc_h2(), reset encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int: api return value.
 */
void reset_enc_h2(v4l2_enc_inst *h) {
  h2_enc_params *params = (h2_enc_params *)h->enc_params;
  VCEncIn *encIn = &params->video_in;

  //when get reset command, reset picture_cnt
  encIn->picture_cnt = 0;
  h->input_frame_cnt = 0;
  h->output_frame_cnt = 0;
}

/**
 * @brief encoder_end_h2(), stream end, write eos.
 * @param v4l2_enc_inst* h: encoder instance.
 * @param uint32_t* stream_size: srteam size.
 * @return int: api return value.
 */
int encoder_end_h2(v4l2_enc_inst *h, uint32_t *stream_size) {
  VCEncRet ret;
  h2_enc_params *params = (h2_enc_params *)h->enc_params;

  ret = VCEncStrmEnd(h->inst, &params->video_in, &params->video_out);
  if (ret != VCENC_OK) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "VCEncStrmEnd() failed, ret=%d\n", ret);
  }
  *stream_size = params->video_out.streamSize;
  return (int)ret;
}

/**
 * @brief encoder_close_h2(), close encoder.
 * @param v4l2_enc_inst* h: encoder instance.
 * @return int: api return value.
 */
int encoder_close_h2(v4l2_enc_inst *h) {
  VCEncRet ret = 0;

  if (h->inst != NULL) {
    ret = VCEncRelease(h->inst);
    if (ret != VCENC_OK) {
      HANTRO_LOG(HANTRO_LEVEL_INFO, "VCEncRelease failed, ret=%d\n", ret);
    }
    h->inst = NULL;
  }
  if(h->cbr_bitrate_change) return ret;
  if (h->bufferlist_input) bufferlist_flush(h->bufferlist_input);
  if (h->bufferlist_output) bufferlist_flush(h->bufferlist_output);
  if (h->bufferlist_reorder) bufferlist_flush(h->bufferlist_reorder);
  return (int)ret;
}

/**
 * @brief encoder_get_attr_h2(), get encoder attributes.
 * @param v4l2_daemon_codec_fmt fmt: codec format.
 * @return
 */
void encoder_get_attr_h2(v4l2_daemon_codec_fmt fmt, enc_inst_attr *attr) {
  ASSERT(attr);

  if (fmt == V4L2_DAEMON_CODEC_ENC_JPEG) {
#ifndef NXP
    attr->param_size = sizeof(jpeg_enc_params);
#else
    attr->param_size = 0;
#endif
  } else {
    attr->param_size = sizeof(h2_enc_params);
  }
}
