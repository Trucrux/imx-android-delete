/****************************************************************************
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 VeriSilicon Holdings Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <memory.h>
#ifdef IMX_G2D
# include <g2dExt.h>
# include <imx/linux/dma-buf.h>
#endif

#ifdef APPMODE_V4L2
# include <linux/videodev2.h>
#endif

#include "log.h"
#define LOGTAG "DrmDisplay"

#ifdef WITH_DRM
#include <MediaBuffer.h>
#include "DrmDisplay.h"
#include <math.h>

static int drm_create_fb(int fd, int index, struct drm_buffer *buf)
{
    struct drm_mode_create_dumb creq;
    struct drm_mode_destroy_dumb dreq;
    struct drm_mode_map_dumb mreq;
    int ret;

    memset(&creq, 0, sizeof(creq));
    creq.width = buf->width;
    creq.height = buf->height;
    creq.bpp = 32;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
        ALOGE("cannot create dumb buffer[%d]", index);
        return ret;
    }

    buf->stride = creq.pitch;
    buf->size = creq.size;
    buf->handle = creq.handle;

    ret = drmModeAddFB(fd, buf->width, buf->height, creq.bpp, creq.bpp,
                buf->stride, buf->handle, &buf->buf_id);

    if (ret < 0) {
        ALOGE("Add framebuffer (%d) fail", index);
        goto destroy_fb;
    }

#ifdef IMX_G2D
    struct dma_buf_phys dmabuf;
    struct drm_prime_handle prime;
    memset(&prime, 0, sizeof(prime));
    prime.handle = buf->handle;
    if (drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) < 0) {
        ALOGE("Failed to ioctl DRM_IOCTL_PRIME_HANDLE_TO_FD!");
        goto remove_fb;
    }
    if (ioctl(prime.fd, DMA_BUF_IOCTL_PHYS, &dmabuf) < 0) {
        ALOGE("Failed to ioctl DMA_BUF_IOCTL_PHYS!");
        goto remove_fb;
    }
    buf->paddr = dmabuf.phys;
#endif

    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = buf->handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
        ALOGE("Map buffer[%d] dump ioctl fail", index);
        goto remove_fb;
    }

    buf->fb_base = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                            fd, mreq.offset);
    if (buf->fb_base == MAP_FAILED) {
        ALOGE("Cannot mmap dumb buffer[%d]", index);
        goto remove_fb;
    }
    memset(buf->fb_base, 0, buf->size);
    return 0;

remove_fb:
    drmModeRmFB(fd, buf->buf_id);
destroy_fb:
    memset(&dreq, 0, sizeof(dreq));
    dreq.handle = buf->handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    ALOGE("Create DRM buffer[%d] fail", index);
    return ret;
}

static void drm_destroy_fb(int fd, int index, struct drm_buffer *buf)
{
    struct drm_mode_destroy_dumb dreq;

    munmap(buf->fb_base, buf->size);
    drmModeRmFB(fd, buf->buf_id);

    memset(&dreq, 0, sizeof(dreq));
    dreq.handle = buf->handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    buf->fb_base = NULL;
}


static int modeset_find_crtc(struct drm_device *drm,
                drmModeRes *res, drmModeConnector *conn)
{
    drmModeEncoder *encoder;
    int drm_fd = drm->drm_fd;
    int crtc_id, j, i;

    for (i = 0; i < conn->count_encoders; i++) {
        encoder = drmModeGetEncoder(drm_fd, conn->encoders[i]);
        if (!encoder) {
            ALOGE("can't retrieve encoders[%d]", i);
            continue;
        }

        for (j = 0; j < res->count_crtcs; j++) {
            if (encoder->possible_crtcs & (1 << j)) {
                crtc_id = res->crtcs[j];
                if (crtc_id > 0) {
                    drm->crtc_id = crtc_id;
                    drmModeFreeEncoder(encoder);
                    return 0;
                }
            }
            crtc_id = -1;
        }

        if (j == res->count_crtcs && crtc_id == -1) {
            ALOGE("cannot find crtc");
            drmModeFreeEncoder(encoder);
            continue;
        }
        drmModeFreeEncoder(encoder);
    }
    ALOGE("cannot find suitable CRTC for connector[%d]", conn->connector_id);
    return -ENOENT;
}


static int modeset_setup_dev(struct drm_device *drm,
                drmModeRes *res, drmModeConnector *conn)
{
    struct drm_buffer *buf = drm->buffers;
    int i, ret;

    ret = modeset_find_crtc(drm, res, conn);
    if (ret < 0)
        return ret;

    drm->count_modes = conn->count_modes;
    drm->modes = (drmModeModeInfoPtr)malloc(sizeof(drmModeModeInfo) * drm->count_modes);
    memcpy(drm->modes,conn->modes,sizeof(drmModeModeInfo) * drm->count_modes);

    memcpy(&drm->mode, &conn->modes[0], sizeof(drm->mode));
    /* Double buffering */
    for (i = 0; i < 2; i++) {
        buf[i].width  = conn->modes[0].hdisplay;
        buf[i].height = conn->modes[0].vdisplay;
        ret = drm_create_fb(drm->drm_fd, i, &buf[i]);
        if (ret < 0) {
            while(i)
                drm_destroy_fb(drm->drm_fd, i - 1, &buf[i-1]);
            return ret;
        }
        ALOGD("DRM bufffer[%d] addr=0x%p size=%d w/h=(%d,%d) buf_id=%d",
                 i, buf[i].fb_base, buf[i].size,
                 buf[i].width, buf[i].height, buf[i].buf_id);
    }
    drm->bits_per_pixel = 32;
    drm->bytes_per_pixel = drm->bits_per_pixel >> 3;
    return 0;
}

