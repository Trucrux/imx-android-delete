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

#include "MediaPipeline.h"
#include <algorithm>
#include <IMemoryAllocator.h>

#include "device.h"
#include "log.h"
#include "viv_video_kevent.h"

#define LOGTAG "MediaPipeline"

MediaPipeline::MediaPipeline(int id, bool warmup) {
#ifdef HAL_CMODEL
    IMemoryAllocator::create(ALLOCATOR_TYPE_USER_PTR);
#elif defined USE_V4L2
    IMemoryAllocator::create(ALLOCATOR_TYPE_V4L2);
#else
    IMemoryAllocator::create(ALLOCATOR_TYPE_V4L2);
#endif
    devId = id;
    bWarmup = warmup;
}

MediaPipeline::~MediaPipeline() {
    for (auto& item : mapModules) {
        if (item.second) {
            ALOGI(" %s close", item.second->getClassName());
            delete item.second;
        }
    }
    mapStreams.clear();
    mapModules.clear();
}

bool MediaPipeline::start(int id) {
    auto stream = mapStreams[id];
    // start module by reverse order.
    if(bWarmup == true) {
        for_each(stream.rbegin(), stream.rend(), [](const StreamNode& node) { node.prevModule->start(true); } );
    } else {
        for_each(stream.rbegin(), stream.rend(), [](const StreamNode& node) { node.prevModule->start(false); } );
    }

    return true;
}

bool MediaPipeline::stop(int id) {
    auto stream = mapStreams[id];
    for (auto& node : stream) {
        ALOGI(" %s stop",  node.prevModule->getClassName());
        node.prevModule->stop();
    }
    return true;
}

/*
    At first, we try to change the format at stream endpoint.
    but some components such as V4l2Control or Dewarp can not handle diff output resolution with input,
    so use backward pass to find prev module and set format.
    ISP ONLY: set directly.
    ISP -> DWE: set dwe,  set isp.
    ISP -> VSE: set vse only.
    also need change all remote pads format which linking at this pad.
    e.g.
    stream0  ISP: 0 -> DWE: 0       --here change another res.
    stream1  ISP: 0 -> VSE: 0
    because we changed ISP0 and DWE0 res in stream0, so need change VSE input res in stream1 too.
    NOTICE: input format may diff with output format.
*/
bool MediaPipeline::setFormat(int id, MediaCap& res) {
    ALOGI("set format:%d   res: %d %d %d", id, res.width, res.height, res.format);

    auto stream = mapStreams[id];
    for (unsigned int i = 0; i< stream.size(); i++){
        auto& node = stream[i];
        MediaCap InputResolution;
        MediaCap OutputResolution;

        if(strcmp(node.prevModule->getClassName(),"NativeSensor") == 0) {
            OutputResolution.format = res.format;
            node.prevModule->setFormat(node.prePort,OutputResolution,PAD_OUTPUT);
            OutputResolution = node.prevModule->getFormat(node.prePort,PAD_OUTPUT);
            InputResolution = OutputResolution;
            node.pModule->setFormat(node.nPort,InputResolution,PAD_INPUT);
        } else {
            if(strcmp(node.prevModule->getClassName(),"NativeDewarp") == 0) {
                OutputResolution = res;
                node.prevModule->setFormat(node.prePort,OutputResolution,PAD_OUTPUT);
            }
            OutputResolution = node.prevModule->getFormat(node.prePort,PAD_OUTPUT);
            InputResolution = OutputResolution;
            node.pModule->setFormat(node.nPort,InputResolution,PAD_INPUT);
        }
    }

    return true;
}

/******************************************************************************
 *  Each linking pads must have the same resolution
 *****************************************************************************/
void MediaPipeline::resetAllStreamsFormat() {

    for (auto& stream : mapStreams) {
        for (auto& node : stream.second) {
            auto prevRes = node.prevModule->getFormat(node.prePort, PadTypeSrc);
            node.pModule->setFormat(node.nPort, prevRes, PadTypeSink);
        }
        stream.second.back().prevModule->setEndpoint(stream.second.back().prePort, true);
    }
}

