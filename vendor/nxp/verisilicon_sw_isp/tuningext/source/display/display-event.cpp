/******************************************************************************\
|* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

#include "ioctl/v4l2-ioctl.hpp"
#include "display/display-event.hpp"
#include "calibration/keys.hpp"
#include "shell/shell.hpp"
#include <cameric_drv/cameric_drv_api.h>
#include <cameric_drv/cameric_mi_drv_api.h>

#include <poll.h>
#include <imx/linux/dma-buf.h>

#define FRAME_CACHE_COUNT 4

DisplayEvent *pDisplayEvent;

int DisplayEvent::bufRequestDma(int fd,
                                struct v4l2_requestbuffers &reqBuffers,
                                struct v4l2_buffer &buffer,
                                struct dma_buf_phys buf_addrs[],
                                struct v4l2_exportbuffer &expbuf) {
    int ret = 0;
    reqBuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBuffers.memory = V4L2_MEMORY_MMAP;
    reqBuffers.count  = FRAME_CACHE_COUNT;

    ret = ioctl(fd, VIDIOC_REQBUFS, &reqBuffers);
    if (ret) {
        ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    for (int i = 0; i < FRAME_CACHE_COUNT; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory   = V4L2_MEMORY_MMAP;
        buffer.index    = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buffer);
        if (ret) {
            ALOGE("VIDIOC_QUERYBUF failed: %s", strerror(errno));
            return ret;
        }

        ret = ioctl(fd, VIDIOC_QBUF, &buffer);
        if (ret) {
            ALOGE("VIDIOC_QBUF failed: %s", strerror(errno));
            return ret;
        }

        expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        expbuf.index = i;
        ret = ioctl(fd, VIDIOC_EXPBUF, &expbuf);
        if (ret) {
            ALOGE("VIDIOC_EXPBUF failed: %s", strerror(errno));
            return ret;
        }

        ret = ioctl(expbuf.fd, DMA_BUF_IOCTL_PHYS, &buf_addrs[i]);
        if (ret) {
            ALOGE("DMA_BUF_IOCTL_PHYS failed: %s", strerror(errno));
            return ret;
        }
    }

    return ret;
}

int DisplayEvent::bufReleaseDma(int fd, struct v4l2_requestbuffers &reqBuffers) {
    int ret = 0;
    reqBuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBuffers.memory = V4L2_MEMORY_MMAP;
    reqBuffers.count  = 0;

    ret = ioctl(fd, VIDIOC_REQBUFS, &reqBuffers);
    if (ret) {
        ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
    }

    return ret;
}

int DisplayEvent::bufRequest(int fd,
                            struct v4l2_requestbuffers &reqBuffers,
                            struct v4l2_buffer &buffer,
                            void *userFrame[]) {
    int ret = 0;
    reqBuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBuffers.memory = V4L2_MEMORY_MMAP;
    reqBuffers.count  = FRAME_CACHE_COUNT;

    ret = ioctl(fd, VIDIOC_REQBUFS, &reqBuffers);
    if (ret) {
        ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    for (int i = 0; i < FRAME_CACHE_COUNT; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory   = V4L2_MEMORY_MMAP;
        buffer.index    = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buffer);
        if (ret) {
            ALOGE("VIDIOC_QUERYBUF failed: %s", strerror(errno));
            return ret;
        }

        userFrame[i] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, buffer.m.offset);
        if (userFrame[i] == MAP_FAILED) {
            ALOGE("mmap failed: %s", strerror(errno));
            return ret;
        }

        ret = ioctl(fd, VIDIOC_QBUF, &buffer);
        if (ret) {
            ALOGE("VIDIOC_QBUF failed: %s", strerror(errno));
            return ret;
        }
    }

    return ret;
}

int DisplayEvent::bufRelease(int fd,
                            struct v4l2_requestbuffers &reqBuffers,
                            struct v4l2_buffer buffer,
                            void *userFrame[]) {
    int ret = 0;
    reqBuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBuffers.memory = V4L2_MEMORY_MMAP;
    reqBuffers.count  = 0;

    ret = ioctl(fd, VIDIOC_REQBUFS, &reqBuffers);
    if (ret) {
        ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    for (int i = 0; i < FRAME_CACHE_COUNT; i++) {
        munmap(userFrame[i], buffer.length);
    }

    return ret;
}

void DisplayEvent::updateModeCount(viv_caps_supports capsSupports) {
    sensorModeCount = capsSupports.count;
    ALOGI("caps supports:{");
    ALOGI("\tcount = %d",capsSupports.count);
    for(unsigned int i=0; i<capsSupports.count; i++) {
        ALOGI("\t{");
        ALOGI("\t\tindex            = %d",capsSupports.mode[i].index);
        ALOGI("\t\tbounds_width     = %d",capsSupports.mode[i].bounds_width);
        ALOGI("\t\tbounds_height    = %d",capsSupports.mode[i].bounds_height);
        ALOGI("\t\ttop              = %d",capsSupports.mode[i].top);
        ALOGI("\t\tleft             = %d",capsSupports.mode[i].left);
        ALOGI("\t\twidth            = %d",capsSupports.mode[i].width);
        ALOGI("\t\theight           = %d",capsSupports.mode[i].height);
        ALOGI("\t\thdr_mode         = %d",capsSupports.mode[i].hdr_mode);
        ALOGI("\t\tstitching_mode   = %d",capsSupports.mode[i].stitching_mode);
        ALOGI("\t\tbit_width        = %d",capsSupports.mode[i].bit_width);
        ALOGI("\t\tbayer_pattern    = %d",capsSupports.mode[i].bayer_pattern);
        ALOGI("\t\tfps              = %d",capsSupports.mode[i].fps);
        ALOGI("\t}");
    }
    ALOGI("}");
    return ;
}

void DisplayEvent::enumFormat(int fd,
                              std::unordered_map<uint32_t, uint32_t> &mediaFmt2V4l2Fmt) {
    int ret;
    struct v4l2_fmtdesc formatDescriptions;

    formatDescriptions.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (int i=0; true; i++) {
        formatDescriptions.index = i;
        ret = ioctl(fd, VIDIOC_ENUM_FMT, &formatDescriptions);
        if (ret) {
            break;
        }
        
        ALOGI("  %2d: %s 0x%08X 0x%X",
                i,
                formatDescriptions.description,
                formatDescriptions.pixelformat,
                formatDescriptions.flags
        );

        switch(formatDescriptions.pixelformat)
        {
            case V4L2_PIX_FMT_SBGGR10:
            case V4L2_PIX_FMT_SGBRG10:
            case V4L2_PIX_FMT_SGRBG10:
            case V4L2_PIX_FMT_SRGGB10:
                mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_RAW10] = formatDescriptions.pixelformat;
                break;
            case V4L2_PIX_FMT_SBGGR12:
            case V4L2_PIX_FMT_SGBRG12:
            case V4L2_PIX_FMT_SGRBG12:
            case V4L2_PIX_FMT_SRGGB12:
                mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_RAW12] = formatDescriptions.pixelformat;
                break;
        }
    }

    mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_YUV422I] = V4L2_PIX_FMT_YUYV;
    mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_YUV422SP] = V4L2_PIX_FMT_NV16;
    mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_YUV420SP] = V4L2_PIX_FMT_NV12;

    return;
}

int DisplayEvent::updatePixelFormat(uint32_t *pPixelFormat,
                                    std::unordered_map<uint32_t, uint32_t> mediaFmt2V4l2Fmt) {
    int ret = 0;

    // only support YUV422I/422SP/YUV420SP/RAW10/RAW12
    switch (miMode) {
    case CAMERIC_MI_DATAMODE_YUV422:
        if (layout == (uint32_t)CAMERIC_MI_DATASTORAGE_INTERLEAVED) {
            *pPixelFormat = mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_YUV422I];
        } else if (layout == (uint32_t)CAMERIC_MI_DATASTORAGE_SEMIPLANAR) {
            *pPixelFormat = mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_YUV422SP];
        } else {
            ret = -1;
            ALOGE("YUV422 unsupported layout %d\n", layout);
        }
        break;
    case CAMERIC_MI_DATAMODE_YUV420:
        if (layout == (uint32_t)CAMERIC_MI_DATASTORAGE_SEMIPLANAR) {
            *pPixelFormat = mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_YUV420SP];
        } else {
            ret = -1;
            ALOGE("YUV420 unsupported layout %d\n", layout);
        }
        break;
    case CAMERIC_MI_DATAMODE_RAW10:
        if (mediaFmt2V4l2Fmt.find(DISPLAY_PIX_FMT_RAW10) == mediaFmt2V4l2Fmt.end()) {
            ALOGE("unsupported format: RAW10\n");
            ret = -1;
        } else {
            *pPixelFormat = mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_RAW10];
        }
        break;
    case CAMERIC_MI_DATAMODE_RAW12:
        if (mediaFmt2V4l2Fmt.find(DISPLAY_PIX_FMT_RAW12) == mediaFmt2V4l2Fmt.end()) {
            ALOGE("unsupported format: RAW12\n");
            ret = -1;
        } else {
            *pPixelFormat = mediaFmt2V4l2Fmt[DISPLAY_PIX_FMT_RAW12];
        }
        break;
    default:
        ret = -1;
        ALOGE("unsupported mi mode %d\n", miMode);
        break;
    }

    return ret;
}

int DisplayEvent::updateFormatParams(v4l2_format *pFormat,
                                     viv_caps_supports capsSupports,
                                     std::unordered_map<uint32_t, uint32_t> mediaFmt2V4l2Fmt) {
    int ret = 0;

    pFormat->type           = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    pFormat->fmt.pix.width  = capsSupports.mode[sensorMode].width;
    pFormat->fmt.pix.height = capsSupports.mode[sensorMode].height;

    ret = updatePixelFormat(&pFormat->fmt.pix.pixelformat, mediaFmt2V4l2Fmt);

    return ret;
    
}

DisplayEvent::DisplayEvent(int videoId) {
    this->videoId = videoId;
    pDisplay = NULL;
    pCapture = NULL;
    ispCfg = false;
    running = false;
    captureRAW = false;
    jLoadXml.clear();
    sensorMode = 0;
    sensorModeCount = 4;
    miMode = CAMERIC_MI_DATAMODE_YUV422;
    layout = CAMERIC_MI_DATASTORAGE_SEMIPLANAR;
    dThread  = std::thread([this]() { displayLoop(); });
}

DisplayEvent::~DisplayEvent() {
    if (pDisplay) {
        delete pDisplay;
    }
    if (pCapture) {
        delete pCapture;
    }
    dThread.join();
}

void DisplayEvent::displayLoop() {
    int ret = RET_SUCCESS;
    int captureCnt = 0;
    BufType bufType = DISPLAY_BUF_TYPE_DMA;
    std::unordered_map<uint32_t, uint32_t> mediaFmt2V4l2Fmt;

    pDisplay = IDisplay::createObject(VIV_DISPLAY_TYPE_DRM);
    if (pDisplay == NULL) {
        ALOGE("Create display type drm failed");
        return;
    }

    pCapture = IDisplay::createObject(VIV_DISPLAY_TYPE_FILE);
    if (pCapture == NULL) {
        ALOGE("Create display type file failed");
        return;
    }

    if (osEventInit(&captureDone, 1, 0) != OSLAYER_OK) {
        ALOGE("captureDone event init failed");
        return;
    }

    if (osEventInit(&previewDone, 1, 0) != OSLAYER_OK) {
        ALOGE("previewDone event init failed");
        return;
    }

    struct v4l2_format format;
    struct v4l2_buffer buffer;
    struct v4l2_requestbuffers reqBuffers;
    memset(&buffer, 0, sizeof(buffer));
    memset(&format, 0, sizeof(format));
    memset(&reqBuffers, 0, sizeof(reqBuffers));
    
    struct dma_buf_phys buf_addrs[FRAME_CACHE_COUNT];
    struct v4l2_exportbuffer expbuf;
    memset(buf_addrs, 0, sizeof(buf_addrs));
    memset(&expbuf, 0, sizeof(expbuf));

    void *userFrame[FRAME_CACHE_COUNT] = {NULL};

    bool prevStop = true;
    int type      = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    while (1) {
        if (!running) {
            if (!prevStop) {
                ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
                if (ret) {
                    ALOGE("VIDIOC_STREAMOFF failed: %s", strerror(errno));
                    goto err;
                }

                if (bufType == DISPLAY_BUF_TYPE_DMA) {
                    ret = bufReleaseDma(fd, reqBuffers);
                } else {
                    ret = bufRelease(fd, reqBuffers, buffer, userFrame);
                    captureCnt = 0;
                    captureRAW = false;
                    filePre.clear();
                }
                if (ret) {
                    ALOGE("bufRelease failed: %s, captureRAW: %d",
                        strerror(errno), captureRAW);
                    goto err;
                }

                close(fd);
                prevStop = true;
                jLoadXml.clear();
                mediaFmt2V4l2Fmt.clear();
                osEventSignal(&previewDone);
                usleep(100000);
            }

            pDisplay->showBufferExt(NULL, 0, -1, -1, -1, -1);
            continue;
        }

        if (prevStop) {
            char szFile[256] = {0};
            sprintf(szFile, "/dev/video%d", videoId);
            fd = open(szFile, O_RDWR | O_NONBLOCK);
            if (fd < 0) {
                ALOGE("can't open video file %s", szFile);
                return;
            }

            v4l2_capability caps;
            memset(&caps, 0, sizeof(caps));
            ret = ioctl(fd, VIDIOC_QUERYCAP, &caps);
            if (ret  < 0) {
                ALOGE("get device caps failed: %s", strerror(errno));
                goto err;
            }

            ALOGI("Open Device: (fd=%d)", fd);
            ALOGI("  Driver: %s", caps.driver);
            if (strcmp((const char*)caps.driver, "viv_v4l2_device")) {
                ALOGE("Open wrong type of viv video dev");
                goto err;
            }

            struct viv_caps_supports capsSupports;
            memset(&capsSupports, 0, sizeof(capsSupports));
            ret = ioctl(fd, VIV_VIDIOC_GET_CAPS_SUPPORTS, &capsSupports);
            if (ret  < 0) {
                ALOGE("get caps support failed: %s", strerror(errno));
                goto err;
            }

            struct viv_caps_mode_s caps_mode;
            memset(&caps_mode, 0, sizeof(caps_mode));
            caps_mode.mode = sensorMode;
            ret = ioctl(fd, VIV_VIDIOC_S_CAPS_MODE, &caps_mode);
            if (ret) {
                ALOGE("VIV_VIDIOC_S_CAPS_MODE %d failed: %s",
                    sensorMode, strerror(errno));
                goto err;
            }

            updateModeCount(capsSupports);
            enumFormat(fd, mediaFmt2V4l2Fmt);
            ret = updateFormatParams(&format, capsSupports, mediaFmt2V4l2Fmt);
            if (ret) {
                ALOGE("updateFormatParams failed");
                goto err;
            }

            if (jLoadXml.isMember(KEY_CALIB_FILE)) {
                Json::Value jQuery, jResponse;
                jQuery = jLoadXml;
                ret = viv_private_ioctl(IF_CALIBRATION_SET, jQuery, jResponse);
                if (ret | jResponse[REST_RET].asInt()) {
                    ALOGE("load xml failed: %s", strerror(errno));
                    goto err;
                }
                jLoadXml.clear();
            }

            if (ispCfg) {
                Json::Value jQuery, jResponse;
                jQuery = jModuledata;
                jQuery[KEY_CONFIG] = ispCfg;

                ret = viv_private_ioctl(IF_MODULE_DATA_PARSE, jQuery, jResponse);
                if (ret | jResponse[REST_RET].asInt()) {
                    ALOGE("load isp config failed: %s", strerror(errno));
                    goto err;
                }
                ispCfg = false;
            }

            ret = ioctl(fd, VIDIOC_S_FMT, &format);
            if (ret) {
                ALOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
                goto err;
            }

            if (!captureRAW) {
                ret = bufRequestDma(fd, reqBuffers, buffer, buf_addrs, expbuf);
                bufType = DISPLAY_BUF_TYPE_DMA;
            } else {
                ret = bufRequest(fd, reqBuffers, buffer, userFrame);
                captureCnt = 1;// for debug capture initial n frames
                bufType = DISPLAY_BUF_TYPE_MMAP;
            }
            if (ret) {
                ALOGE("bufRequest failed: %s, captureRAW:%d",
                    strerror(errno), captureRAW);
                goto err;
            }

            ret = ioctl(fd, VIDIOC_STREAMON, &type);
            if (ret) {
                ALOGE("VIDIOC_STREAMON failed: %s", strerror(errno));
                goto err;
            }

            prevStop = false;
            osEventSignal(&previewDone);
            usleep(100000);
        }

        struct pollfd pollfds;
        pollfds.events = POLLIN | POLLPRI;
        pollfds.fd = fd;
        ret = poll(&pollfds, 1, 10);
        if (ret <= 0) {
            continue;
        }

        ret = ioctl(fd, VIDIOC_DQBUF, &buffer);
        if (ret) {
            ALOGE("VIDIOC_DQBUF failed: %s", strerror(errno));
            goto err;
        }

        ALOGI("Get Frame buf %d\n", buffer.index);
        if (!captureRAW) {
            pDisplay->showBufferExt(NULL,
                buf_addrs[buffer.index].phys,
                format.fmt.pix.width, format.fmt.pix.height,
                format.fmt.pix.pixelformat, buffer.length);
        } else {
            std::string fileName = filePre;
            if (captureCnt > 1) {
                fileName = fileName + "_" + std::to_string(captureCnt);
            }
            pCapture->showBuffer((unsigned char*)userFrame[buffer.index],
                format.fmt.pix.width, format.fmt.pix.height,
                format.fmt.pix.pixelformat, buffer.length,
                fileName);
            captureCnt--;
            if (captureCnt == 0) {
                osEventSignal(&captureDone);
            }
        }

        ret = ioctl(fd, VIDIOC_QBUF, &buffer);
        if (ret) {
            ALOGE("VIDIOC_QBUF failed: %s", strerror(errno));
            goto err;
        }
    }

err:
    close(fd);
    return;
}
