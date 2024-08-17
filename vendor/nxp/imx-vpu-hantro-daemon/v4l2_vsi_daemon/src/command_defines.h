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

#ifndef COMMAND_DEFINES_H
#define COMMAND_DEFINES_H

#include "object_heap.h"
#include "v4l2_base_type.h"

#define MAX_STREAMS 200
#define MAX_GOP_SIZE 8
#define MAX_INTRA_PIC_RATE 0x7fffffff
#define NO_RESPONSE_SEQID 0xFFFFFFFE
#define SEQID_UPLIMT 0x7FFFFFFE

#define OUTF_BASE 0x3ffff000L

#define VSI_DAEMON_DEVMAJOR 100

#define VSI_DAEMON_FNAME "vsi_daemon_ctrl"
#define VSI_DAEMON_PATH "/usr/bin/vsidaemon"
#define PIPE_DEVICE "/dev/vsi_daemon_ctrl"
#define MMAP_DEVICE "/dev/vsi_daemon_ctrl"

#define MEM_PAGE_SIZE (4096)

/* some common defines between driver and daemon */
#define DEFAULTLEVEL 0

#define FRAMETYPE_I (1 << 1)
#define FRAMETYPE_P (1 << 2)
#define FRAMETYPE_B (1 << 3)
#define LAST_BUFFER_FLAG (1 << 4)
#define FORCE_IDR (1 << 5)
#define UPDATE_INFO (1 << 6)
#define ERROR_BUFFER_FLAG (1 << 7)

#define VSI_V4L2_MAX_ROI_REGIONS 8
#define VSI_V4L2_MAX_ROI_REGIONS_H1 2
#define VSI_V4L2_MAX_IPCM_REGIONS 2

#define INVALID_IOBUFF_IDX (-1)

/*-----------------------------------------------------------
        communication with v4l2 driver.
-----------------------------------------------------------*/

struct vsi_v4l2_dev_info {
  int dec_corenum;
  int enc_corenum;
  int enc_isH1;
  unsigned int max_dec_resolution;
  unsigned long decformat;  // hw_dec_formats
  unsigned long encformat;  // hw_enc_formats
};

/* daemon ioctl id definitions */
#define VSIV4L2_IOCTL_BASE 'd'
#define VSI_IOCTL_CMD_BASE _IO(VSIV4L2_IOCTL_BASE, 0x44)

/* user space daemno should use this ioctl to initial HW info to v4l2 driver */
#define VSI_IOCTL_CMD_INITDEV \
  _IOW(VSIV4L2_IOCTL_BASE, 45, struct vsi_v4l2_dev_info)
/* end of daemon ioctl id definitions */

/*these two enum have same sequence, identical to the table vsi_coded_fmt[] in
 * vsi-v4l2-config.c */
enum hw_enc_formats {
  ENC_HAS_HEVC = 0,
  ENC_HAS_H264,
  ENC_HAS_JPEG,
  ENC_HAS_VP8,
  ENC_HAS_VP9,
  ENC_HAS_AV1,
  ENC_FORMATS_MAX,
};

enum hw_dec_formats {
  DEC_HAS_HEVC = 0,
  DEC_HAS_H264,
  DEC_HAS_JPEG,
  DEC_HAS_VP8,
  DEC_HAS_VP9,
  DEC_HAS_AV1,

  DEC_HAS_MPEG2,
  DEC_HAS_MPEG4,
  DEC_HAS_H263,
  DEC_HAS_VC1_G,
  DEC_HAS_VC1_L,
  DEC_HAS_RV,
  DEC_HAS_AVS2,
  DEC_HAS_XVID,
  DEC_HAS_CSC,
  DEC_FORMATS_MAX
};
/*above two enum have same sequence, identical to the table vsi_coded_fmt[] in
 * vsi-v4l2-config.c */

