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

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>

#include <algorithm>

#include "device.h"
#include "log.h"
#include "BufferManager.h"
#include "MediaBuffer.h"
#include "V4l2File.h"

#include "viv_video_kevent.h"
#include "viv_v4l2_ioctl.h"
#include "viv_v4l2_ioctl2.h"

#include "V4l2Event.h"

#define LOGTAG "V4l2Event"

static std::map<int, int> v4l2_format_cov {
    {V4L2_PIX_FMT_YUYV, MEDIA_PIX_FMT_YUV422I},
    {V4L2_PIX_FMT_NV12, MEDIA_PIX_FMT_YUV420SP},
    {V4L2_PIX_FMT_NV16, MEDIA_PIX_FMT_YUV422SP},
    {V4L2_PIX_FMT_SRGGB8, MEDIA_PIX_FMT_RAW8},
    {V4L2_PIX_FMT_SGRBG8, MEDIA_PIX_FMT_RAW8},
    {V4L2_PIX_FMT_SGBRG8, MEDIA_PIX_FMT_RAW8},
    {V4L2_PIX_FMT_SBGGR8, MEDIA_PIX_FMT_RAW8},
    {V4L2_PIX_FMT_SRGGB10, MEDIA_PIX_FMT_RAW10},
    {V4L2_PIX_FMT_SGRBG10, MEDIA_PIX_FMT_RAW10},
    {V4L2_PIX_FMT_SGBRG10, MEDIA_PIX_FMT_RAW10},
    {V4L2_PIX_FMT_SBGGR10, MEDIA_PIX_FMT_RAW10},
    {V4L2_PIX_FMT_SRGGB12, MEDIA_PIX_FMT_RAW12},
    {V4L2_PIX_FMT_SGRBG12, MEDIA_PIX_FMT_RAW12},
    {V4L2_PIX_FMT_SGBRG12, MEDIA_PIX_FMT_RAW12},
    {V4L2_PIX_FMT_SBGGR12, MEDIA_PIX_FMT_RAW12}
};

V4l2Event::V4l2Event(int id, bool bWarmup) {
    devId = id;
    pMediaPip = NULL;
    bRunning = true;
    mThread = std::thread([this]() { eventLoop(); });
#ifndef ENABLE_IRQ
    qbufThread = std::thread([this]() { qbufLoop(); });
#endif
}

V4l2Event::~V4l2Event() {
    bRunning = false;
#ifndef ENABLE_IRQ
    qbufThread.join();
#endif

    if (mThread.joinable()) {
        mThread.join();
    }

    if(pMediaPip) {
        removeListener(pMediaPip);
        pMediaPip->remove();
        delete pMediaPip;
        pMediaPip = NULL;
    }
}

#ifndef ENABLE_IRQ
/* should not block qbuf event at anytime */
void V4l2Event::qbufLoop() {
    EventData data;
    struct v4l2_event_subscription subscribe;
    struct v4l2_event kevent;
    int streamid = -1;
    int eventDevice = openVideoDevice(devId);
    if (eventDevice < 0)
        return;
    ioctl(eventDevice, VIV_VIDIOC_S_STREAMID, &streamid);

    memset(&subscribe, 0, sizeof(subscribe));
    subscribe.type = VIV_VIDEO_EVENT_TYPE;
    subscribe.id = VIV_VIDEO_EVENT_QBUF;
    if (ioctl(eventDevice, VIDIOC_SUBSCRIBE_EVENT, &subscribe) < 0) {
        ALOGE("subscribe failed.");
        return;
    }

    while (bRunning) {
        fd_set fds;
        FD_ZERO (&fds);
        FD_SET (eventDevice, &fds);
        select (eventDevice + 1, &fds, NULL, NULL, NULL);
        if (ioctl(eventDevice, VIDIOC_DQEVENT, &kevent) == 0) {
            struct viv_video_event event;
            memcpy(&event, kevent.u.data, sizeof(event));
            switch (kevent.id) {
            case VIV_VIDEO_EVENT_QBUF: {
                V4l2BufferManager::inst()->push(event.file, event.addr);
                data.type = MEDIA_EVENT_QUEUE_BUFFER;
                data.data = (void*) event.addr;
                data.stream_id = event.stream_id;
                notifyAll(data);
                break;
            }
            }
            if (event.sync) {
                ioctl(eventDevice, VIV_VIDIOC_EVENT_COMPLETE, &event);
            }
        } else {
            ALOGE("%s faile to pool event %s", __func__, strerror(errno));
        }
    }

    if (ioctl(eventDevice, VIDIOC_UNSUBSCRIBE_EVENT, &subscribe) < 0) {
        ALOGE("VIV_VIDEO_EVENT_QBUF unsubscribe failed.");
    }
    closeVideoDevice(&eventDevice);
}
#endif