static int open_drm_device(struct drm_device *drm)
{
    char dev_name[15];
    uint64_t has_dumb;
    int fd, i;

    i = 0;
loop:
    sprintf(dev_name, "/dev/dri/card%d", i++);

    fd = open(dev_name, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        ALOGE("Open %s fail", dev_name);
        return -1;
    }

    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 ||
        !has_dumb) {
        ALOGI("drm device '%s' does not support dumb buffers", dev_name);
        close(fd);
        if (i < 4)
            goto loop;
    }
    drm->drm_fd = fd;
    drm->card_id = --i;

    ALOGI("Open %s success", dev_name);

    return 0;
}

static int drm_device_prepare(struct drm_device *drm)
{
    drmModeRes *res;
    drmModeConnector *conn;
    int drm_fd = drm->drm_fd;
    int ret, i;

    ret = drmSetMaster(drm_fd);
    if (ret < 0) {
        ALOGE("drmSetMaster fail: %d %s", ret,  strerror(ret));
        //return ret;
    }

    res = drmModeGetResources(drm_fd);
    if (res == NULL) {
        ALOGE("Cannot retrieve DRM resources");
        drmDropMaster(drm_fd);
        return -errno;
    }

    /* iterate all connectors */
    for (i = 0; i < res->count_connectors; i++) {
        /* get information for each connector */
        conn = drmModeGetConnector(drm_fd, res->connectors[i]);
        if (!conn) {
            ALOGE("Cannot retrieve DRM connector %u:%u (%d)",
                i, res->connectors[i], errno);
            continue;
        }

        /* valid connector? */
        if (conn->connection != DRM_MODE_CONNECTED ||
                    conn->count_modes == 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        /* find a valid connector */
        drm->conn_id = conn->connector_id;
        ret = modeset_setup_dev(drm, res, conn);
        if (ret < 0) {
            ALOGE("mode setup device environment fail");
            drmDropMaster(drm_fd);
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            return ret;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);
    return 0;
}


static int drm_adaptive_resolution(struct drm_device *drm, int width,int height)
{
    int i,ret;
    struct drm_buffer *buf = drm->buffers;

    for (i=0; i<drm->count_modes; i++)
    {
        if ((drm->modes[i].hdisplay == width) && (drm->modes[i].vdisplay == height))
        {
            break;
        }
    }

    if  ((drm->modes[i].hdisplay != width) || (drm->modes[i].vdisplay != height))
    {
         ALOGE("%s:DRM not support resolution %d*%d\n",__func__,width,height);
        return -1;
    }

    memcpy(&drm->mode, &drm->modes[i], sizeof(drm->mode));

    if (buf[0].fb_base != NULL)
    {
        drm_destroy_fb(drm->drm_fd, 0, &buf[0]);
    }
    if (buf[1].fb_base != NULL)
    {
        drm_destroy_fb(drm->drm_fd, 1, &buf[1]);
    }
    
    for (i = 0; i < 2; i++) {
        buf[i].width  = drm->mode.hdisplay;
        buf[i].height = drm->mode.vdisplay;
        ret = drm_create_fb(drm->drm_fd, i, &buf[i]);
        if (ret < 0) {
            while(i)
                drm_destroy_fb(drm->drm_fd, i - 1, &buf[i-1]);
            return ret;
        }
    }
    return 0;
}

static inline int parse_display_mode(const char *arg)
{
    int mode = -1;

    if (strcmp(arg, "full") == 0)
        mode = VIV_DISPLAY_MODE_FULL;
    else if (strcmp(arg, "lpbox") == 0)
        mode = VIV_DISPLAY_MODE_LPBOX;
    else if (strcmp(arg, "panscan") == 0)
        mode = VIV_DISPLAY_MODE_PANSCAN;

    return mode;
}

static int calc_aspect_ratio(const int width, const int height)
{
    if (width <= 0 || height <= 0)
    {
        return -1;
    }

    if (fabs( (float) ((height * 4) / (width * 3) - 1)) < 0.1)
    {
        return VIV_DISPLAY_ASPECT_4_3;
    }
    else if (fabs( (float) ((height * 16) / (width * 9) - 1)) < 0.1)
    {
        return VIV_DISPLAY_ASPECT_16_9;
    }
    else
    {
        return VIV_DISPLAY_ASPECT_OTHERS;
    }
}

static int get_display_mode(int *display_mode)
{
    FILE *fp;
    char lineStr[128];
    char cmd[128];
    char params[128];

    if ( (fp = fopen("VSI_Monitor.cfg", "r")) == NULL)
    {
        ALOGI("No VSI_Monitor.cfg, Use default display mode:full");
        return 0;
    }

    while(!feof(fp))
    {
        if (NULL == fgets(lineStr, 128, fp))
        {
            continue;
        }

        if (lineStr[0] == '\n')
        {
            continue;
        }

        memset(params,0,sizeof(params));
        memset(cmd,0,sizeof(cmd));

        sscanf(lineStr+strspn(lineStr," \t"),"%[^ =\t]",cmd);
        if (strcmp(cmd, "mode") == 0)
        {
            sscanf(lineStr+strspn(lineStr," \t"),"%*[^=]=%*[\" \t]%[^\" \t\n\r]",params);
            if (!strlen(params))
            {
                ALOGW("Incorrect display mode format: %s, Use default mode", params);
                continue;
            }
            *display_mode = parse_display_mode(params);
            if (*display_mode < 0)
            {
                ALOGW("Incorrect display mode format: %s, Use default mode", params);
                *display_mode = VIV_DISPLAY_MODE_FULL;
            }
            else if (*display_mode < VIV_DISPLAY_MODE_FULL || *display_mode > VIV_DISPLAY_MODE_PANSCAN)
            {
                ALOGW("Not support display mode: %d, Use default mode", *display_mode);
                *display_mode = VIV_DISPLAY_MODE_FULL;
            }
        }
    }
    fclose(fp);
    return 0;
}

int DrmDisplay::adaptWindow(const int width, const int height)
{
    int in_aspect_ratio = -1;
    int buf_aspect_ratio = -1;
    int tmp = -1;
    int buf_width = -1, buf_height = -1;

    buf_width = drm.buffers[0].width;
    buf_height = drm.buffers[0].height;
    buf_aspect_ratio = calc_aspect_ratio(buf_width, buf_height);
    if (buf_aspect_ratio < 0)
    {
        ALOGE("Buffer width:%d heigth:%d", buf_width, buf_height);
        return -1;
    }
    src_roi.left = 0;
    src_roi.top = 0;
    src_roi.right = width;
    src_roi.bottom = height;

    dst_roi.left = 0;
    dst_roi.top = 0;
    dst_roi.right = buf_width;
    dst_roi.bottom = buf_height;

    in_aspect_ratio = calc_aspect_ratio(width, height);
    if (in_aspect_ratio < 0)
    {
        ALOGE("Invalid input width:%d  height:%d", width, height);
        return -1;
    }

    if (mode == VIV_DISPLAY_MODE_FULL)
    {
        return 0;
    }
    else if (mode == VIV_DISPLAY_MODE_LPBOX)
    {
        if (in_aspect_ratio == VIV_DISPLAY_ASPECT_4_3 && buf_aspect_ratio == VIV_DISPLAY_ASPECT_16_9)
        {
            tmp = (buf_height * 4) / 3;
            dst_roi.left = (buf_width - tmp) / 2;
            dst_roi.top = 0;
            dst_roi.right = buf_width - dst_roi.left;
            dst_roi.bottom = buf_height;
        }
        else if (in_aspect_ratio == VIV_DISPLAY_ASPECT_16_9 && buf_aspect_ratio == VIV_DISPLAY_ASPECT_4_3)
        {
            tmp = (buf_width * 9) / 16;
            dst_roi.left = 0;
            dst_roi.top = (buf_height - tmp) / 2;
            dst_roi.right = buf_width;
            dst_roi.bottom = buf_height - dst_roi.top;
        }
    }
    else if (mode == VIV_DISPLAY_MODE_PANSCAN)
    {
        if (in_aspect_ratio == VIV_DISPLAY_ASPECT_4_3 && buf_aspect_ratio == VIV_DISPLAY_ASPECT_16_9)
        {
            tmp = (width * 9) / 16;
            src_roi.left = 0;
            src_roi.top = (height - tmp) / 2;
            src_roi.right = width;
            src_roi.bottom = height - src_roi.top;
        }
        else if (in_aspect_ratio == VIV_DISPLAY_ASPECT_16_9 && buf_aspect_ratio == VIV_DISPLAY_ASPECT_4_3)
        {
            tmp = (height * 4) / 3;
            src_roi.left = (width - tmp) / 2;
            src_roi.top = 0;
            src_roi.right = width - src_roi.left;
            src_roi.bottom = height;
        }
    }
    else
    {
        ALOGE("Invalid Display mode: %d", mode);
        return -1;
    }

    return 0;
}

DrmDisplay::DrmDisplay() {
    open_drm_device(&drm);
    drm_device_prepare(&drm);
    drm.saved_crtc = drmModeGetCrtc(drm.drm_fd, drm.crtc_id);

    mode = VIV_DISPLAY_MODE_FULL;
    setup = false;
    get_display_mode(&mode);

    if (g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
        ALOGE("Failed to open g2d!");
    }
}

DrmDisplay::~DrmDisplay() {
    drmDropMaster(drm.drm_fd);
    close(drm.drm_fd);
    if (drm.modes != NULL)
    {
        free(drm.modes);
        drm.modes = NULL;
    }
    
    if (g2d_handle) {
        g2d_close(g2d_handle);
        g2d_handle = NULL;
    }
}

#ifdef APPMODE_V4L2
static inline bool isBayerFormat(int format) {
    return (format == V4L2_PIX_FMT_SBGGR8 ||
        format == V4L2_PIX_FMT_SGBRG8 ||
        format == V4L2_PIX_FMT_SGRBG8 ||
        format == V4L2_PIX_FMT_SRGGB8 ||
        format == V4L2_PIX_FMT_SBGGR10 ||
        format == V4L2_PIX_FMT_SGBRG10 ||
        format == V4L2_PIX_FMT_SGRBG10 ||
        format == V4L2_PIX_FMT_SRGGB10 ||
        format == V4L2_PIX_FMT_SBGGR12 ||
        format == V4L2_PIX_FMT_SGBRG12 ||
        format == V4L2_PIX_FMT_SGRBG12 ||
        format == V4L2_PIX_FMT_SRGGB12);
}
#endif

void DrmDisplay::showBuffer(unsigned char* data, int width, int height, int format, int size) {
    if (!data || width <=0 || height <= 0) {
        drmModeSetCrtc(drm.drm_fd,
                        drm.saved_crtc->crtc_id,
                        drm.saved_crtc->buffer_id,
                        drm.saved_crtc->x,
                        drm.saved_crtc->y,
                        &drm.conn_id,
                        1,
                        &drm.saved_crtc->mode);
        return;
    }
    struct drm_buffer* buf = &drm.buffers[frameid++%2];
    
    if ((buf->width != (unsigned int)width) || (buf->height != (unsigned int)height))
    {
        drm_adaptive_resolution(&drm,width,height);
    }

#ifndef APPMODE_V4L2
    switch (format) {
        case MEDIA_PIX_FMT_YUV422SP:
            convert422spToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height);
            break;
        case MEDIA_PIX_FMT_YUV422I:
            convertYUYVToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height);
            break;
        case MEDIA_PIX_FMT_YUV420SP:
            convertNV12ToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height);
            break;
        default:
            return;
    }