typedef enum {
  /*  every command should mark which kind of parameters is valid.
   *      For example, V4L2_DAEMON_VIDIOC_BUF_RDY can contains input or output
   * buffers.
   *          also it can contains other parameters.  */
  V4L2_DAEMON_VIDIOC_STREAMON = 0,  // for streamon and start
  V4L2_DAEMON_VIDIOC_BUF_RDY,
  V4L2_DAEMON_VIDIOC_CMD_STOP,   // this is for flush.
  V4L2_DAEMON_VIDIOC_DESTROY_ENC,   //enc destroy
  V4L2_DAEMON_VIDIOC_ENC_RESET,   //enc reset, as in spec
  //above are enc cmds

  V4L2_DAEMON_VIDIOC_FAKE,  // fake command.

  /*Below is for decoder*/
  V4L2_DAEMON_VIDIOC_S_EXT_CTRLS,
  V4L2_DAEMON_VIDIOC_RESET_BITRATE,
  V4L2_DAEMON_VIDIOC_CHANGE_RES,
  V4L2_DAEMON_VIDIOC_G_FMT,
  V4L2_DAEMON_VIDIOC_S_SELECTION,
  V4L2_DAEMON_VIDIOC_S_FMT,
  V4L2_DAEMON_VIDIOC_PACKET,            // tell daemon a frame is ready.
  V4L2_DAEMON_VIDIOC_STREAMON_CAPTURE,  // for streamon and start
  V4L2_DAEMON_VIDIOC_STREAMON_OUTPUT,
  V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE,
  V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT,
  V4L2_DAEMON_VIDIOC_CMD_START,
  V4L2_DAEMON_VIDIOC_FRAME,
  V4L2_DAEMON_VIDIOC_DESTROY_DEC,

  V4L2_DAEMON_VIDIOC_EXIT,  // daemon should exit itself
  V4L2_DAEMON_VIDIOC_PICCONSUMED,
  V4L2_DAEMON_VIDIOC_CROPCHANGE,
  V4L2_DAEMON_VIDIOC_WARNONOPTION,
  V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE_DONE,
  V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT_DONE,
  V4L2_DAEMON_VIDIOC_TOTAL_AMOUNT,
} v4l2_daemon_cmd_id;

typedef enum {
  /*enc format, identical to VCEncVideoCodecFormat except name*/
  V4L2_DAEMON_CODEC_ENC_HEVC = 0,
  V4L2_DAEMON_CODEC_ENC_H264,
  V4L2_DAEMON_CODEC_ENC_AV1,
  V4L2_DAEMON_CODEC_ENC_VP8,
  V4L2_DAEMON_CODEC_ENC_VP9,
  V4L2_DAEMON_CODEC_ENC_MPEG2,
  V4L2_DAEMON_CODEC_ENC_JPEG,

  /*dec format*/
  V4L2_DAEMON_CODEC_DEC_HEVC,
  V4L2_DAEMON_CODEC_DEC_H264,
  V4L2_DAEMON_CODEC_DEC_JPEG,
  V4L2_DAEMON_CODEC_DEC_VP9,
  V4L2_DAEMON_CODEC_DEC_MPEG2,
  V4L2_DAEMON_CODEC_DEC_MPEG4,
  V4L2_DAEMON_CODEC_DEC_VP8,
  V4L2_DAEMON_CODEC_DEC_H263,
  V4L2_DAEMON_CODEC_DEC_VC1_G,
  V4L2_DAEMON_CODEC_DEC_VC1_L,
  V4L2_DAEMON_CODEC_DEC_RV,
  V4L2_DAEMON_CODEC_DEC_AVS2,
  V4L2_DAEMON_CODEC_DEC_XVID,
  V4L2_DAEMON_CODEC_UNKNOW_TYPE,
} v4l2_daemon_codec_fmt;

typedef enum {
  VSI_V4L2_DECOUT_DEFAULT,
  VSI_V4L2_DEC_PIX_FMT_NV12,
  VSI_V4L2_DEC_PIX_FMT_400,
  VSI_V4L2_DEC_PIX_FMT_411SP,
  VSI_V4L2_DEC_PIX_FMT_422SP,
  VSI_V4L2_DEC_PIX_FMT_444SP,
  
  VSI_V4L2_DECOUT_DTRC,
  VSI_V4L2_DECOUT_P010,
  VSI_V4L2_DECOUT_NV12_10BIT,
  VSI_V4L2_DECOUT_DTRC_10BIT,
  VSI_V4L2_DECOUT_RFC,
  VSI_V4L2_DECOUT_RFC_10BIT,
} vsi_v4l2dec_pixfmt;

