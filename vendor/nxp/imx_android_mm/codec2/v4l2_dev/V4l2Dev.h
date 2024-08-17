/**
 *  Copyright 2019-2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#ifndef V4L2_DEV_H
#define V4L2_DEV_H
#include <stdint.h>
#include <sys/types.h>
#include <linux/videodev2.h>
#include <media/stagefright/foundation/AHandler.h>
#include <vector>

namespace android {

typedef enum{
V4L2_DEV_START = 0,
V4L2_DEV_DECODER = V4L2_DEV_START,
V4L2_DEV_ENCODER,
V4L2_DEV_ISI,
V4L2_DEV_IMX_JPEG_DEC,
V4L2_DEV_IMX_JPEG_ENC,
V4L2_DEV_END,
}V4l2DEV_TYPE;


#define V4L2_DEV_POLL_NONE 0
#define V4L2_DEV_POLL_EVENT 1
#define V4L2_DEV_POLL_OUTPUT 2
#define V4L2_DEV_POLL_CAPTURE 4
#define V4L2_DEV_POLL_ERROR 8

#define MAX_DEV_NAME_LEN (16)

#define V4L2_FORMAT_IS_NON_CONTIGUOUS_PLANES(fmt) \
	((fmt) == V4L2_PIX_FMT_NV12M        \
	 || (fmt) == V4L2_PIX_FMT_YUV420M   \
     || (fmt) == V4L2_PIX_FMT_YVU420M   \
	 || (fmt) == V4L2_PIX_FMT_NV12M_8L128   \
     || (fmt) == V4L2_PIX_FMT_NV12M_10BE_8L128)

typedef struct {
    uint32_t colourPrimaries;
    uint32_t transferCharacteristics;
    uint32_t matrixCoeffs;
    uint32_t fullRange;
} VideoColorAspect;

typedef struct {
    int32_t format;
    int32_t nBitRate;/*unit: bps*/
    int32_t nBitRateMode;
    int32_t nGOPSize;
    int32_t qpValid;
    int32_t i_qp;
    int32_t p_qp;
    int32_t b_qp;
    int32_t min_qp;
    int32_t max_qp;
    int32_t nIntraFreshNum;//V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB
    int32_t nProfile;
    int32_t nLevel;
    int32_t nRotAngle;
    int32_t nFrameRate;
} V4l2EncInputParam;

class V4l2Dev{
public:
    explicit V4l2Dev();
    int32_t Open(V4l2DEV_TYPE type);
    status_t Close();
    status_t GetVideoBufferType(enum v4l2_buf_type *outType, enum v4l2_buf_type *capType);
    bool IsOutputFormatSupported(uint32_t format);
    bool IsCaptureFormatSupported(uint32_t format);

    status_t GetContiguousV4l2Format(uint32_t format, uint32_t *contiguous_format);
    status_t GetCaptureFormat(uint32_t *format, uint32_t i);
    status_t GetFormatFrameInfo(uint32_t format, struct v4l2_frmsizeenum * info);

    status_t GetStreamTypeByMime(const char * mime, uint32_t * format_type);
    status_t GetMimeByStreamType(uint32_t format_type, const char ** mime);

    status_t GetColorFormatByV4l2(uint32_t v4l2_format, uint32_t * color_format);
    status_t GetV4l2FormatByColor(uint32_t color_format, uint32_t * v4l2_format);
    bool Is10BitV4l2Format(uint32_t color_format);

    uint32_t Poll();
    status_t SetPollInterrupt();
    status_t ClearPollInterrupt();
    status_t ResetDecoder();
    status_t StopDecoder();

    status_t EnableLowLatencyDecoder(bool enabled);
    status_t EnableSecureMode(bool enabled);
    status_t GetColorAspectsInfo(uint32_t colorspace, uint32_t xfer_func,
                                            uint32_t ycbcr_enc, uint32_t quantization,
                                            VideoColorAspect * desc);

    //encoder functions
    status_t StopEncoder();
    status_t SetEncoderParam(V4l2EncInputParam *param);
    status_t SetEncoderProfileAndLevel(uint32_t profile, uint32_t level);
    status_t SetFrameRate(uint32_t framerate);
    status_t SetForceKeyFrame();
    status_t SetColorAspectsInfo(VideoColorAspect * desc, struct v4l2_pix_format_mplane * pixel_fmt);
    status_t SetEncoderBitrate(int32_t bitrate);
    status_t EnableSeparateMode();

private:
    status_t SearchName(V4l2DEV_TYPE type);
    void Init();
    bool isV4lBufferTypeSupported(int32_t fd, V4l2DEV_TYPE dec_type, uint32_t v4l2_buf_type);
    status_t QueryFormats(uint32_t format_type);

    status_t SetCtrl(uint32_t id, int32_t value);

    bool CheckVsiV4l2DeviceType(V4l2DEV_TYPE type, int fd);

    char sDevName[MAX_DEV_NAME_LEN];
    int32_t nFd;
    int32_t nEventFd;
    int32_t mStreamType;
    enum v4l2_buf_type nOutBufType;
    enum v4l2_buf_type nCapBufType;
    std::vector<uint32_t> output_formats;
    std::vector<uint32_t> capture_formats;
};
}
#endif