#else
    switch (format) {
        case V4L2_PIX_FMT_NV16:
            convert422spToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height);
            break;
        case V4L2_PIX_FMT_YUYV:
            convertYUYVToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height);
            break;
        case V4L2_PIX_FMT_NV12:
            convertNV12ToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height);
            break;
        default:
            if (isBayerFormat(format))
                convertRawToBGRX(data, (unsigned char*)buf->fb_base,  width, height, width, height, format);
            else
                return;
    }
#endif
    drmModeSetCrtc(drm.drm_fd, drm.crtc_id, buf->buf_id, 0, 0, &drm.conn_id, 1, &drm.mode);
}

void DrmDisplay::showBufferExt(unsigned char* data, unsigned long buf_addr,
                int width, int height, int format, int size) {
#ifdef IMX_G2D
    /* this means need to reset to original configuration */
    if (!buf_addr || width <= 0 || height <= 0 || format < 0) {
        drmModeSetCrtc(drm.drm_fd,
                        drm.saved_crtc->crtc_id,
                        drm.saved_crtc->buffer_id,
                        drm.saved_crtc->x,
                        drm.saved_crtc->y,
                        &drm.conn_id,
                        1,
                        &drm.saved_crtc->mode);
        setup = false;
        return;
    }

    struct drm_buffer *buf = &drm.buffers[frameid++%2];
    struct g2d_surfaceEx src, dst;


#ifdef APPMODE_V4L2
    if (isBayerFormat(format)) {
        convertRawToBGRX(data, (unsigned char*)buf->fb_base,  width, height,
                buf->width, buf->height, format);
    } else
#endif
    {
        if (g2d_handle == NULL) {
            ALOGE("G2D is not opened");
            return;
        }
        memset(&src, 0, sizeof(src));
        memset(&dst, 0, sizeof(dst));

        if (!setup)
        {
            if (adaptWindow(width, height) < 0)
            {
                return;
            }
            setup = true;
        }

        src.base.left = src_roi.left;
        src.base.top = src_roi.top;
        src.base.right = src_roi.right;
        src.base.bottom = src_roi.bottom;
        src.base.width = src_roi.right - src_roi.left;
        src.base.height = src_roi.bottom - src_roi.top;
        src.base.stride = width;

#ifdef APPMODE_V4L2
        if (format == V4L2_PIX_FMT_NV12) {
#else
        if (format == MEDIA_PIX_FMT_YUV420SP) {
#endif
            src.base.format = G2D_NV12;
            src.base.planes[0] = buf_addr;
            src.base.planes[1] = buf_addr + width * height;
#ifdef APPMODE_V4L2
        } else if (format == V4L2_PIX_FMT_NV16) {
#else
        } else if (format == MEDIA_PIX_FMT_YUV422SP) {
#endif
            src.base.format = G2D_NV16;
            src.base.planes[0] = buf_addr;
            src.base.planes[1] = buf_addr + width * height;
#ifdef APPMODE_V4L2
        } else if (format == V4L2_PIX_FMT_YUYV) {
#else
        } else if (format == MEDIA_PIX_FMT_YUV422I) {
#endif
            src.base.format = G2D_YUYV;
            src.base.planes[0] = buf_addr;
        } else {
            ALOGW("Format (%x) not supported!", format);
            return;
        }
        src.tiling = G2D_AMPHION_TILED;

        dst.base.left = dst_roi.left;
        dst.base.top = dst_roi.top;
        dst.base.right = dst_roi.right;
        dst.base.bottom = dst_roi.bottom;
        dst.base.width = dst_roi.right - dst_roi.left;
        dst.base.height = dst_roi.bottom - dst_roi.top;
        
        dst.base.stride = buf->width;
        dst.base.format = G2D_BGRA8888;
        dst.base.planes[0] = buf->paddr;
        dst.tiling = G2D_LINEAR;

        g2d_blitEx(g2d_handle, &src, &dst);
        g2d_finish(g2d_handle);

    }
    drmModeSetCrtc(drm.drm_fd, drm.crtc_id, buf->buf_id, 0, 0, &drm.conn_id, 1, &drm.mode);
#else
    showBuffer(data, width, height, format, size);
#endif
}
#endif
