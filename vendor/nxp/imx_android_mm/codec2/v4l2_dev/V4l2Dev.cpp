/**
 *  Copyright 2018-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "V4l2Dev"
#include "V4l2Dev.h"

#include <linux/videodev2.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>

#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include "graphics_ext.h"
#include "Imx_ext.h"
#include "C2Config.h"
#include <linux/imx_vpu.h>

namespace android {

#define MAX_VIDEO_SEARCH_NODE (20)
static std::atomic<std::int32_t> gDevIndex[V4L2_DEV_END] = {-1, -1, -1, -1};

static int is_v4l2_mplane(struct v4l2_capability *cap)
{
    if (cap->capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE)
			&& cap->capabilities & V4L2_CAP_STREAMING)
        return true;

    if (cap->capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)
        return true;

    return false;
}

V4l2Dev::V4l2Dev()
{
    memset((char*)sDevName, 0, MAX_DEV_NAME_LEN);
    nFd = -1;
    nEventFd = -1;
    mStreamType = V4L2_PIX_FMT_H264;
    nOutBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    nCapBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
}
int32_t V4l2Dev::Open(V4l2DEV_TYPE type){

    if(OK != SearchName(type))
        return -1;

    ALOGD("open dev name %s", (char*)sDevName);

    nFd = open ((char*)sDevName, O_RDWR | O_NONBLOCK);

    if(nFd > 0) {
        struct v4l2_event_subscription  sub;
        memset(&sub, 0, sizeof(struct v4l2_event_subscription));

        sub.type = V4L2_EVENT_SOURCE_CHANGE;
        ioctl(nFd, VIDIOC_SUBSCRIBE_EVENT, &sub); 

        sub.type = V4L2_EVENT_CODEC_ERROR;
        ioctl(nFd, VIDIOC_SUBSCRIBE_EVENT, &sub);

    }

    nEventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    return nFd;
}
status_t V4l2Dev::Close()
{
    if(nFd >= 0){
        close(nFd);
        nFd = -1;
    }

    if(nEventFd >= 0){
        close(nEventFd);
        nEventFd = -1;
    }
    return OK;
}

status_t V4l2Dev::GetVideoBufferType(enum v4l2_buf_type *outType, enum v4l2_buf_type *capType)
{
    struct v4l2_capability cap;

    if (ioctl(nFd, VIDIOC_QUERYCAP, &cap) != 0) {
        ALOGE("%s failed", __FUNCTION__);
        return BAD_VALUE;
    }

    if (is_v4l2_mplane(&cap)) {
        nCapBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        nOutBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    } else {
        nCapBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        nOutBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    }

    *outType = nOutBufType;
    *capType = nCapBufType;

    return OK;
}

bool V4l2Dev::CheckVsiV4l2DeviceType(V4l2DEV_TYPE type, int fd)
{
    if(type != V4L2_DEV_DECODER && type != V4L2_DEV_ENCODER)
        return false;

    bool isVsiV4l2Dev = false;

    enum v4l2_buf_type outBufferType, capBufferType;

    nFd = fd; // temporary set nFd to check if it's vsi v4l2 node

    GetVideoBufferType(&outBufferType, &capBufferType);

    if (type == V4L2_DEV_DECODER
            && IsOutputFormatSupported(V4L2_PIX_FMT_H264)
            && IsCaptureFormatSupported(V4L2_PIX_FMT_NV12)) {
        isVsiV4l2Dev = true;
    }
    else if (type == V4L2_DEV_ENCODER
                && IsOutputFormatSupported(V4L2_PIX_FMT_NV12)
                && IsCaptureFormatSupported(V4L2_PIX_FMT_H264)) {
        isVsiV4l2Dev = true;
    }

    nFd = -1;

    if (!isVsiV4l2Dev) {
        output_formats.clear();
        capture_formats.clear();
    }

    return isVsiV4l2Dev;
}

status_t V4l2Dev::SearchName(V4l2DEV_TYPE type)
{
    int32_t devType = (int32_t)type;
    int32_t index = 0;

    if(!(devType >= V4L2_DEV_START && devType < V4L2_DEV_END))
        return UNKNOWN_ERROR;

    if(gDevIndex[devType] < 0)
        Init();

    if(gDevIndex[devType] >= 0)
    {
        index = gDevIndex[devType];
        sprintf((char*)sDevName, "/dev/video%d", index);
        ALOGV("SearchName get %s for %d device", (char *)sDevName,devType);
        return OK;
    }

    return UNKNOWN_ERROR;
}
void V4l2Dev::Init(){

    int32_t index = 0;
    int32_t fd = -1;
    char name[MAX_DEV_NAME_LEN];
    struct v4l2_capability cap;

    while(index < MAX_VIDEO_SEARCH_NODE) {
        int32_t devType = -1;

        sprintf((char*)name, "/dev/video%d", index);

        fd = open ((char*)name, O_RDWR);
        if(fd < 0){
            ALOGV("open index %d failed\n",index);
            goto SEARCH_NEXT;
        }
        if (ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0) {
            ALOGV("VIDIOC_QUERYCAP %d failed\n",index);
            goto SEARCH_NEXT;
        }
        ALOGV("index %d name=%s, card name=%s, bus info %s\n",index,(char*)cap.driver, (char*)cap.card, (char*)cap.bus_info);

        if((!strcmp((char*)cap.card, "vsi_v4l2dec") || !strcmp((char*)cap.card, "amphion vpu decoder"))){
            devType = V4L2_DEV_DECODER;
        } else if((!strcmp((char*)cap.card, "vsi_v4l2enc") || !strcmp((char*)cap.card, "amphion vpu encoder"))){
            devType = V4L2_DEV_ENCODER;
        } else if((!strcmp((char*)cap.card, "mxc-isi-m2m"))){
            // do more check for isi
            if (!((cap.capabilities & (V4L2_CAP_VIDEO_M2M |
                            V4L2_CAP_VIDEO_M2M_MPLANE)) ||
                    ((cap.capabilities &
                            (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) &&
                        (cap.capabilities &
                            (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE))))){
                goto SEARCH_NEXT;
            }
            if((isV4lBufferTypeSupported(fd,V4L2_DEV_ISI,V4L2_BUF_TYPE_VIDEO_OUTPUT)||
                isV4lBufferTypeSupported(fd,V4L2_DEV_ISI,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) &&
                (isV4lBufferTypeSupported(fd,V4L2_DEV_ISI,V4L2_BUF_TYPE_VIDEO_CAPTURE)||
                isV4lBufferTypeSupported(fd,V4L2_DEV_ISI,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))){
                devType = V4L2_DEV_ISI;
            }
        } else if((strstr((char*)cap.bus_info, "jpegdec") != NULL)){
            devType = V4L2_DEV_IMX_JPEG_DEC;
        }

        if(devType >= V4L2_DEV_START && devType < V4L2_DEV_END){
            if(gDevIndex[devType] < 0)
                gDevIndex[devType] = index;
            ALOGV("SearchName set %d for %d device", index, devType);
        }

SEARCH_NEXT:
        if (fd >= 0)
            close(fd);
        index ++;
        continue;
    }

    return;
}

bool V4l2Dev::isV4lBufferTypeSupported(int32_t fd,V4l2DEV_TYPE dec_type, uint32_t v4l2_buf_type )
{
    uint32_t i = 0;
    bool bGot = false;
    struct v4l2_fmtdesc sFmt;

    while(true){
        sFmt.index = i;
        sFmt.type = v4l2_buf_type;
        if(ioctl(fd,VIDIOC_ENUM_FMT,&sFmt) < 0)
            break;

        i++;
        if(v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT || v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE){
            if(dec_type == V4L2_DEV_DECODER && (sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = true;
                break;
            }else if(dec_type == V4L2_DEV_ENCODER && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = true;
                break;
            }else if(dec_type == V4L2_DEV_ISI && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = true;
                break;
            }
        }else if(v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE || v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE){
            if(dec_type == V4L2_DEV_DECODER && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = true;
                break;
            }else if(dec_type == V4L2_DEV_ENCODER && (sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = true;
                break;
            }else if(dec_type == V4L2_DEV_ISI && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = true;
                break;
            }
        }
    }

    return bGot;

}
status_t V4l2Dev::QueryFormats(uint32_t format_type)
{
    struct v4l2_fmtdesc fmt;
    int32_t i = 0;
    if(format_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE || format_type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        output_formats.clear();
        while(true){
            fmt.type = format_type;
            fmt.index = i;
            if (ioctl(nFd,VIDIOC_ENUM_FMT,&fmt) < 0) {
                ALOGV("VIDIOC_ENUM_FMT fail");
                break;
            }

            output_formats.push_back(fmt.pixelformat);
            ALOGV("QueryFormat add output format %x\n",fmt.pixelformat);
            i++;
        }
        if(output_formats.size() > 0)
            return OK;
        else
            return UNKNOWN_ERROR;
    }
    else if(format_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || format_type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        capture_formats.clear();
        while(true){
            fmt.type = format_type;
            fmt.index = i;
            if (ioctl(nFd,VIDIOC_ENUM_FMT,&fmt) < 0)
                break;

            capture_formats.push_back(fmt.pixelformat);
            ALOGD("QueryFormat add capture format %x\n",fmt.pixelformat);
            i++;
        }

        if(capture_formats.size() > 0)
            return OK;
        else
            return UNKNOWN_ERROR;
    }
    return BAD_TYPE;
}

bool V4l2Dev::IsOutputFormatSupported(uint32_t format)
{
    ALOGD("IsOutputFormatSupported format=%x", format);
    if(output_formats.empty()){
        status_t ret = QueryFormats(nOutBufType);
        if(ret != OK)
            return false;
    }

    for (uint32_t i = 0; i < output_formats.size(); i++) {
        if(format == output_formats.at(i)){
            return true;
        }
    }

    return false;
}
bool V4l2Dev::IsCaptureFormatSupported(uint32_t format)
{
    ALOGD("IsCaptureFormatSupported format=%x", format);
    if(capture_formats.empty()){
        status_t ret = QueryFormats(nCapBufType);
        if(ret != OK)
            return false;
    }

    for (uint32_t i = 0; i < capture_formats.size(); i++) {
        if(format == capture_formats.at(i)){
            return true;
        }
    }
    return false;
}

typedef struct {
    uint32_t noncontiguous_format;
    uint32_t contiguous_format;
} CONTINUGUOUS_FORMAT_TABLE;

//TODO: add android pixel format
static const CONTINUGUOUS_FORMAT_TABLE contiguous_format_table[]={
    { V4L2_PIX_FMT_NV12M, V4L2_PIX_FMT_NV12},
    { V4L2_PIX_FMT_YUV420M, V4L2_PIX_FMT_YUV420},
    { V4L2_PIX_FMT_YVU420M, V4L2_PIX_FMT_YVU420},
    { V4L2_PIX_FMT_NV12M_8L128, V4L2_PIX_FMT_NV12_8L128},
    { V4L2_PIX_FMT_NV12M_10BE_8L128, V4L2_PIX_FMT_NV12_10BE_8L128},
};

status_t V4l2Dev:: GetContiguousV4l2Format(uint32_t format, uint32_t *contiguous_format)
{
    status_t ret = BAD_VALUE;
    for (size_t i = 0; i < sizeof(contiguous_format_table)/sizeof(CONTINUGUOUS_FORMAT_TABLE); i++) {
        if (format == contiguous_format_table[i].noncontiguous_format) {
            *contiguous_format = contiguous_format_table[i].contiguous_format;
            ret = OK;
            break;
        }
    }

    if (ret)
        ALOGE("unknown contiguous v4l2 format 0x%x", format);

    return ret;
}

status_t V4l2Dev:: GetCaptureFormat(uint32_t *format, uint32_t i)
{
    status_t ret = OK;

    if(capture_formats.empty()){
        ret = QueryFormats(nCapBufType);
        if(ret != OK)
            return ret;
    }

    if (i >= capture_formats.size())
        return BAD_VALUE;

    *format = capture_formats.at(i);

    return ret;
}

typedef struct{
    const char * mime;
    uint32_t v4l2_format;
}V4L2_FORMAT_TABLE;

static const V4L2_FORMAT_TABLE v4l2_format_table[]={
    { MEDIA_MIMETYPE_VIDEO_AVC, V4L2_PIX_FMT_H264 },
    { MEDIA_MIMETYPE_VIDEO_HEVC, V4L2_PIX_FMT_HEVC},
    { MEDIA_MIMETYPE_VIDEO_H263, V4L2_PIX_FMT_H263 },
    { MEDIA_MIMETYPE_VIDEO_MPEG4, V4L2_PIX_FMT_MPEG4 },
    { MEDIA_MIMETYPE_VIDEO_MPEG2, V4L2_PIX_FMT_MPEG2 },
    { MEDIA_MIMETYPE_VIDEO_VP8, V4L2_PIX_FMT_VP8 },
    { MEDIA_MIMETYPE_VIDEO_VP9, V4L2_PIX_FMT_VP9 },
    { MEDIA_MIMETYPE_VIDEO_VC1, V4L2_PIX_FMT_VC1_ANNEX_L },
    { MEDIA_MIMETYPE_VIDEO_XVID, V4L2_PIX_FMT_XVID },
    { MEDIA_MIMETYPE_VIDEO_REAL, v4l2_fourcc('R', 'V', '0', '0')},
    { MEDIA_MIMETYPE_VIDEO_MJPEG, V4L2_PIX_FMT_JPEG },
    { MEDIA_MIMETYPE_VIDEO_SORENSON, v4l2_fourcc('S', 'P', 'K', '0')},
#if 0//TODO: add extended format
    { MEDIA_MIMETYPE_VIDEO_DIV3, v4l2_fourcc('D', 'I', 'V', '3') },
    { MEDIA_MIMETYPE_VIDEO_DIV4, v4l2_fourcc('D', 'I', 'V', 'X') },
    { MEDIA_MIMETYPE_VIDEO_DIVX, v4l2_fourcc('D', 'I', 'V', 'X') },
#endif
};

#define FLAG_10BIT_FORMAT   (1)
typedef struct{
    uint32_t color_format;
    uint32_t v4l2_format;
    uint32_t flag;
}COLOR_FORMAT_TABLE;

//TODO: add android pixel format
static const COLOR_FORMAT_TABLE color_format_table[]={
    { HAL_PIXEL_FORMAT_YCbCr_420_SP, V4L2_PIX_FMT_NV12, 0},
    { HAL_PIXEL_FORMAT_NV12_TILED, V4L2_PIX_FMT_NV12_8L128, 0},
    { HAL_PIXEL_FORMAT_P010_TILED, V4L2_PIX_FMT_NV12_10BE_8L128, FLAG_10BIT_FORMAT},
    { HAL_PIXEL_FORMAT_P010, V4L2_PIX_FMT_NV12X, FLAG_10BIT_FORMAT},
    { HAL_PIXEL_FORMAT_YCBCR_P010 , V4L2_PIX_FMT_P010, FLAG_10BIT_FORMAT},
    { HAL_PIXEL_FORMAT_YCbCr_422_SP, V4L2_PIX_FMT_NV16, 0},
    { HAL_PIXEL_FORMAT_YCbCr_422_I, V4L2_PIX_FMT_YUYV, 0},
    { HAL_PIXEL_FORMAT_RGB_565, V4L2_PIX_FMT_RGB565, 0},
    { HAL_PIXEL_FORMAT_RGB_888, V4L2_PIX_FMT_RGB24, 0},
    { HAL_PIXEL_FORMAT_RGBA_8888, V4L2_PIX_FMT_RGBA32, 0},
    { HAL_PIXEL_FORMAT_RGBX_8888, V4L2_PIX_FMT_RGBX32, 0},
    { HAL_PIXEL_FORMAT_BGRA_8888, V4L2_PIX_FMT_BGRA32, 0},
#ifdef AMPHION_V4L2
    { HAL_PIXEL_FORMAT_YCbCr_420_P, V4L2_PIX_FMT_NV12, 0},//workaround
#else
    // YV12 -> YUV420M, use multiple planes to swicth UV
    { HAL_PIXEL_FORMAT_YCbCr_420_P, V4L2_PIX_FMT_YUV420M, 0}
#endif
};
status_t V4l2Dev::GetStreamTypeByMime(const char * mime, uint32_t * format_type)
{

    for( size_t i = 0; i < sizeof(v4l2_format_table)/sizeof(V4L2_FORMAT_TABLE); i++){
        if (!strcmp(mime, v4l2_format_table[i].mime)) {
            mStreamType = v4l2_format_table[i].v4l2_format;
            *format_type = mStreamType;
            return OK;
        }
    }

    *format_type = 0x0;
    return ERROR_UNSUPPORTED;
}

status_t V4l2Dev::GetMimeByStreamType(uint32_t format_type, const char ** mime)
{
    for( size_t i = 0; i < sizeof(v4l2_format_table)/sizeof(V4L2_FORMAT_TABLE); i++){
        if (format_type == v4l2_format_table[i].v4l2_format) {
            *mime = v4l2_format_table[i].mime;
            return OK;
        }
    }

    *mime = NULL;
    return ERROR_UNSUPPORTED;
}

status_t V4l2Dev::GetColorFormatByV4l2(uint32_t v4l2_format, uint32_t * color_format)
{
    for( size_t i = 0; i < sizeof(color_format_table)/sizeof(COLOR_FORMAT_TABLE); i++){
        if (v4l2_format == color_format_table[i].v4l2_format) {
            *color_format = color_format_table[i].color_format;
            return OK;
        }
    }
    *color_format = 0;
    return ERROR_UNSUPPORTED;
}
status_t V4l2Dev::GetV4l2FormatByColor(uint32_t color_format, uint32_t * v4l2_format)
{
    for( size_t i = 0; i < sizeof(color_format_table)/sizeof(COLOR_FORMAT_TABLE); i++){
        if (color_format == color_format_table[i].color_format) {
            *v4l2_format = color_format_table[i].v4l2_format;
            return OK;
        }
    }
    *v4l2_format = 0;
    return ERROR_UNSUPPORTED;
}
bool V4l2Dev::Is10BitV4l2Format(uint32_t v4l2_format)
{
    for( size_t i = 0; i < sizeof(color_format_table)/sizeof(COLOR_FORMAT_TABLE); i++){
        if (v4l2_format == color_format_table[i].v4l2_format) {
            return (color_format_table[i].flag & FLAG_10BIT_FORMAT);
        }
    }
    return false;
}

status_t V4l2Dev::GetFormatFrameInfo(uint32_t format, struct v4l2_frmsizeenum * info)
{
    if(info == NULL)
        return BAD_TYPE;

    info->index = 0;
    info->type = V4L2_FRMSIZE_TYPE_STEPWISE;
    info->pixel_format = format;

    if(0 == ioctl(nFd, VIDIOC_ENUM_FRAMESIZES, info)){
        return OK;
    }

    return UNKNOWN_ERROR;
}
uint32_t V4l2Dev::Poll()
{
    uint32_t ret = V4L2_DEV_POLL_NONE;
    int r;
    struct pollfd pfd[2];
    struct timespec ts;
    ts.tv_sec = 0;//default timeout 1 seconds
    ts.tv_nsec = 300000000;

    pfd[0].fd = nFd;
    pfd[0].events = POLLERR | POLLNVAL | POLLHUP;
    pfd[0].revents = 0;

    pfd[0].events |= POLLOUT | POLLPRI | POLLWRNORM;
    pfd[0].events |= POLLIN | POLLRDNORM;

    pfd[1].fd = nEventFd;
    pfd[1].events = POLLIN | POLLERR;
    pfd[1].revents = 0;

    ALOGV("Poll BEGIN %p\n",this);
    r = ppoll (&pfd[0], 2, &ts, NULL);

    if(r <= 0){
        ret = V4L2_DEV_POLL_NONE;
    }else{
        if((pfd[1].revents & POLLIN) || (pfd[1].revents & POLLERR)){
            ret = V4L2_DEV_POLL_NONE;
            return ret;
        }

        if(pfd[0].revents & POLLPRI){
            ALOGV("[%p]POLLPRI \n",this);
            ret |= V4L2_DEV_POLL_EVENT;
        }

        if((pfd[0].revents & POLLIN) || (pfd[0].revents & POLLRDNORM)){
            ret |= V4L2_DEV_POLL_CAPTURE;
        }
        if((pfd[0].revents & POLLOUT) || (pfd[0].revents & POLLWRNORM)){
            ret |= V4L2_DEV_POLL_OUTPUT;
        }

        if(pfd[0].revents & POLLERR){
            ret |= V4L2_DEV_POLL_ERROR;
        }
    }

    ALOGV("Poll END,ret=%x\n",ret);
    return ret;
}
status_t V4l2Dev::SetPollInterrupt()
{
    if(nEventFd > 0){
        const uint64_t buf = EFD_CLOEXEC|EFD_NONBLOCK;
        eventfd_write(nEventFd, buf);
    }
    return OK;
}
status_t V4l2Dev::ClearPollInterrupt()
{
    if(nEventFd > 0){
        uint64_t buf;
        eventfd_read(nEventFd, &buf);
    }
    return OK;
}
status_t V4l2Dev::ResetDecoder()
{
    int ret = 0;
    struct v4l2_decoder_cmd cmd;
    memset(&cmd, 0, sizeof(struct v4l2_decoder_cmd));

    cmd.cmd = V4L2_DEC_CMD_RESET;
    cmd.flags = V4L2_DEC_CMD_STOP_IMMEDIATELY;

    ret = ioctl(nFd, VIDIOC_DECODER_CMD, &cmd);
    ALOGV("V4l2Dev::ResetDecoder ret=%x %s\n",ret, ret<0?"FAIL":"SUCCESS");
    return OK;
}
status_t V4l2Dev::StopDecoder()
{
    int ret = 0;
    struct v4l2_decoder_cmd cmd;
    memset(&cmd, 0, sizeof(struct v4l2_decoder_cmd));

    cmd.cmd = V4L2_DEC_CMD_STOP;
    cmd.flags = V4L2_DEC_CMD_STOP_IMMEDIATELY;

    ret = ioctl(nFd, VIDIOC_DECODER_CMD, &cmd);
    if(ret < 0){
        ALOGV("V4l2Dev::StopDecoder ret=%x\n",ret);
        return UNKNOWN_ERROR;
    }

    ALOGV("V4l2Dev::StopDecoder SUCCESS\n");
    return OK;
}
status_t V4l2Dev::EnableLowLatencyDecoder(bool enabled)
{

    int ret = 0;
    struct v4l2_control ctl = { 0,0 };
    ctl.id = V4L2_CID_DIS_REORDER;
    ctl.value = enabled;
    ret = ioctl(nFd, VIDIOC_S_CTRL, &ctl);

    if(ret < 0){
        ALOGV("V4l2Dev::EnableLowLatencyDecoder ret=%x\n",ret);
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t V4l2Dev::EnableSecureMode(bool enabled)
{

    int ret = 0;
    struct v4l2_control ctl = { 0,0 };
    ctl.id = V4L2_CID_SECUREMODE;
    ctl.value = enabled;
    ret = ioctl(nFd, VIDIOC_S_CTRL, &ctl);

    if(ret < 0){
        ALOGV("V4l2Dev::EnableSecureMode ret=%x\n",ret);
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t V4l2Dev::StopEncoder()
{
    int ret = 0;

    struct v4l2_encoder_cmd cmd;
    memset(&cmd, 0, sizeof(struct v4l2_encoder_cmd));

    cmd.cmd = V4L2_ENC_CMD_STOP;
    cmd.flags = V4L2_ENC_CMD_STOP_AT_GOP_END;
    ret = ioctl(nFd, VIDIOC_ENCODER_CMD, &cmd);

    if(ret < 0){
        ALOGV("V4l2Dev::StopEncoder FAILED\n");
        return UNKNOWN_ERROR;
    }

    ALOGV("V4l2Dev::StopEncoder SUCCESS\n");
    return OK;
}
status_t V4l2Dev::SetEncoderParam(V4l2EncInputParam *param)
{
    int ret = 0;
    if(param == NULL)
        return UNKNOWN_ERROR;

    ALOGV("SetEncoderParam nBitRate=%d\n",param->nBitRate);
    ALOGV("SetEncoderParam nGOPSize=%d\n",param->nGOPSize);
    ALOGV("SetEncoderParam nIntraFreshNum=%d\n",param->nIntraFreshNum);
    ret = SetEncoderBitrate(param->nBitRate);
    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_BITRATE_MODE, param->nBitRateMode);

    // constant qp: rate control disable
    // cbr/vbr: rate control enable so that encoder can control bitrate
    if (param->nBitRateMode == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE, 0);
    else
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE, 1);

    if(param->nGOPSize > 0)
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_GOP_SIZE,param->nGOPSize);

    ALOGV("SetEncoderParam qp %s bitrate mode %s",
        param->qpValid ? "valid" : "invalid",
        param->nBitRateMode == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR ? "CBR" : "VBR");
    ALOGV("SetEncoderParam i_qp=%d,p_qp=%d,b_qp=%d,min_qp=%d,max_qp=%d \n",
        param->i_qp, param->p_qp, param->b_qp, param->min_qp, param->max_qp);

    if (param->qpValid) {
        switch(param->format){
            case V4L2_PIX_FMT_H264:{
                if(param->i_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP, param->i_qp);
                if(param->p_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP, param->p_qp);
                if(param->b_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP, param->b_qp);
                if(param->min_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_MIN_QP, param->min_qp);
                if(param->max_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_MAX_QP, param->max_qp);
                break;
            }
            case V4L2_PIX_FMT_VP8:{
                if(param->i_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_VPX_I_FRAME_QP, param->i_qp);
                if(param->p_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_VPX_P_FRAME_QP, param->p_qp);
                if(param->min_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_VPX_MIN_QP, param->min_qp);
                if(param->max_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_VPX_MAX_QP, param->max_qp);
                break;
            }
            case V4L2_PIX_FMT_HEVC:{
                if(param->i_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP, param->i_qp);
                if(param->p_qp > 0)
                    ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP, param->p_qp);
                //TODO: set min & max qp for hevc
                break;
            }
            default:
                ALOGE("format %x need qp",param->format);
                break;
        }
    }

    if(param->nIntraFreshNum > 0)
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB,param->nIntraFreshNum);

    ALOGV("SetEncoderParam ret=%x nIntraFreshNum=%d\n",ret,param->nIntraFreshNum);

    //ignore result
    int32_t value= 1;
    if(90 == param->nRotAngle || 270 == param->nRotAngle)
        SetCtrl(V4L2_CID_HFLIP,value);
    else if(0 == param->nRotAngle || 180 == param->nRotAngle)
        SetCtrl(V4L2_CID_VFLIP,value);

    ret = SetEncoderProfileAndLevel(param->nProfile, param->nLevel);

    ALOGV("SetEncoderProfileAndLevel ret=%x\n",ret);
    return ret;
}
status_t V4l2Dev::SetEncoderProfileAndLevel(uint32_t profile, uint32_t level)
{
    int ret = 0;

    if (mStreamType == V4L2_PIX_FMT_H264) {
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_PROFILE, profile);
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_H264_LEVEL, level);
    } else if (mStreamType == V4L2_PIX_FMT_HEVC) {
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_HEVC_PROFILE, profile);
        ret |= SetCtrl(V4L2_CID_MPEG_VIDEO_HEVC_LEVEL, level);
    }
    ALOGV("set profile=%d,level=%d,ret=%d",profile,level,ret);
    return OK;
}

status_t V4l2Dev::SetFrameRate(uint32_t framerate)
{
    struct v4l2_streamparm parm;
    int ret = 0;

    if (0 == framerate)
        return UNKNOWN_ERROR;

    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    parm.parm.capture.timeperframe.numerator = 0x1000;
    parm.parm.capture.timeperframe.denominator = framerate * 0x1000;
    ALOGV("set frame rate =%d",framerate);
    ret = ioctl(nFd, VIDIOC_S_PARM, &parm);
    if (ret) {
        ALOGE("SetFrameRate fail\n");
        return UNKNOWN_ERROR;
    }
    return OK;
}

status_t V4l2Dev::SetForceKeyFrame()
{
    return SetCtrl(V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
}

status_t V4l2Dev::SetCtrl(uint32_t id, int32_t value)
{
    int ret = 0;
    struct v4l2_control ctl = { 0,0 };
    ctl.id = id;
    ctl.value = value;
    ret = ioctl(nFd, VIDIOC_S_CTRL, &ctl);

    return (status_t)ret;
}

typedef struct{
    uint32_t iso_value;
    uint32_t v4l2_value;
}V4L2_ISO_MAP;

static const V4L2_ISO_MAP v4l2_color_table[]={
    { 1, V4L2_COLORSPACE_REC709 },
    { 4, V4L2_COLORSPACE_470_SYSTEM_M },
    { 5, V4L2_COLORSPACE_470_SYSTEM_BG },
    { 6, V4L2_COLORSPACE_SMPTE170M },
    { 7, V4L2_COLORSPACE_SMPTE240M },
    { 8, V4L2_COLORSPACE_GENERIC_FILM },
    { 9, V4L2_COLORSPACE_BT2020 },
    { 10, V4L2_COLORSPACE_BT2020 },
};
static const V4L2_ISO_MAP v4l2_xfer_table[]={
    { 1, V4L2_XFER_FUNC_709 },
    { 4, V4L2_XFER_FUNC_GAMMA22 },
    { 5, V4L2_XFER_FUNC_GAMMA28 },
    { 6, V4L2_XFER_FUNC_709 },
    { 7, V4L2_XFER_FUNC_SMPTE240M },
    { 8, V4L2_XFER_FUNC_NONE },
    { 11, V4L2_XFER_FUNC_XVYCC },
    { 12, V4L2_XFER_FUNC_BT1361 },
    { 13, V4L2_XFER_FUNC_SRGB },
    { 14, V4L2_XFER_FUNC_709 },
    { 16, V4L2_XFER_FUNC_SMPTE2084 },
    { 17, V4L2_XFER_FUNC_ST428 },
    { 18, V4L2_XFER_FUNC_HLG },
};
static const V4L2_ISO_MAP v4l2_ycbcr_table[]={
    { 1, V4L2_YCBCR_ENC_709 },
    { 4, V4L2_YCBCR_ENC_BT470_6M },
    { 5, V4L2_YCBCR_ENC_601 },
    { 6, V4L2_YCBCR_ENC_601 },
    { 7, V4L2_YCBCR_ENC_SMPTE240M },
    { 9, V4L2_YCBCR_ENC_BT2020 },
    { 10, V4L2_YCBCR_ENC_BT2020_CONST_LUM },
};
status_t V4l2Dev::GetColorAspectsInfo(uint32_t colorspace, uint32_t xfer_func,
                                            uint32_t ycbcr_enc, uint32_t quantization,
                                            VideoColorAspect * desc)
{
    if(desc == NULL)
        return UNKNOWN_ERROR;

    ALOGV("GetColorAspectsInfo %d %d %d %d", colorspace, xfer_func, ycbcr_enc, quantization);

    desc->colourPrimaries = 2; //iso value 2, map to ColorAspects::PrimariesUnspecified
    for( size_t i = 0; i < sizeof(v4l2_color_table)/sizeof(V4L2_ISO_MAP); i++){
        if (colorspace == v4l2_color_table[i].v4l2_value) {
            desc->colourPrimaries = v4l2_color_table[i].iso_value;
            break;
        }
    }

    desc->transferCharacteristics = 2; //iso value 2, map to ColorAspects::TransferUnspecified
    for( size_t i = 0; i < sizeof(v4l2_xfer_table)/sizeof(V4L2_ISO_MAP); i++){
        if (xfer_func == v4l2_xfer_table[i].v4l2_value) {
            desc->transferCharacteristics = v4l2_xfer_table[i].iso_value;
            break;
        }
    }

    desc->matrixCoeffs = 2; //iso value 2, map toColorAspects::MatrixUnspecified
    for( size_t i = 0; i < sizeof(v4l2_ycbcr_table)/sizeof(V4L2_ISO_MAP); i++){
        if (ycbcr_enc == v4l2_ycbcr_table[i].v4l2_value) {
            desc->matrixCoeffs = v4l2_ycbcr_table[i].iso_value;
            break;
        }
    }

    desc->fullRange = (quantization == V4L2_QUANTIZATION_FULL_RANGE) ? 1:0;

    // if all parameters are not initialized, return error to notify user there's no color aspects info
    if (0 == desc->colourPrimaries &&
            0 == desc->transferCharacteristics &&
            2 == desc->matrixCoeffs &&
            V4L2_QUANTIZATION_DEFAULT == quantization)
        return BAD_VALUE;

    ALOGV("getColorAspectsInfo success, p=%d,t=%d,m=%d,r=%d\n",
        desc->colourPrimaries,desc->transferCharacteristics,desc->matrixCoeffs,desc->fullRange);

    return OK;
}
status_t V4l2Dev::SetColorAspectsInfo(VideoColorAspect * desc, struct v4l2_pix_format_mplane * pixel_fmt)
{
    if(pixel_fmt == NULL || desc == NULL)
        return UNKNOWN_ERROR;

    pixel_fmt->colorspace = V4L2_COLORSPACE_DEFAULT;
    for( size_t i = 0; i < sizeof(v4l2_color_table)/sizeof(V4L2_ISO_MAP); i++){
        if (desc->colourPrimaries == v4l2_color_table[i].iso_value) {
            pixel_fmt->colorspace = v4l2_color_table[i].v4l2_value;
            break;
        }
    }

    pixel_fmt->xfer_func = V4L2_XFER_FUNC_DEFAULT;
    for( size_t i = 0; i < sizeof(v4l2_xfer_table)/sizeof(V4L2_ISO_MAP); i++){
        if (desc->transferCharacteristics == v4l2_xfer_table[i].iso_value) {
            pixel_fmt->xfer_func = v4l2_xfer_table[i].v4l2_value;
            break;
        }
    }

    pixel_fmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
    for( size_t i = 0; i < sizeof(v4l2_ycbcr_table)/sizeof(V4L2_ISO_MAP); i++){
        if (desc->matrixCoeffs == v4l2_ycbcr_table[i].iso_value) {
            pixel_fmt->ycbcr_enc = v4l2_ycbcr_table[i].v4l2_value;
            break;
        }
    }

    pixel_fmt->quantization = (desc->fullRange)? V4L2_QUANTIZATION_FULL_RANGE:V4L2_QUANTIZATION_LIM_RANGE;
    ALOGV("SetColorAspectsInfo success, p=%d,t=%d,m=%d,r=%d\n",
        pixel_fmt->colorspace,pixel_fmt->xfer_func,pixel_fmt->ycbcr_enc,pixel_fmt->quantization);
    return OK;
}
status_t V4l2Dev::SetEncoderBitrate(int32_t bitrate){
    return SetCtrl(V4L2_CID_MPEG_VIDEO_BITRATE, bitrate);
}

status_t V4l2Dev::EnableSeparateMode(){
    return SetCtrl(V4L2_CID_MPEG_VIDEO_HEADER_MODE, V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE);
}

}