typedef enum {
  DAEMON_OK = 0,                   // no error.
  DAEMON_ENC_FRAME_READY = 1,      // frame encoded
  DAEMON_ENC_FRAME_ENQUEUE = 2,    // frame enqueued

  DAEMON_ERR_INST_CREATE = -1,       // inst_init() failed.
  DAEMON_ERR_SIGNAL_CONFIG = -2,     // sigsetjmp() failed.
  DAEMON_ERR_DAEMON_MISSING = -3,     // daemon is not alive.
  DAEMON_ERR_NO_MEM = -4,     		// no mem, used also by driver.
	
  DAEMON_ERR_ENC_PARA = -100,        // Parameters Error.
  DAEMON_ERR_ENC_NOT_SUPPORT = -101, // Not Support Error.
  DAEMON_ERR_ENC_INTERNAL = -102,    // Ctrlsw reported Error.
  DAEMON_ERR_ENC_BUF_MISSED = -103,  // No desired input buffer.
  DAEMON_ERR_ENC_FATAL_ERROR = -104,  // Fatal error.

  DAEMON_ERR_DEC_FATAL_ERROR = -200, // Fatal error.
  DAEMON_ERR_DEC_METADATA_ONLY = -201,  // CMD_STOP after metadata-only.
}vsi_v4l2daemon_err;

//warn type attached in V4L2_DAEMON_VIDIOC_WARNONOPTION message. Stored in msg.error member
enum {
	UNKONW_WARNTYPE = -1,		//not known warning type
	WARN_ROIREGION,			//(part of)roi region can not work with media setting and be ignored by enc
	WARN_IPCMREGION,			//(part of)ipcm region can not work with media setting and be ignored by enc
	WARN_LEVEL,				//current level cant't work with media setting and be updated by enc
};

typedef struct v4l2_daemon_enc_buffers {
  /*IO*/
  int32_t inbufidx;   // from v4l2 driver, don't modify it
  int32_t outbufidx;  //-1:invalid, other:valid.

  vpu_addr_t busLuma;
  int32_t busLumaSize;
  vpu_addr_t busChromaU;
  int32_t busChromaUSize;
  vpu_addr_t busChromaV;
  int32_t busChromaVSize;

  vpu_addr_t busLumaOrig;
  vpu_addr_t busChromaUOrig;
  vpu_addr_t busChromaVOrig;

  vpu_addr_t busOutBuf;
  uint32_t outBufSize;

  uint32_t bytesused;  // valid bytes in buffer from user app.
  int64_t timestamp;
} v4l2_daemon_enc_buffers;

typedef struct v4l2_daemon_enc_general_cmd {
  int32_t valid;  // 0:invalid, 1:valid.

  /*frame property*/
  int32_t outputRateNumer; /* Output frame rate numerator */
  int32_t outputRateDenom; /* Output frame rate denominator */
  int32_t inputRateNumer;  /* Input frame rate numerator */
  int32_t inputRateDenom;  /* Input frame rate denominator */

  int32_t firstPic;
  int32_t lastPic;

  int32_t width;         // encode width
  int32_t height;        // encode height
  int32_t lumWidthSrc;   // input width
  int32_t lumHeightSrc;  // input height

  int32_t inputFormat;  // input format
  int32_t bitPerSecond;

  int32_t rotation;  // prep
  int32_t mirror;
  int32_t horOffsetSrc;
  int32_t verOffsetSrc;
  int32_t colorConversion;
  int32_t scaledWidth;
  int32_t scaledHeight;
  int32_t scaledOutputFormat;

  int32_t codecFormat;
} v4l2_daemon_enc_general_cmd;