bool MediaPipeline::findNearestFormat(int id, MediaCap& res, MediaCap& lres) {
    std::vector<MediaCap> v;
    auto& caps = mStreamsCapability[id];
    for (auto& cap : caps) {
        if (cap.format == res.format) {
            v.push_back(cap);
        }
    }
    // if can't find the format, return false
    if (v.empty()) {
        ALOGE("stream %d don't support format:%d", id, res.format);
        return false;
    }

    typedef struct ResDistance {
        int width;
        int height;
        int distance;
        bool operator<(const ResDistance& rd) const { return distance < rd.distance; }
    } ResDistance;
    std::vector<ResDistance> rds;
    // find nearest resolution
    for (auto& cap : v) {
        int dw = res.width - cap.width;
        int dh = res.height - cap.height;
        ResDistance rd = {cap.width, cap.height, dw*dw+dh*dh};
        rds.push_back(rd);
    }
    sort(rds.begin(), rds.end());
    lres.width = rds[0].width;
    lres.height = rds[0].height;
    lres.format = res.format;
    return true;
}

bool MediaPipeline::open(int id) {
    auto stream = mapStreams[id];
    for (auto& node : stream) {
        ALOGI("stream%d connect %s:%d ---> %s:%d", id, node.prevModule->getClassName(),
                node.prePort, node.pModule->getClassName(), node.nPort);
        if (!node.prevModule->connect(node.pModule, node.nPort , node.prePort)) {
            ALOGE("connect failed!!");
            return false;
        }
    }
    return true;
}

bool MediaPipeline::close(int id) {
    auto stream = mapStreams[id];
    for (auto& node : stream) {
        ALOGI("stream%d disconnect %s:%d -X-> %s:%d", id, node.prevModule->getClassName(),
                node.prePort, node.pModule->getClassName(), node.nPort);
        node.prevModule->disconnect(node.pModule, node.nPort, node.prePort);
        // node.prevModule->close();
    }
    return true;
}

bool MediaPipeline::setStringParam(int id, const char* key, int userVal) {
    auto stream = mapStreams[id];
    ALOGI("%s: stream %d, key %s, val: %d", __func__, id, key, userVal);
    for (auto& node : stream) {
        // node.prevModule->SetParameter(key, (float)userVal);
        node.pModule->setParam(key, static_cast<float>(userVal));
    }
    return true;
}

int MediaPipeline::configDewarp(bool enable) {
    int ret = -1, en = enable ? 1 : 0;
    int fd = openVideoDevice(devId);
    if (fd >= 0) {
        ret = ioctl(fd, VIV_VIDIOC_S_DWECFG, &en);
        closeVideoDevice(&fd);
    }
    return ret;
}

bool MediaPipeline::init()
{
    bool ret;
    mStreamsCapability.clear();

    Json::Value node;

    ALOGI("NativeSensor Creat");
    IMediaModule* pSensorModule = ModuleRegistry::inst()->create("NativeSensor");
    ret = pSensorModule->open(devId,node);
    if(ret == false) {
        ALOGE("NativeSensor open error!");
        return ret;
    }
    mapModules["NativeSensor"] = pSensorModule;

    ALOGI("NativeDewarp Creat");
    IMediaModule* pDewarpModule = ModuleRegistry::inst()->create("NativeDewarp");
    ret = pDewarpModule->open(devId,node);
    if(ret == false) {
        pSensorModule->close();
        ALOGE("NativeDewarp open error!");
        return ret;
    }
    mapModules["NativeDewarp"] = pDewarpModule;

    ALOGI("V4l2Control Creat");
    IMediaModule* pControlModule = ModuleRegistry::inst()->create("V4l2Control");
    ret = pControlModule->open(devId,node);
    if(ret == false) {
        pDewarpModule->close();
        pSensorModule->close();
        ALOGE("V4l2Control open error!");
        return ret;
    }
    mapModules["V4l2Control"] = pControlModule;

    int streamid = 0;
    StreamNode node0 = { pSensorModule, pDewarpModule, 0, 0 };
    mapStreams[streamid].push_back(node0);
    StreamNode node1 = { pDewarpModule, pControlModule, 0, 0 };
    mapStreams[streamid].push_back(node1);
    //StreamNode node0 = { pSensorModule, pControlModule, 0, 0 };
    //mapStreams[streamid].push_back(node0);

    configDewarp(true);
    return true;
}

bool MediaPipeline::load(const char* szFileName) {
    mStreamsCapability.clear();
    if (!parseMediaControlConfig(szFileName) )
        return false;
    return true;
}

void MediaPipeline::reigsterBufferCallBack(std::function<void(MediaBuffer*)> callback) {
    auto stream = mapStreams[0];
    auto node = stream.back();
    node.pModule->reigsterBufferCallBack(callback);
}

