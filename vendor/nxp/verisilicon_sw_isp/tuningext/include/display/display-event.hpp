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

#pragma once

#include "IDisplay.h"
#include <oslayer/oslayer.h>
#include <thread>
#include <unordered_map>

class DisplayEvent {
public:
    DisplayEvent(int videoId);
    ~DisplayEvent();

    int videoId;
    bool ispCfg;
    bool running;
    bool captureRAW;
    uint32_t miMode;
    uint32_t layout;
    uint32_t sensorMode;
    uint32_t sensorModeCount;
    osEvent captureDone;
    osEvent previewDone;
    std::string filePre;
    Json::Value jModuledata;
    Json::Value jLoadXml;

private:
    void displayLoop();
    int bufRequestDma(int fd,
                      struct v4l2_requestbuffers &reqBuffers,
                      struct v4l2_buffer &buffer,
                      struct dma_buf_phys buf_addrs[],
                      struct v4l2_exportbuffer &expbuf);
    int bufReleaseDma(int fd, struct v4l2_requestbuffers &reqBuffers);
    int bufRequest(int fd,
                   struct v4l2_requestbuffers &reqBuffers,
                   struct v4l2_buffer &buffer,
                   void *userFrame[]);
    int bufRelease(int fd,
                   struct v4l2_requestbuffers &reqBuffers,
                   struct v4l2_buffer buffer,
                   void *userFrame[]);
    int updateFormatParams(v4l2_format *pFormat,
                           viv_caps_supports capsSupports,
                           std::unordered_map<uint32_t, uint32_t> mediaFmt2V4l2Fmt);
    int updatePixelFormat(uint32_t *pPixelFormat,
                          std::unordered_map<uint32_t, uint32_t> mediaFmt2V4l2Fmt);
    void enumFormat(int fd, std::unordered_map<uint32_t, uint32_t> &mediaFmt2V4l2Fmt);
    void updateModeCount(viv_caps_supports capsSupports);

    IDisplay *pDisplay;
    IDisplay *pCapture;
    std::thread dThread;
};

enum {
    DISPLAY_PIX_FMT_YUV422SP = 0,
    DISPLAY_PIX_FMT_YUV422I,
    DISPLAY_PIX_FMT_YUV420SP,
    DISPLAY_PIX_FMT_RAW10,
    DISPLAY_PIX_FMT_RAW12,
};

enum BufType {
    DISPLAY_BUF_TYPE_DMA = 0,
    DISPLAY_BUF_TYPE_MMAP
};

extern DisplayEvent *pDisplayEvent;