typedef struct v4l2_daemon_enc_h26x_cmd {
  int32_t valid;       // 0:invalid, 1:valid.
  int32_t byteStream;  // byteStream

  int32_t enableCabac;    /* [0,1] H.264 entropy coding mode, 0 for CAVLC, 1 for
                             CABAC */
  int32_t cabacInitFlag;  // cabacInitFlag

  int32_t profile;   /*main profile or main still picture profile*/
  int32_t tier;      /*main tier or high tier*/
  int32_t avclevel;  /*h264 main profile level*/
  int32_t hevclevel; /*hevc main profile level*/

  uint32_t strong_intra_smoothing_enabled_flag;  // intra setup

  int32_t cirStart;  // cir
  int32_t cirInterval;

  int32_t intraAreaEnable;  // intra area
  int32_t intraAreaTop;
  int32_t intraAreaLeft;
  int32_t intraAreaBottom;
  int32_t intraAreaRight;

  int32_t pcm_loop_filter_disabled_flag;

  int32_t ipcmAreaEnable[VSI_V4L2_MAX_IPCM_REGIONS];
  int32_t ipcmAreaTop[VSI_V4L2_MAX_IPCM_REGIONS];  // ipcm area 1, 2
  int32_t ipcmAreaLeft[VSI_V4L2_MAX_IPCM_REGIONS];
  int32_t ipcmAreaBottom[VSI_V4L2_MAX_IPCM_REGIONS];
  int32_t ipcmAreaRight[VSI_V4L2_MAX_IPCM_REGIONS];

  int32_t ipcmMapEnable;  // ipcm map
  uint8_t *ipcmMapBuf;

  int32_t skipMapEnable;  // skip map
  int32_t skipMapBlockUnit;
  uint8_t *skipMapBuf;

  int32_t roiAreaEnable[VSI_V4L2_MAX_ROI_REGIONS];  // 8 roi for H2, 2 roi for
                                                    // H1
  int32_t roiAreaTop[VSI_V4L2_MAX_ROI_REGIONS];
  int32_t roiAreaLeft[VSI_V4L2_MAX_ROI_REGIONS];
  int32_t roiAreaBottom[VSI_V4L2_MAX_ROI_REGIONS];
  int32_t roiAreaRight[VSI_V4L2_MAX_ROI_REGIONS];
  int32_t roiDeltaQp[VSI_V4L2_MAX_ROI_REGIONS];  // roiQp has higher priority
                                                 // than roiDeltaQp
  int32_t roiQp[VSI_V4L2_MAX_ROI_REGIONS];  // only H2 use it

  uint32_t roiMapDeltaQpBlockUnit;  // roimap cuctrl
  uint32_t roiMapDeltaQpEnable;
  uint32_t RoiCuCtrlVer;
  uint32_t RoiQpDeltaVer;
  uint8_t *roiMapDeltaQpBuf;
  uint8_t *cuCtrlInfoBuf;

  /* Rate control parameters */
  int32_t hrdConformance;
  int32_t cpbSize;
  int32_t intraPicRate; /* IDR interval */
  int32_t vbr;          /* Variable Bit Rate Control by qpMin */
  int32_t qpHdr;
  int32_t qpHdrI_h26x;  // for 264/5 I frame QP
  int32_t qpHdrP_h26x;  // for 264/5 P frame PQ
  int32_t qpMin_h26x;
  int32_t qpMax_h26x;
  int32_t qpHdrI_vpx;  // for vpx I frame QP
  int32_t qpHdrP_vpx;  // for vpx P frame PQ
  int32_t qpMin_vpx;
  int32_t qpMax_vpx;
  int32_t qpMinI;
  int32_t qpMaxI;
  int32_t bitVarRangeI;
  int32_t bitVarRangeP;
  int32_t bitVarRangeB;
  uint32_t u32StaticSceneIbitPercent;
  int32_t tolMovingBitRate; /*tolerance of max Moving bit rate */
  int32_t monitorFrames;    /*monitor frame length for moving bit rate*/
  int32_t picRc;
  int32_t ctbRc;
  int32_t blockRCSize;
  uint32_t rcQpDeltaRange;
  uint32_t rcBaseMBComplexity;
  int32_t picSkip;
  int32_t picQpDeltaMin;
  int32_t picQpDeltaMax;
  int32_t ctbRcRowQpStep;
  int32_t tolCtbRcInter;
  int32_t tolCtbRcIntra;
  int32_t bitrateWindow;
  int32_t intraQpDelta;
  int32_t fixedIntraQp;
  int32_t bFrameQpDelta;
  int32_t disableDeblocking;
  int32_t enableSao;
  int32_t tc_Offset;
  int32_t beta_Offset;
  int32_t chromaQpOffset;

  int32_t smoothPsnrInGOP;  // smooth psnr
  int32_t sliceSize;        // multi slice

  int32_t enableDeblockOverride;  // deblock
  int32_t deblockOverride;

  int32_t enableScalingList;  // scale list

  uint32_t compressor;  // rfc

  int32_t interlacedFrame;  // pregress/interlace
  int32_t fieldOrder;       // field order
  int32_t ssim;

  int32_t sei;  // sei
  int8_t *userData;

  uint32_t gopSize;  // gop
  int8_t *gopCfg;
  uint32_t gopLowdelay;
  int32_t outReconFrame;

  uint32_t longTermGap;  // longterm
  uint32_t longTermGapOffset;
  uint32_t ltrInterval;
  int32_t longTermQpDelta;

  int32_t gdrDuration;  // gdr

  int32_t bitDepthLuma;  // 10 bit
  int32_t bitDepthChroma;

  uint32_t enableOutputCuInfo;  // cu info

  uint32_t rdoLevel;  // rdo

  int32_t constChromaEn; /* constant chroma control */
  uint32_t constCb;
  uint32_t constCr;

  int32_t skip_frame_enabled_flag; /*for skip frame encoding ctr*/
  int32_t skip_frame_poc;

  /* HDR10 */
  uint32_t hdr10_display_enable;
  uint32_t hdr10_dx0;
  uint32_t hdr10_dy0;
  uint32_t hdr10_dx1;
  uint32_t hdr10_dy1;
  uint32_t hdr10_dx2;
  uint32_t hdr10_dy2;
  uint32_t hdr10_wx;
  uint32_t hdr10_wy;
  uint32_t hdr10_maxluma;
  uint32_t hdr10_minluma;

  uint32_t hdr10_lightlevel_enable;
  uint32_t hdr10_maxlight;
  uint32_t hdr10_avglight;

  uint32_t hdr10_color_enable;
  uint32_t hdr10_primary;
  uint32_t hdr10_transfer;
  uint32_t hdr10_matrix;

  uint32_t RpsInSliceHeader;
  uint32_t P010RefEnable;
  uint32_t vui_timing_info_enable;

  uint32_t picOrderCntType;
  uint32_t log2MaxPicOrderCntLsb;
  uint32_t log2MaxFrameNum;

  uint32_t lookaheadDepth;
  uint32_t halfDsInput;
  uint32_t cuInfoVersion;
  uint32_t parallelCoreNum;

  uint32_t force_idr;

  uint32_t vuiVideoSignalTypePresentFlag;  // 1
  uint32_t vuiVideoFormat;                 // default 5
  int32_t videoRange;
  uint32_t vuiColorDescripPresentFlag;  // 1 if elems below exist
  uint32_t vuiColorPrimaries;
  uint32_t vuiTransferCharacteristics;
  uint32_t vuiMatrixCoefficients;

  uint32_t idrHdr;
} v4l2_daemon_enc_h26x_cmd;