void V4l2Event::eventLoop() {
    EventData data;
    struct v4l2_event_subscription subscribe;
    struct v4l2_event kevent;
    int streamid = -1;
    bool bWarmup = pMediaPip ? pMediaPip->getWarmupOnOff() : false;
    bool sync;
    int eventDevice = openVideoDevice(devId);
    if (eventDevice < 0) {
        ALOGE("%s line[%d]: open video device failed",__func__, __LINE__);
        return;
    }
    ioctl(eventDevice, VIV_VIDIOC_S_STREAMID, &streamid);

    memset(&subscribe, 0, sizeof(subscribe));
    subscribe.type = VIV_VIDEO_EVENT_TYPE;

    for (int i = VIV_VIDEO_EVENT_MIN+1; i < VIV_VIDEO_EVENT_QBUF; i++) {
        subscribe.id = i;
        if (ioctl(eventDevice, VIDIOC_SUBSCRIBE_EVENT, &subscribe) < 0) {
            ALOGE("subscribe failed.");
            return;
        }
    }

    std::map<uint64_t, void*> bufMap;

    while (bRunning) {
        fd_set fds;
        struct timeval tv = {2, 0};/*2 secs*/
        FD_ZERO (&fds);
        FD_SET (eventDevice, &fds);
        if (select (eventDevice + 1, &fds, NULL, NULL, &tv) <= 0)
            continue;
        data.jsonRequest.clear();
        data.jsonResponse.clear();
        if (ioctl(eventDevice, VIDIOC_DQEVENT, &kevent) == 0) {
            struct viv_video_event event;
            if(!pMediaPip) {
                pMediaPip = new MediaPipeline(devId, bWarmup);
                if(pMediaPip) {
                    if(pMediaPip->init() == false) {
                        bRunning = false;
                        ALOGE("initialize MediaPipeline error!\n");
                        delete pMediaPip;
                        pMediaPip = NULL;
                        goto CLOSE_FD;
                    }
                    registerListener(pMediaPip);
                } else {
                    ALOGE("new MediaPipeline error!\n");
                    goto CLOSE_FD;
                }
            }
            memcpy(&event, kevent.u.data, sizeof(event));
            sync = event.sync;
            switch (kevent.id) {
            case VIV_VIDEO_EVENT_NEW_STREAM :
                data.type = MEDIA_EVENT_STREAM_OPEN;
                data.stream_id = event.stream_id;
                notifyAll(data);
                break;
            case VIV_VIDEO_EVENT_DEL_STREAM :
                data.type = MEDIA_EVENT_STREAM_CLOSE;
                data.stream_id = event.stream_id;
                notifyAll(data);
                break;
            case VIV_VIDEO_EVENT_START_STREAM:
                data.type = MEDIA_EVENT_STREAM_START;
                data.stream_id = event.stream_id;
                notifyAll(data);
                break;
            case VIV_VIDEO_EVENT_STOP_STREAM:
                data.type = MEDIA_EVENT_STREAM_STOP;
                data.stream_id = event.stream_id;
                notifyAll(data);
                break;
            case VIV_VIDEO_EVENT_SET_FMT:
                data.type = MEDIA_EVENT_STREAM_SET_FMT;
                data.stream_id = event.stream_id;
                data.res.width = event.addr;
                data.res.height = event.response;
                data.res.format = v4l2_format_cov[event.buf_index];
                notifyAll(data);
                break;
            case VIV_VIDEO_EVENT_SET_CROP: {
                if (bufMap.find(event.addr) == bufMap.end()) {
                    bufMap[event.addr] = ::mmap(NULL, VIV_JSON_BUFFER_SIZE,
                        PROT_READ | PROT_WRITE, MAP_SHARED, eventDevice, event.addr);
                }
                struct viv_rect *rect = (struct viv_rect *)bufMap[event.addr];
                ALOGI("Crop: left=%d,top=%d,width=%d,height=%d\n",rect->left,
                        rect->top,rect->width,rect->height);

                data.type = MEDIA_EVENT_SET_CROP;
                data.stream_id = event.stream_id;
                data.ctrlId = ISPCORE_MODULE_DEVICE_SET_CROP;
                data.jsonRequest["crop"]["start_x"] = rect->left;
                data.jsonRequest["crop"]["start_y"] = rect->top;
                data.jsonRequest["crop"]["width"]   = rect->width;
                data.jsonRequest["crop"]["height"]  = rect->height;
                notifyAll(data);
                break;
            }
            case VIV_VIDEO_EVENT_SET_COMPOSE: {
                if (bufMap.find(event.addr) == bufMap.end()) {
                    bufMap[event.addr] = ::mmap(NULL, VIV_JSON_BUFFER_SIZE,
                        PROT_READ | PROT_WRITE, MAP_SHARED, eventDevice, event.addr);
                }
                struct viv_rect *rect = (struct viv_rect *)bufMap[event.addr];
                ALOGI("Compose: left=%d,top=%d,width=%d,height=%d\n",rect->left,
                        rect->top,rect->width,rect->height);

                data.type = MEDIA_EVENT_SET_SCALE;
                data.stream_id = event.stream_id;
                data.ctrlId = ISPCORE_MODULE_DEVICE_SET_SCALE;
                data.jsonRequest["scale"]["start_x"] = rect->left;
                data.jsonRequest["scale"]["start_y"] = rect->top;
                data.jsonRequest["scale"]["width"]   = rect->width;
                data.jsonRequest["scale"]["height"]  = rect->height;
                notifyAll(data);
                break;
            }
            case VIV_VIDEO_EVENT_PASS_JSON: {
                data.type = MEDIA_EVENT_PASS_JSON;
                data.stream_id = event.stream_id;
                data.ctrlId = event.buf_index;
                if (bufMap.find(event.addr) == bufMap.end()) {
                    bufMap[event.addr] = ::mmap(NULL, VIV_JSON_BUFFER_SIZE,
                        PROT_READ | PROT_WRITE, MAP_SHARED, eventDevice, event.addr);
                }
                if (bufMap.find(event.response) == bufMap.end()) {
                    bufMap[event.response] = ::mmap(NULL, VIV_JSON_BUFFER_SIZE,
                        PROT_READ | PROT_WRITE, MAP_SHARED, eventDevice, event.addr);
                }
                Json::Reader reader;
                std::string str = (char *)bufMap[event.addr];
                ALOGI("get json request: 0x%x %s", data.ctrlId, (char *)bufMap[event.addr]);
                reader.parse(str, data.jsonRequest, true);
                notifyAll(data);
                strcpy((char *)bufMap[event.response], data.jsonResponse.toStyledString().c_str());
                break;
            }
            case VIV_VIDEO_EVENT_EXTCTRL: {
                if (bufMap.find(event.addr) == bufMap.end()) {
                    bufMap[event.addr] = ::mmap(NULL, VIV_JSON_BUFFER_SIZE,
                        PROT_READ | PROT_WRITE, MAP_SHARED, eventDevice, event.addr);
                }
                Json::Reader reader;
                std::string str = (char *)bufMap[event.addr];
                std::string::size_type startpos = 0;
                while (startpos!= std::string::npos) {
                    if ((startpos = str.find(';')) != std::string::npos)
                        str.replace(startpos, 1, ",");
                }
                startpos = 0;
                while (startpos!= std::string::npos) {
                    if ((startpos = str.find('<')) != std::string::npos)
                        str.replace(startpos, 1, "\"");
                }
                startpos = 0;
                while (startpos!= std::string::npos) {
                    if ((startpos = str.find('>')) != std::string::npos)
                        str.replace(startpos, 1, "\"");
                }

                ALOGI("get json request: %s", str.c_str());
                reader.parse(str, data.jsonRequest, true);

                data.type = MEDIA_EVENT_PASS_JSON;

                if (!data.jsonRequest["streamid"].isObject())
                    data.jsonRequest["streamid"] = 0;

                Json::Value id = data.jsonRequest["id"];
                if (id.isString()) {
                    std::string strCid = id.asCString();
                    if (v4l2_cid_map.find(strCid) != v4l2_cid_map.end()) {
                        data.ctrlId = v4l2_cid_map[strCid];
                        notifyAll(data);
                    } else
                        data.jsonResponse["result"] = -1;
                } else
                    data.jsonResponse["result"] = -1;
                strcpy((char *)bufMap[event.addr], data.jsonResponse.toStyledString().c_str());
                ALOGI("send json response: %s", data.jsonResponse.toStyledString().c_str());
                break;
            }
            case VIV_VIDEO_EVENT_QUERYCAPS: {
                // data.type = MEDIA_EVENT_PASS_JSON;
                // data.stream_id = event.stream_id;
                // data.jsonRequest["streamid"] = 0;
                // data.jsonRequest["index"] = event.buf_index;
                // data.ctrlId = ISPCORE_MODULE_SENSOR_QUERY;
                // notifyAll(data);
                // ALOGW("get modeinfo:\n   %s", data.jsonResponse.toStyledString().c_str());
                // struct vvcam_constant_modeinfo modeinfo;
                // uint32_t current = data.jsonResponse["current"].asUInt();
                // for (auto& item : data.jsonResponse["caps"]) {
                //     /* We presently just configure the current sensor resolution
                //      * information into the driver and will be got enumerated by
                //      * the users since we cannot support resolution switching on
                //      * the fly yet.
                //      */
                //     if (current == item["index"].asUInt()) {
                //         modeinfo.index = item["index"].asUInt();
                //         modeinfo.w = item["width"].asUInt();
                //         modeinfo.h = item["height"].asUInt();
                //         modeinfo.fps = item["fps"].asUInt();
                //         modeinfo.brpat = item["bayer_pattern"].asUInt();
                //         modeinfo.bitw = item["bit_width"].asUInt();
                //         ioctl(eventDevice, VIV_VIDIOC_S_MODEINFO, &modeinfo);
                //         break;
                //     }
                // }
                break;
            }
            case VIV_VIDEO_EVENT_EXTCTRL2: {
                if (bufMap.find(event.addr) == bufMap.end()) {
                    bufMap[event.addr] = ::mmap(NULL, VIV_JSON_BUFFER_SIZE,
                        PROT_READ | PROT_WRITE, MAP_SHARED, eventDevice, event.addr);
                }
                struct v4l2_ctrl_data *pData =
                        (struct v4l2_ctrl_data *)bufMap[event.addr];
                std::map<int, struct vvctrl> *pCIdMap;
                if (!pData)
                    break;
                if (pData->ctrls.count != 1) {
                    ALOGE("not supported currently!");
                    pData->ret = -1;
                    break;
                }
                if (pData->dir == V4L2_CTRL_GET)
                    pCIdMap = &v4l2_cid_map_get;
                else
                    pCIdMap = &v4l2_cid_map_set;

                struct v4l2_ext_control *pCtrl =
                        nextof(pData, struct v4l2_ext_control *);
                if (pCIdMap->find(pCtrl->id) ==  pCIdMap->end()) {
                    pData->ret = -1;
                    break;
                }
                struct vvctrl &ctrl = pCIdMap->at(pCtrl->id);

                if (ctrl.supported && ctrl.begin)
                    ctrl.begin(pData, data);
                else {
                    pData->ret = -1;
                    break;
                }

                data.type = MEDIA_EVENT_PASS_JSON;
                data.ctrlId = ctrl.ctrlId;
                notifyAll(data);
                if (ctrl.end)
                    ctrl.end(pData, data);
                ALOGW("extctrl2: cid=0x%x,ret=%d", pCtrl->id, pData->ret);
                break;
            }
            case VIV_VIDEO_EVENT_SET_CAPSMODE:{
                struct viv_caps_mode_s caps_mode;
                memset(&caps_mode,0,sizeof(caps_mode));
                ioctl(eventDevice, VIV_VIDIOC_G_CAPS_MODE, &caps_mode);

                data.type = MEDIA_EVENT_PASS_JSON;
                data.stream_id = event.stream_id;
                data.jsonRequest["streamid"] = 0;
                data.jsonRequest["index"] = event.buf_index;
                data.jsonRequest["mode"] = caps_mode.mode;
                data.jsonRequest["CalibXmlName"] = caps_mode.CalibXmlName;
                data.ctrlId = ISPCORE_MODULE_SENSOR_MODE_SET;
                notifyAll(data);

                data.type = MEDIA_EVENT_PASS_JSON;
                data.stream_id = event.stream_id;
                data.jsonRequest["streamid"] = 0;
                data.jsonRequest["dwe"]["mode"] = caps_mode.mode;
                data.ctrlId = VIV_V4L_DWE_SET_MODE;
                notifyAll(data);

                int ret = data.jsonResponse["result"].asInt();
                ioctl(eventDevice, VIV_VIDIOC_EVENT_RESULT, &ret);
                break;
            }
            case VIV_VIDEO_EVENT_GET_CAPS_SUPPORTS:{
                data.type = MEDIA_EVENT_PASS_JSON;
                data.stream_id = event.stream_id;
                data.jsonRequest["streamid"] = 0;
                data.jsonRequest["index"] = event.buf_index;
                data.ctrlId = ISPCORE_MODULE_SENSOR_QUERY;
                notifyAll(data);

                struct viv_caps_supports caps_supports;
                memset(&caps_supports,0,sizeof(caps_supports));

                for (auto& item : data.jsonResponse["caps"]) {
                   caps_supports.count++;
                   caps_supports.mode[caps_supports.count-1].index = item["index"].asUInt();
                   caps_supports.mode[caps_supports.count-1].bounds_width = item["size"]["bounds_width"].asUInt();
                   caps_supports.mode[caps_supports.count-1].bounds_height = item["size"]["bounds_height"].asUInt();
                   caps_supports.mode[caps_supports.count-1].top = item["size"]["top"].asUInt();
                   caps_supports.mode[caps_supports.count-1].left = item["size"]["left"].asUInt();
                   caps_supports.mode[caps_supports.count-1].width = item["size"]["width"].asUInt();
                   caps_supports.mode[caps_supports.count-1].height = item["size"]["height"].asUInt();
                   caps_supports.mode[caps_supports.count-1].hdr_mode = item["hdr_mode"].asUInt();
                   caps_supports.mode[caps_supports.count-1].stitching_mode = item["stitching_mode"].asUInt();
                   caps_supports.mode[caps_supports.count-1].bit_width = item["bit_width"].asUInt();
                   caps_supports.mode[caps_supports.count-1].bayer_pattern = item["bayer_pattern"].asUInt();
                   caps_supports.mode[caps_supports.count-1].fps = item["fps"].asUInt();
                }

                ioctl(eventDevice, VIV_VIDIOC_SET_CAPS_SUPPORTS, &caps_supports);

                break;
            }
            }

            if(kevent.id == VIV_VIDEO_EVENT_DEL_STREAM) {
                if(pMediaPip && bRunning) {
                    removeListener(pMediaPip);
                    delete pMediaPip;
                    pMediaPip = NULL;
                }
            }

            if (sync) {
                ioctl(eventDevice, VIV_VIDIOC_EVENT_COMPLETE, &event);
            }

        } else {
            ALOGE("%s faile to pool event %s", __func__, strerror(errno));
        }
    }
CLOSE_FD:
    for (int i = VIV_VIDEO_EVENT_MIN + 1; i < VIV_VIDEO_EVENT_QBUF; i++) {
        subscribe.id = i;
        if (ioctl(eventDevice, VIDIOC_UNSUBSCRIBE_EVENT, &subscribe) < 0) {
            ALOGE("unsubscribe %d failed.", subscribe.id);
        }
    }
    closeVideoDevice(&eventDevice);
}