void MediaPipeline::removeBufferCallBack() {
    auto stream = mapStreams[0];
    auto node = stream.back();
    node.pModule->removeBufferCallBack();
}

bool MediaPipeline::queueBuffer(int id, uint64_t addr) {
    auto stream = mapStreams[id];
    //ALOGI("push buffer %s %d %p", stream.back().prevModule->getClassName(), stream.back().prePort, (unsigned char*) addr);
    GPUSH(stream.back().prevModule, 1, stream.back().prePort, addr);
    stream.back().prevModule->onGlobalPadBufferChanged(1, stream.back().prePort);
    return true;
}

bool MediaPipeline::setMode(int id, int msgType, void* param) {
    auto stream = mapStreams[id];
    for (auto& node : stream) {
        node.prevModule->setMode(msgType, param);
    }
    return true;
}

int MediaPipeline::jsonRequest(int streamid, int ctrlId, Json::Value& jsonRequest, Json::Value& jsonResponse) {
    int ret = 0;
    auto stream = mapStreams[streamid];
    switch (ctrlId) {
        case VIV_V4L_MC_SET_LINKS: break;
        case VIV_V4L_MC_USE_COMPONENT: break;
        default: {
            for (auto& node : stream) {
                int rc = node.prevModule->jsonRequest(node.prePort, ctrlId, jsonRequest, jsonResponse);
                if (rc != 0)
                    ret |= rc;
            }
            break;
        }
    }
    return ret;
}

bool MediaPipeline::remove() {

    ALOGI("NativeSensor stop");
    IMediaModule* pSensorModule = mapModules["NativeSensor"];
    if(pSensorModule)
    {
        pSensorModule->stop();
    }

    ALOGI("NativeDewarp stop");
    IMediaModule* pDewarpModule = mapModules["NativeDewarp"];
    if(pDewarpModule)
    {
        pDewarpModule->stop();
        
    }

    ALOGI("V4l2Control stop");
    IMediaModule* pControlModule = mapModules["V4l2Control"];
    if(pControlModule)
    {
        pControlModule->stop();
    }

    return true;
}

bool MediaPipeline::parseMediaControlConfig(const char* pFileName) {
    std::string strInputFileName = pFileName;
    std::ifstream configStream(strInputFileName);
    Json::Reader reader;
    Json::Value rootNode;
    int streamid = 0;

    bool parseOk = reader.parse(configStream, rootNode, false);
    if (!parseOk) {
        ALOGD("Failed to read configuration file %s", pFileName);
        ALOGD("%s", reader.getFormatedErrorMessages().c_str());
        return false;
    }
    {
        Json::Value node = rootNode["using components"];
        if (!node.isArray()) return false;

        for (auto&& item : node) {
            const char* szComponetName = item["classname"].asCString();
            const char* szComponetId= item["id"].asCString();
            IMediaModule* pModule = ModuleRegistry::inst()->create(szComponetName);
            if (pModule == NULL) {
                ALOGE("can't find class name: %s", szComponetName);
                return false;
            }
            pModule->open(devId, item);
            ALOGI("add component %s", szComponetName);
            mapModules[szComponetId] = pModule;
        }
    }
    {
        Json::Value streamArray = rootNode["streams"];
        if (!streamArray.isArray()) return false;
        for (auto&& stream : streamArray) {
            Json::Value linkArray = stream["links"];
            for (auto&& link : linkArray) {
                Json::Value linkNode = link["prev"];
                std::string strPrev = linkNode.asCString();
                linkNode = link["next"];
                std::string strNext = linkNode.asCString();

                int srcpad = link["srcpad"].asUInt();
                int sinkpad = link["sinkpad"].asUInt();
                IMediaModule* pPrevModule = NULL;
                IMediaModule* pNextMoudle = NULL;

                auto iter = mapModules.find(strPrev);
                if (iter == mapModules.end()) {
                    ALOGE("Need add using component: %s", strPrev.c_str());
                    return false;
                }
                pPrevModule = iter->second;
                iter = mapModules.find(strNext);
                if (iter == mapModules.end()) {
                    ALOGE("Need add using component: %s", strNext.c_str());
                    return false;
                }
                pNextMoudle = iter->second;
                StreamNode node = { pPrevModule, pNextMoudle, srcpad, sinkpad };
                mapStreams[streamid].push_back(node);
            }
            Json::Value asc = stream["caps"]["availableStreamConfigurations"];
            if (asc.isArray()) {
                for (auto&& cap : asc) {
                    MediaCap sc;
                    sc.format = mediaFormatStringTable[cap[0].asCString()];
                    sc.width = cap[1].asInt();
                    sc.height = cap[2].asInt();
                    ALOGD("stream caps : %d format: %d w:%d h:%d", streamid, sc.format, sc.width, sc.height);
                    mStreamsCapability[streamid].push_back(sc);
                }
            }
            streamid++;
        }
    }

    mCapsArray.resize(streamid, CapsArray());
    for (size_t i = 0; i < mCapsArray.size(); i++) {
        if(mStreamsCapability.find(i) == mStreamsCapability.end())
            continue;  // stream has not caps description!!!
        mCapsArray[i].caps_num = mStreamsCapability[i].size();
        for(size_t j = 0; j < mStreamsCapability[i].size(); j++) {
            mCapsArray[i].cap[j].format = mStreamsCapability[i][j].format;
            mCapsArray[i].cap[j].width  = mStreamsCapability[i][j].width;
            mCapsArray[i].cap[j].height = mStreamsCapability[i][j].height;
        }
    }
    return true;
}