typedef struct v4l2_daemon_enc_jpeg_cmd {
  int32_t valid;  // 0:invalid, 1:valid.
  int32_t restartInterval;
  int32_t frameType;
  int32_t partialCoding;
  int32_t codingMode;
  int32_t markerType;
  int32_t qLevel;
  int32_t unitsType;
  int32_t xdensity;
  int32_t ydensity;
  int32_t thumbnail;
  int32_t widthThumb;
  int32_t heightThumb;
  int32_t lumWidthSrcThumb;
  int32_t lumHeightSrcThumb;
  int32_t horOffsetSrcThumb;
  int32_t verOffsetSrcThumb;
  int32_t write;
  int32_t comLength;
  int32_t mirror;
  int32_t formatCustomizedType;
  int32_t constChromaEn;
  uint32_t constCb;
  uint32_t constCr;
  int32_t losslessEnable;
  int32_t predictMode;
  int32_t ptransValue;
  uint32_t bitPerSecond;
  uint32_t mjpeg;
  int32_t rcMode;
  int32_t picQpDeltaMin;
  int32_t picQpDeltaMax;
  uint32_t qpmin;
  uint32_t qpmax;
  int32_t fixedQP;
  uint32_t exp_of_input_alignment;
  uint32_t streamBufChain;
  uint32_t streamMultiSegmentMode;
  uint32_t streamMultiSegmentAmount;
} v4l2_daemon_enc_jpeg_cmd;

typedef struct v4l2_daemon_enc_params {
  v4l2_daemon_enc_buffers io_buffer;
  v4l2_daemon_enc_general_cmd general;
  union {
    v4l2_daemon_enc_h26x_cmd enc_h26x_cmd;
    v4l2_daemon_enc_jpeg_cmd enc_jpeg_cmd;
  } specific;
} v4l2_daemon_enc_params;

typedef struct v4l2_daemon_dec_buffers {
  /*IO*/
  int32_t inbufidx;   // from v4l2 driver, don't modify it
  int32_t outbufidx;  //-1:invalid, other:valid.

  vpu_addr_t busInBuf;
  uint32_t inBufSize;
  int32_t inputFormat;  // input format
  int32_t srcwidth;     // encode width
  int32_t srcheight;    // encode height
  // infer output
  vpu_addr_t busOutBuf;  // for Y or YUV
  int32_t OutBufSize;
  vpu_addr_t busOutBufUV;
  int32_t OutUVBufSize;
  int32_t outBufFormat;
  int32_t output_width;
  int32_t output_height;
  int32_t output_wstride;
  int32_t output_hstride;
  int32_t outputPixelDepth;

  vpu_addr_t rfc_luma_offset;
  vpu_addr_t rfc_chroma_offset;

  uint32_t bytesused;  // valid bytes in buffer from user app.
  int64_t timestamp;

  int32_t no_reordering_decoding;
  int32_t securemode_on;
} v4l2_daemon_dec_buffers;

// stub struct
typedef struct v4l2_daemon_dec_pp_cfg {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} v4l2_daemon_dec_pp_cfg;

typedef struct v4l2_vpu_hdr10_meta {
  unsigned int hasHdr10Meta;
  unsigned int redPrimary[2];
  unsigned int greenPrimary[2];
  unsigned int bluePrimary[2];
  unsigned int whitePoint[2];
  unsigned int maxMasteringLuminance;
  unsigned int minMasteringLuminance;
  unsigned int maxContentLightLevel;
  unsigned int maxFrameAverageLightLevel;
} v4l2_vpu_hdr10_meta;

typedef struct v4l2_daemon_dec_info {
  uint32_t frame_width;
  uint32_t frame_height;
  uint32_t bit_depth;
  struct {
    uint32_t left;
    uint32_t top;
    uint32_t width;
    uint32_t height;
  } visible_rect;
  uint32_t needed_dpb_nums;
  uint32_t dpb_buffer_size;
  uint32_t pic_wstride;
  v4l2_daemon_dec_pp_cfg pp_params;
  uint32_t colour_description_present_flag;
  uint32_t matrix_coefficients;
  uint32_t colour_primaries;
  uint32_t transfer_characteristics;
  uint32_t video_range;
  vsi_v4l2dec_pixfmt src_pix_fmt;
  v4l2_vpu_hdr10_meta vpu_hdr10_meta;
} v4l2_daemon_dec_info;

typedef struct v4l2_daemon_pic_info {
  uint32_t width;
  uint32_t height;
  uint32_t crop_left;
  uint32_t crop_top;
  uint32_t crop_width;
  uint32_t crop_height;
  uint32_t pic_wstride;
} v4l2_daemon_pic_info;

struct v4l2_daemon_dec_resochange_params {
  v4l2_daemon_dec_buffers io_buffer;
  v4l2_daemon_dec_info dec_info;
};

struct v4l2_daemon_dec_pictureinfo_params {
  // v4l2_daemon_dec_buffers io_buffer;
  v4l2_daemon_pic_info pic_info;
};

typedef struct v4l2_daemon_dec_params {
  union {
    v4l2_daemon_dec_buffers io_buffer;
    struct v4l2_daemon_dec_resochange_params dec_info;
    struct v4l2_daemon_dec_pictureinfo_params pic_info;
  };
  //	struct TBCfg general;
} v4l2_daemon_dec_params;

typedef struct vsi_v4l2_msg_hdr {
  int32_t size;
  int32_t error;
  unsigned long seq_id;
  unsigned long inst_id;
  v4l2_daemon_cmd_id cmd_id;
  v4l2_daemon_codec_fmt codec_fmt;
  int32_t param_type;  // for save memory. 0:no parameters. 1:enc_params.
} vsi_v4l2_msg_hdr;

typedef struct vsi_v4l2_msg {
  int32_t size;
  int32_t error;
  unsigned long seq_id;
  unsigned long inst_id;
  v4l2_daemon_cmd_id cmd_id;
  v4l2_daemon_codec_fmt codec_fmt;
  uint32_t param_type;
  // above part must be identical to vsi_v4l2_msg_hdr

  union {
    v4l2_daemon_enc_params enc_params;
    v4l2_daemon_dec_params dec_params;
  } params;
} vsi_v4l2_msg;

typedef struct vsi_v4l2_cmd {
  struct object_base base;
  vsi_v4l2_msg msg;
} vsi_v4l2_cmd;

#endif  //#ifndef COMMAND_DEFINES_H