bool MediaPipeline::setWarmupOnOff(bool on)
{
    bWarmup = on;
    return true;
}
bool MediaPipeline::getWarmupOnOff()
{
    return bWarmup;
}

bool MediaPipeline::handleEvent(EventData& data) {
    // ALOGD("handleEvent session: %d, streamid: %d", data.session_id, data.stream_id);
    int streamid = 0;
    switch (data.type) {
    case MEDIA_EVENT_STREAM_START:
        ALOGI("MEDIA_EVENT_STREAM_START");
        return start(data.stream_id);
    case MEDIA_EVENT_STREAM_STOP:
        ALOGI("MEDIA_EVENT_STREAM_STOP");
        return stop(data.stream_id);
    case MEDIA_EVENT_STREAM_OPEN:
        ALOGI("MEDIA_EVENT_STREAM_OPEN");
        return open(data.stream_id);
    case MEDIA_EVENT_STREAM_CLOSE:
        ALOGI("MEDIA_EVENT_STREAM_CLOSE");
        return close(data.stream_id);
    case MEDIA_EVENT_STREAM_SET_FMT:
        ALOGI("MEDIA_EVENT_STREAM_SET_FMT");
        return setFormat(data.stream_id, data.res);
    case MEDIA_EVENT_QUERY_STREAM_INFO:
        ALOGI("MEDIA_EVENT_QUERY_STREAM_INFO");
        data.stream_number = getStreamNumber();
        data.data = (CapsArray*) queryAllStreamCaps();
        return true;
    case MEDIA_EVENT_SET_STRING_PARAM:
        ALOGI("MEDIA_EVENT_SET_STRING_PARAM");
        return setStringParam(data.stream_id, (const char*)data.data, data.userValue);
    case MEDIA_EVENT_QUEUE_BUFFER:
        return queueBuffer(data.stream_id, (uint64_t)(uintptr_t)data.data);
    case MEDIA_EVENT_SET_MODE:
        ALOGI("MEDIA_EVENT_SET_MODE");
        return setMode(data.stream_id, data.userValue,  data.data);
    case MEDIA_EVENT_PASS_JSON:
        if (data.jsonRequest["streamid"].isObject()) {
            streamid = data.jsonRequest["streamid"].asUInt();
        }
        return jsonRequest(streamid, data.ctrlId, data.jsonRequest, data.jsonResponse);
    case MEDIA_EVENT_SET_CROP:
        streamid = data.jsonRequest["streamid"].asInt();
        data.ctrlId = ISPCORE_MODULE_DEVICE_SET_CROP;
        if ( jsonRequest(streamid, data.ctrlId, data.jsonRequest, data.jsonResponse) != 0) {
            data.ctrlId = VIV_V4L_DWE_SET_CROP;
            return jsonRequest(streamid, data.ctrlId, data.jsonRequest, data.jsonResponse);
        }
    case MEDIA_EVENT_SET_SCALE:
        streamid = data.jsonRequest["streamid"].asInt();
        data.ctrlId = ISPCORE_MODULE_DEVICE_SET_SCALE;
       if ( jsonRequest(streamid, data.ctrlId, data.jsonRequest, data.jsonResponse) != 0) {
            data.ctrlId = VIV_V4L_DWE_SET_SCALE;
            return jsonRequest(streamid, data.ctrlId, data.jsonRequest, data.jsonResponse);
        }
    }
    return false;
}
