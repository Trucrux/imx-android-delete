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
#include <stdio.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <memory.h>
#include "math.h"

#include <inttypes.h>

#include <thread>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <functional>

#include <EAutoLock.h>
#include <cam_device_api.hpp>
#include <json_helper.h>

#include "viv_video_kevent.h"
#include "ioctl_cmds.h"
#include "base64.hpp"

#include "log.h"

#define LOGTAG "VVEXT:"

ESignal sig;
int fd;
int streamid = 0;
std::string strError;

#ifndef MIN
#define MIN(a, b)   ( ((a)<=(b)) ? (a) : (b) )
#endif /* MIN */

#ifndef MAX
#define MAX(a, b)   ( ((a)>=(b)) ? (a) : (b) )
#endif /* MAX */

#define S_EXT_FLAG 555

template <typename T> int binEncode(T &object, Json::Value &jValue) {
    uint32_t size = sizeof(T);

    jValue["base64.size"] = size;
    jValue["base64.bin"] = base64_encode((u_char *)&object, size);

    return EXIT_SUCCESS;
}

class Splitter {
public:
    static std::vector<std::string> exec(const std::string &str, const std::string &pattern);
    static void exec(const std::string &str, const std::string &pattern, std::vector<uint32_t>& ret);
    static void exec(const std::string &str, const std::string &pattern, std::vector<int32_t>& ret);
    static void exec(const std::string &str, const std::string &pattern, std::vector<int64_t>& ret);
    static void exec(const std::string &str, const std::string &pattern, std::vector<float>& ret);
};

std::vector<std::string> Splitter::exec(const std::string &str, const std::string &pattern) {
    std::vector<std::string> resultVec;
    char* tmpStr = strtok((char*)str.c_str(), pattern.c_str());

    while (tmpStr != NULL) {
        resultVec.push_back(std::string(tmpStr));
        tmpStr = strtok(NULL, pattern.c_str());
    }

    return resultVec;
}

void Splitter::exec(const std::string &str,const std::string &pattern, std::vector<int32_t>& ret) {
    std::vector<std::string> stringArray = exec(str, pattern);
    ret.clear();
    for (const auto &elem : stringArray) {
        // autodetect 0x hex notation
        ret.push_back(stoi(elem, 0, 0));
    }
}

void Splitter::exec(const std::string &str,const std::string &pattern, std::vector<uint32_t>& ret) {
    std::vector<std::string> stringArray = exec(str, pattern);
    ret.clear();
    for (const auto &elem : stringArray) {
        // autodetect 0x hex notation
        ret.push_back(stoul(elem, 0, 0));
    }
}


void Splitter::exec(const std::string &str, const std::string &pattern, std::vector<int64_t>& ret) {
    std::vector<std::string> stringArray = exec(str, pattern);
    ret.clear();
    for (const auto &elem : stringArray) {
        // autodetect 0x hex notation
        ret.push_back(stoll(elem, 0, 0));
    }
}

void Splitter::exec(const std::string &str,const std::string &pattern, std::vector<float>& ret) {
    std::vector<std::string> stringArray = exec(str, pattern);;
    ret.clear();
    for (const auto &elem : stringArray) {
        ret.push_back(stof(elem));
    }
}
#define VIV_CUSTOM_CID_BASE (V4L2_CID_USER_BASE | 0xf000)
#define V4L2_CID_VIV_EXTCTRL (VIV_CUSTOM_CID_BASE + 1)

int viv_private_ioctl(const char *cmd, Json::Value& jsonRequest, Json::Value& jsonResponse) {
    if (!cmd) {
        ALOGE("cmd should not be null!");
        return -1;
    }
    jsonRequest["id"] = cmd;
    jsonRequest["streamid"] = streamid;

    struct v4l2_ext_controls ecs;
    struct v4l2_ext_control ec;
    memset(&ecs, 0, sizeof(ecs));
    memset(&ec, 0, sizeof(ec));
    ec.string = new char[VIV_JSON_BUFFER_SIZE];
    ec.id = V4L2_CID_VIV_EXTCTRL;
    ec.size = 0;
    ecs.controls = &ec;
    ecs.count = 1;

    ::ioctl(fd, VIDIOC_G_EXT_CTRLS, &ecs);

    strcpy(ec.string, jsonRequest.toStyledString().c_str());

    int ret = ::ioctl(fd, VIDIOC_S_EXT_CTRLS, &ecs);
    if (ret != 0) {
        ALOGE("failed to set ext ctrl\n");
        goto end;
    } else {
        ::ioctl(fd, VIDIOC_G_EXT_CTRLS, &ecs);
        Json::Reader reader;
        reader.parse(ec.string, jsonResponse, true);
        delete[] ec.string;
        ec.string = NULL;
        return jsonResponse["MC_RET"].asInt();
    }

end:
     delete ec.string;
     ec.string = NULL;
    return S_EXT_FLAG;
}

/*************************BASE**************************/
struct IBasePage {
    IBasePage(IBasePage* page, const char* szName);
    virtual ~IBasePage();

    void insert(IBasePage* subPage);
    virtual void dump();
    virtual IBasePage* search(const char* szName);
    virtual void process(std::string& str);
    virtual void onSelected() {}
    virtual const char* getClassName() { return mName.c_str(); }
    void navigateBack();

    std::string mName;
    std::vector<IBasePage*> pages;
    IBasePage* parent = NULL;
    bool isPage = true;
};

IBasePage* pCurrentPage;


IBasePage::IBasePage(IBasePage* page, const char* szName) { 
    parent = page;
    mName = szName;
}

IBasePage::~IBasePage() { 
    for (auto item : pages)
        delete item;
}


void IBasePage::insert(IBasePage* subPage) {
    pages.push_back(subPage);
}

void IBasePage::dump() {
    printf("%s: select and press enter\n", getClassName());
    if (pCurrentPage == this) {
        for (size_t i = 0; i < pages.size(); i++)
            printf("  %ld. %s\n", i, pages[i]->getClassName());
        printf("\n");
        if (parent)
            ALOGI("press enter to navigate back");
    } else {
        for (size_t i = 0; i < pages.size(); i++)
            pages[i]->dump();
    }
}

IBasePage* IBasePage::search(const char* szName) {
    if (strcmp(getClassName(), szName) == 0)
        return this;
    else {
        for (size_t i = 0; i < pages.size(); i++) {
            IBasePage* pSearchedPage = pages[i]->search(szName);
            if (pSearchedPage)
                return pSearchedPage;
        }
    }
    return NULL;
}

void IBasePage::navigateBack() {
    if (parent) {
        pCurrentPage = parent;
    }
}

void IBasePage::process(std::string& str) {
    if (pCurrentPage == this) {
        int id = atoi(str.c_str());
        if (id >= 0 && id < (int)pages.size() ) {
            if (pages[id]->isPage)
                pCurrentPage = pages[id];
            else
                pages[id]->onSelected();
        } else {
            strError = "input error.";
        }
    }
}

struct BasePage : IBasePage {
    using IBasePage::IBasePage;
};

struct BaseOption : IBasePage {
    using IBasePage::IBasePage;
    BaseOption(IBasePage* page, const char* szName) : IBasePage(page, szName) {
        isPage = false;
    }
    virtual void onSelected() {}
    bool enable = true;
};

/*************************PIPELINE**************************/

struct PagePipeline : BasePage {
    using BasePage::BasePage;

    void dump() {
        printf("please input pipeline On/Off: 0-off 1-on\n:");
    }

    void process(std::string &str) {
        int i = atoi(str.c_str());
        if (i == 1 || i == 0) {
            Json::Value jRequest, jResponse;
            jRequest[PIPELINE_ENABLE_PARAMS] = i;
            viv_private_ioctl(IF_PIPELINE_S_WARM_UP, jRequest, jResponse);
        } else {
            strError = ("input error.");
        }
    }

};

/*************************AWB**************************/
struct OptionAwbEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "awb enabled" : "awb disabled";
        Json::Value jRequest, jResponse;
        jRequest[AWB_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_AWB_S_EN, jRequest, jResponse);
    }
};

struct OptionAwbReset : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        jRequest[AWB_RESET_PARAMS] = enable;
        viv_private_ioctl(IF_AWB_RESET, jRequest, jResponse);
    }
};

struct PageAwbAutoCtrl : BasePage {
    using BasePage::BasePage;
    void dump() {
         Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AWB_G_EN, jRequest, jResponse);
        bool enable = jResponse[AWB_ENABLE_PARAMS].asBool();
        if (enable) {
            strError = ("awb is enabled, need disable first");
        } else {
            jRequest.clear();
            jResponse.clear();
            viv_private_ioctl(IF_AWB_G_CFG, jRequest, jResponse);

            printf("awb auto ctrl params:[%d, %d, %d]\n", 
                    jResponse[AWB_MODE_PARAMS].asInt(), jResponse[AWB_INDEX_PARAMS].asUInt(), jResponse[AWB_DAMPING_PARAMS].asBool());
            printf("please input params : [mode, profile-index, isDamping](1-manual/2-auto, index:0~4, 0/1)\n:");
        }
    }

    void process(std::string& str) {
        std::vector<float> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 3) {
            strError = ("input error [mode, profile-index, isDamping].");
        } else {
            Json::Value jRequest, jResponse;
            jRequest[AWB_MODE_PARAMS] = params[0];
            jRequest[AWB_INDEX_PARAMS] = params[1];
            jRequest[AWB_DAMPING_PARAMS] = params[2];
            viv_private_ioctl(IF_AWB_S_CFG, jRequest, jResponse);
        }
    }
};

struct OptionAwbGainR : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AWB_G_EN, jRequest, jResponse);

        bool enable = jResponse[AWB_ENABLE_PARAMS].asBool();
        if (enable) {
            strError = ("awb is enabled, need disable first");
        } else {
            Json::Value jRequest, jResponse;

            jRequest[WB_RED_PARAMS]     = 3.0f;
            jRequest[WB_GREEN_R_PARAMS] = 1.0f;
            jRequest[WB_GREEN_B_PARAMS] = 1.0f;
            jRequest[WB_BLUE_PARAMS]    = 1.0f;

            viv_private_ioctl(IF_WB_S_GAIN, jRequest, jResponse);
        }
    }
};

struct OptionAwbGainB : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AWB_G_EN, jRequest, jResponse);

        bool enable = jResponse[AWB_ENABLE_PARAMS].asBool();
        if (enable) {
            strError = ("awb is enabled, need disable first");
        } else {
            Json::Value jRequest, jResponse;

            jRequest[WB_RED_PARAMS]     = 1.0f;
            jRequest[WB_GREEN_R_PARAMS] = 1.0f;
            jRequest[WB_GREEN_B_PARAMS] = 1.0f;
            jRequest[WB_BLUE_PARAMS]    = 3.0f;

            viv_private_ioctl(IF_WB_S_GAIN, jRequest, jResponse);
        }
    }
};

struct PageAwbGain : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AWB_G_EN, jRequest, jResponse);

        bool enable = jResponse[AWB_ENABLE_PARAMS].asBool();
        if (enable)
            strError = ("awb is enabled, need disable first");
        else {
            jRequest.clear();
            jResponse.clear();
            viv_private_ioctl(IF_WB_G_CFG, jRequest, jResponse);

            printf("wb gain:[%f, %f, %f, %f]\n", jResponse[WB_RED_PARAMS] .asFloat(),
                     jResponse[WB_GREEN_R_PARAMS].asFloat(), jResponse[WB_GREEN_B_PARAMS].asFloat(), jResponse[WB_BLUE_PARAMS].asFloat());
            printf("please input params : [r, gr, gb, b](1.0 - 3.999)\n:");
        }
    }

    void process(std::string& str) {
        std::vector<float> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 4) {
            strError = ("input error [r, gr, gb, b].");
            return;
        }

        Json::Value jRequest, jResponse;
        for (int i = 0; i < 4; i++) {
            if (params[i] < 1.0f || params[i] > 3.999f) {
                strError = ("input error [r, gr, gb, b] out of range.");
                return;
            }
        }

        jRequest[WB_RED_PARAMS]     = params[0];
        jRequest[WB_GREEN_R_PARAMS] = params[1];
        jRequest[WB_GREEN_B_PARAMS] = params[2];
        jRequest[WB_BLUE_PARAMS]    = params[3];

        viv_private_ioctl(IF_WB_S_GAIN, jRequest, jResponse);

    }
};

struct PageAwbOffset : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AWB_G_EN, jRequest, jResponse);

        bool enable = jResponse[AWB_ENABLE_PARAMS].asBool();
        if (enable)
            strError = ("awb is enabled, need disable first");
        else {
            int offset[3] = {0};
            jRequest.clear();
            jResponse.clear();
            viv_private_ioctl(IF_WB_G_CFG, jRequest, jResponse);
            readArrayFromNode<int>(jResponse, WB_OFFSET_PARAMS, offset);

            printf("wb offset params:[%d, %d, %d]\n", offset[0], offset[1], offset[2]);
            printf("please input params : [r, g, b](-2048 - 2047)\n:");
        }
    }

    void process(std::string& str) {
        std::vector<int> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 3) {
            strError = ("input error [r, g, b].");
        } else {
            int16_t offset[3] = {0};
            Json::Value jRequest, jResponse;

            offset[0] = std::min(2047, std::max(-2048, params[0]));
            offset[1] = std::min(2047, std::max(-2048, params[1]));
            offset[2] = std::min(2047, std::max(-2048, params[2]));
            addArray(offset, jRequest, WB_OFFSET_PARAMS);

            viv_private_ioctl(IF_WB_S_OFFSET, jRequest, jResponse);
        }
    }
};

struct PageAwbCCM : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AWB_G_EN, jRequest, jResponse);

        bool enable = jResponse[AWB_ENABLE_PARAMS].asBool();
        if (enable) {
            strError = ("awb is enabled, need disable first");
        } else {
            float coeff[9U];
            jRequest.clear();
            jResponse.clear();
            viv_private_ioctl(IF_WB_G_CFG, jRequest, jResponse);
            readArrayFromNode(jResponse, WB_MATRIX_PARAMS, coeff);

            printf("wb ccm params:[");
            for (int i = 0; i < 8; i++) {
                printf("%f,", coeff[i]);
            }
            printf("%f]\n", coeff[8]);
            printf("please input 9 params : [-8 - 7.992]\n:");
        }
    }

    void process(std::string& str) {
        std::vector<float> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 9) {
            strError = ("input error, input 9 params.");
        } else {
            int i;
            float coeff[9U];
            Json::Value jRequest, jResponse;

            for (i = 0; i< 9; i++) {
                coeff[i] = std::min(7.992f, std::max(-8.0f, params[i]));
            }

            addArray(coeff, jRequest, WB_MATRIX_PARAMS);
            viv_private_ioctl(IF_WB_S_CCM, jRequest, jResponse);
        }
    }
};

struct PageAwbMeasWin : BasePage {
    using BasePage::BasePage;
    void dump() {
        printf("please input params : [left, top, width, height]\n:");
    }
    void process(std::string& str) {
        std::vector<float> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 4) {
            strError = ("input error [left, top, width, height].");
        } else {
            Json::Value jRequest, jResponse;
            jRequest[RECT_LEFT] = params[0];
            jRequest[RECT_TOP] = params[1];
            jRequest[RECT_WIDTH] = params[2];
            jRequest[RECT_HEIGHT] = params[3];
            viv_private_ioctl(IF_AWB_S_MEASWIN, jRequest, jResponse);
        }
    }
};

struct PageAwb : BasePage {
    using BasePage::BasePage;
    PageAwb(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionAwbEnable(this, "AWB On/Off"));
        insert(new OptionAwbReset(this, "AWB Reset"));
        insert(new PageAwbAutoCtrl(this, "AWB Auto Ctrl item"));
        insert(new OptionAwbGainR(this, "GAIN RED to 3") );
        insert(new OptionAwbGainB(this, "GAIN BLUE to 3"));
        insert(new PageAwbGain(this, "GAIN INPUT"));
        insert(new PageAwbCCM(this, "CCM INPUT"));
        insert(new PageAwbOffset(this, "Offset INPUT"));
        insert(new PageAwbMeasWin(this, "Measuring Window Set"));
    }
};

struct PageFps : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        uint32_t fps = 0;
        uint32_t maxFps = 0;
        uint32_t minFps = 0;

        viv_private_ioctl(IF_SENSOR_QUERY, jRequest, jResponse);
        unsigned int CurModeIndex = jResponse["current"].asUInt();
        for (auto& item : jResponse["caps"]) {
            if (CurModeIndex == item["index"].asUInt()) {
                maxFps = item["fps"].asUInt();
                minFps = item["min_fps"].asUInt();
                break;
            }
        }

        minFps = (minFps==0) ? 5 : minFps;

        jRequest.clear();
        jResponse.clear();
        viv_private_ioctl(IF_G_FPS, jRequest, jResponse);
        fps = jResponse[SENSOR_FPS_PARAMS].asUInt();
        if (fps > maxFps) {
            fps = maxFps;
        }

        if (fps < minFps) {
            fps = minFps;
        }

        printf("fps:%d\n",fps);
        printf("please input new fps: [%d ~ %d] \n:", minFps, maxFps);
    }
    void process(std::string &str) {
        Json::Value jRequest, jResponse;
        uint32_t maxFps = 0;
        uint32_t minFps = 0;
        viv_private_ioctl(IF_SENSOR_QUERY, jRequest, jResponse);
        unsigned int CurModeIndex = jResponse["current"].asUInt();
        for (auto& item : jResponse["caps"]) {
            if (CurModeIndex == item["index"].asUInt()) {
                maxFps = item["fps"].asUInt();
                minFps = item["min_fps"].asUInt();
                break;
            }
        }

        minFps = (minFps==0) ? 5 : minFps;

        uint32_t fps = atoi(str.c_str());
        if (fps >= minFps && fps <= maxFps) {
            jRequest.clear();
            jResponse.clear();
            jRequest[DEVICE_FPS_PARAMS] = fps;
            viv_private_ioctl(IF_S_FPS, jRequest, jResponse);
        } else {
            strError = ("input error fps.");
        }
    }
};

struct PageAecSetPoint : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);

        printf("ae setpoint:%f\n", jResponse[AE_SET_POINT_PARAMS].asFloat());
        printf("please input new setpoint: [0.0 ~ 255.0]\n:");
    }
    void process(std::string &str) {
        Json::Value jRequest, jResponse;
        float setpoint = 0;
        setpoint = atof(str.c_str());
        if (setpoint >= 0.0 && setpoint <= 255.0) {
            viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);

            jRequest = jResponse;
            jRequest[AE_SET_POINT_PARAMS] = atof(str.c_str());
            viv_private_ioctl(IF_AE_S_CFG, jRequest, jResponse);
        } else {
            strError = ("input error.");
        }
    }
};

struct PageAecDampOver : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);

        printf("ae dampOver:%f\n", jResponse[AE_DAMP_OVER_PARAMS].asFloat());
        printf("please input new dampOver: [0.0 ~ 1.0]\n:");
    }
    void process(std::string &str) {
        Json::Value jRequest, jResponse;
        float dampOver = -1.0;

        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);
        dampOver = atof(str.c_str());
        if (dampOver > 1.0 || dampOver < 0.0) {
            strError = ("input error.");
            return;
        }
        jRequest = jResponse;
        jRequest[AE_DAMP_OVER_PARAMS] = dampOver;
        viv_private_ioctl(IF_AE_S_CFG, jRequest, jResponse);
    }
};

struct PageAecDampUnder : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);

        printf("ae dampUnder:%f\n", jResponse[AE_DAMP_UNDER_PARAMS].asFloat());
        printf("please input new dampUnder: [0.0 ~ 1.0]\n:");
    }
    void process(std::string &str) {
        Json::Value jRequest, jResponse;
        float dampUnder = -1.0;

        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);
        dampUnder = atof(str.c_str());
        if (dampUnder > 1.0 || dampUnder < 0.0) {
            strError = ("input error.");
            return;
        }

        jRequest = jResponse;
        jRequest[AE_DAMP_UNDER_PARAMS] = dampUnder;
        viv_private_ioctl(IF_AE_S_CFG, jRequest, jResponse);
    }
};

struct PageAecTolerance : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);

        printf("ae clmTolerance:%f\n", jResponse[AE_CLM_TOLERANCE_PARAMS].asFloat());
        printf("please input new clmTolerance: [0.0 ~ 100.0]]\n:");
    }
    void process(std::string &str) {
        Json::Value jRequest, jResponse;
        float tolerence = 0;
        tolerence = atof(str.c_str());
        if (tolerence >= 0 && tolerence <= 100.0) {
            viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);
    
            jRequest = jResponse;
            jRequest[AE_CLM_TOLERANCE_PARAMS] = atof(str.c_str());
            viv_private_ioctl(IF_AE_S_CFG, jRequest, jResponse);
        } else {
            strError = ("input error.");
        }
        
    }
};

struct OptionAecEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "aec enabled" : "aec disabled";
        Json::Value jRequest, jResponse;
        jRequest[AE_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_AE_S_EN, jRequest, jResponse);
    }
};

struct PageAecGain : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        bool enable = false;
        int ret = viv_private_ioctl(IF_AE_G_EN, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            enable = jResponse[AE_ENABLE_PARAMS].asBool();
            if (enable) {
                strError = ("ae is enabled, need disable first");
                return;
            }
        } else {
            ALOGE("EC enable get failed, return\n");
            return;
        }

        jRequest.clear();
        jResponse.clear();
        ret = viv_private_ioctl(IF_EC_G_CFG, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            double minGain = jResponse[EC_GAIN_MIN_PARAMS].asDouble();
            double maxGain = jResponse[EC_GAIN_MAX_PARAMS].asDouble();
            double currentGain = jResponse[EC_GAIN_PARAMS].asDouble();
            printf("ec gain:%f\n", currentGain);
            printf("Please input new gain: [%f ~ %f]\n", minGain, maxGain);
        } else {
            ALOGE("EC gain get failed, return\n");
            return;
        }
    }

    void process(std::string &str) {
        Json::Value jRequest, jResponse;
        double minGain, maxGain, currentGain, currentInt;
        int ret = viv_private_ioctl(IF_EC_G_CFG, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            minGain = jResponse[EC_GAIN_MIN_PARAMS].asDouble();
            maxGain = jResponse[EC_GAIN_MAX_PARAMS].asDouble();
            currentGain = jResponse[EC_GAIN_PARAMS].asDouble();
            currentInt = jResponse[EC_TIME_PARAMS].asDouble();
        } else {
            ALOGE("EC gain get failed, return\n");
            return;
        }
        double setGain_in = str.empty()?currentGain:std::stod(str);
        double setGain = MAX(MIN(setGain_in, maxGain), minGain);
        if (setGain_in != setGain) {
            ALOGE("data out range, set gain (%f) to (%f)", setGain_in, setGain);
        }
        jRequest[EC_GAIN_PARAMS] = setGain;
        jRequest[EC_TIME_PARAMS] = currentInt;
        ret = viv_private_ioctl(IF_EC_S_CFG, jRequest, jResponse);
        if (S_EXT_FLAG == ret) {
            ALOGE("EC gain set failed, return\n");
            return;
        }
    }
};

struct PageAecExposureTime : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        bool enable = false;

        int ret = viv_private_ioctl(IF_AE_G_EN, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            enable = jResponse[AE_ENABLE_PARAMS].asBool();
            if (enable) {
                strError = ("ae is enabled, need disable first");
                return;
            }
        } else {
            ALOGE("EC enable get failed, return\n");
            return;
        }

        jRequest.clear();
        jResponse.clear();
        ret = viv_private_ioctl(IF_EC_G_CFG, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            double minInt = jResponse[EC_INTEGRATION_MIN_PARAMS].asDouble();
            double maxInt = jResponse[EC_INTEGRATION_MAX_PARAMS].asDouble();
            double currentInt = jResponse[EC_TIME_PARAMS].asDouble();

            if (currentInt > maxInt) {
                currentInt = maxInt;
            }

            if (currentInt < minInt) {
                currentInt = minInt;
            }

            printf("ec exposure time:%f\n", currentInt);
            printf("please input new exposure time value(%f~%f):\n:", minInt, maxInt);
        } else {
            ALOGE("EC integration get failed, return\n");
            return;
        }
    }
    void process(std::string &str) {
        double minInt, maxInt, currentGain, currentInt;
        Json::Value jRequest, jResponse;
        int ret = viv_private_ioctl(IF_EC_G_CFG, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            minInt = jResponse[EC_INTEGRATION_MIN_PARAMS].asDouble();
            maxInt = jResponse[EC_INTEGRATION_MAX_PARAMS].asDouble();
            currentGain = jResponse[EC_GAIN_PARAMS].asDouble();
            currentInt = jResponse[EC_TIME_PARAMS].asDouble();
        } else {
            ALOGE("EC integration get failed, return\n");
            return;
        }
        double setInt_in = str.empty()?currentInt:std::stod(str);
        double setInt = MAX(MIN(setInt_in, maxInt), minInt);
        if (setInt_in != setInt) {
            ALOGE("data out range, set integration(%f) to (%f)", setInt_in, setInt);
        }
        jRequest[EC_GAIN_PARAMS] = currentGain;
        jRequest[EC_TIME_PARAMS] = setInt;
        ret = viv_private_ioctl(IF_EC_S_CFG, jRequest, jResponse);
        if (S_EXT_FLAG == ret) {
            ALOGE("EC integration set failed, return\n");
            return;
        }
    }
};

struct OptionAecReset : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        jRequest[AE_RESET_PARAMS] = enable;
        viv_private_ioctl(IF_AE_RESET, jRequest, jResponse);
    }
};

struct PageAecSensitivity : BasePage {
    using BasePage::BasePage;
    void dump() {
        bool enable = false;
        int minIso = 0;
        int maxIso = 0;
        int currentIso = 0;
        Json::Value jRequest, jResponse;

        int ret = viv_private_ioctl(IF_AE_G_EN, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            enable = jResponse[AE_ENABLE_PARAMS].asBool();
            if (enable) {
                strError = ("ae is enabled, need disable first");
                return;
            }
        } else {
            ALOGE("EC enable get failed, return\n");
            return;
        }

        ret = viv_private_ioctl(IF_AE_G_ISO, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            minIso = jResponse[AE_SENSITIVITY_MIN_PARAMS].asInt();
            maxIso = jResponse[AE_SENSITIVITY_MAX_PARAMS].asInt();
            currentIso = jResponse[AE_SENSITIVITY_PARAMS].asInt();
            printf("ae iso:%d\n", currentIso);
            printf("Please input new iso [%d ~ %d]:\n", minIso, maxIso);
        } else {
            ALOGE("EC iso get failed, return\n");
            return;
        }
    }

    void process(std::string &str) {
        int minIso = 0;
        int maxIso = 0;
        int currentIso = 0;
        int setIso_in = 0;
        int setIso = 0;
        Json::Value jRequest, jResponse;

        int ret = viv_private_ioctl(IF_AE_G_ISO, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            minIso = jResponse[AE_SENSITIVITY_MIN_PARAMS].asInt();
            maxIso = jResponse[AE_SENSITIVITY_MAX_PARAMS].asInt();
            currentIso = jResponse[AE_SENSITIVITY_PARAMS].asInt();
        } else {
            ALOGE("EC iso get failed, return\n");
            return;
        }

        setIso_in = str.empty() ? currentIso : std::stoi(str);
        setIso = MAX(MIN(setIso_in, maxIso), minIso);
        if (setIso_in != setIso) {
            ALOGE("data out range, set iso (%d) to (%d)", setIso_in, setIso);
        }

        jRequest[AE_SENSITIVITY_PARAMS] = setIso;
        ret = viv_private_ioctl(IF_AE_S_ISO, jRequest, jResponse);
        if (S_EXT_FLAG == ret) {
            ALOGE("EC iso set failed, return\n");
            return;
        }
    }
};

struct PageAecWeight : BasePage {
    using BasePage::BasePage;
    void dump() {
        uint8_t weight[25] = {0};
        Json::Value jRequest, jResponse;
        bool enable;

        viv_private_ioctl(IF_AE_G_EN, jRequest, jResponse);
        enable = jResponse[AE_ENABLE_PARAMS].asBool();
        if (!enable) {
            strError = ("ae is disabled, need enable first");
            return;
        }

        viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);
        readArrayFromNode(jResponse, AE_WEIGHT_PARAMS, weight);
        printf("ae blocks weight:[");
        for (int i = 0; i < 24; i++) {
            printf("%d,", weight[i]);
        }
        printf("%d]\n", weight[24]);
        printf("please input 25 params : [0 ~ 16]\n:");
    }

    void process(std::string& str) {
        int i = 0;
        uint8_t weight[25] = {0};
        std::vector<uint32_t> params;
        Json::Value jRequest, jResponse;

        Splitter::exec(str, ",", params);
        if (params.size() < 25) {
            strError = ("input error.");
        } else {
            for (i = 0; i < 25; i++) {
                weight[i] = std::min(16U, std::max(0U, params[i]));
            }

            viv_private_ioctl(IF_AE_G_CFG, jRequest, jResponse);
            jRequest = jResponse;

            addArray(weight, jRequest, AE_WEIGHT_PARAMS);
            viv_private_ioctl(IF_AE_S_CFG, jRequest, jResponse);
        }
    }
};

struct PageAec : BasePage {
    using BasePage::BasePage;
    PageAec(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionAecEnable(this, "AEC On/Off"));
        insert(new OptionAecReset(this, "AEC Reset"));
        insert(new PageAecSetPoint(this, "AEC SetPoint"));
        insert(new PageAecDampOver(this, "AEC DampOver"));
        insert(new PageAecDampUnder(this, "AEC DampUnder"));
        insert(new PageAecTolerance(this, "AEC Tolerance"));
        insert(new PageAecGain(this, "AEC Gain"));
        insert(new PageAecExposureTime(this, "AEC ExposureTime"));
        insert(new PageAecSensitivity(this, "AEC Sensitivity"));
        insert(new PageAecWeight(this, "AEC weight"));
    }
};

/*************************AF***************************/
struct OptionAfEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AF_G_EN, jRequest, jResponse);
        enable = jResponse[AF_ENABLE_PARAMS].asBool();

        enable = !enable;
        strError = enable ? "af enabled" : "af disabled";
        jRequest[AF_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_AF_S_EN, jRequest, jResponse);
    }
};

struct PageAfWorkMode : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);

        printf("af working mode:%d\n",jResponse[AF_MODE_PARAMS].asInt());
        printf("please input mode : [1-manual,2-auto)]\n:");
    }

    void process(std::string &str) {
        int mode = 0;
        bool enable = false;
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AF_G_EN, jRequest, jResponse);
        enable = jResponse[AF_ENABLE_PARAMS].asBool();
        if (enable == false) {
            strError = ("af is disabled, need enable first");
            return;
        }

        mode = std::atoi(str.c_str());
        if (mode <= 0 || mode >= 3) {
            strError = ("input out of range");
            return;
        }

        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        jRequest[AF_MODE_PARAMS] = mode;
        viv_private_ioctl(IF_AF_S_CFG, jRequest, jResponse);
    }
};

struct PageAfSearchAlgo : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);

        printf("af search algorithm:%d\n", jResponse[AF_ALGORITHM_PARAMS].asInt());
        printf("please input search algorithm : [1-full search/2-adaptive range search/3-hill climbing]\n:");
    }

    void process(std::string &str) {
        int algo = 0;
        int mode = 0;
        bool enable = false;
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AF_G_EN, jRequest, jResponse);
        enable = jResponse[AF_ENABLE_PARAMS].asBool();
        if (enable == false) {
            strError = ("af is disabled, need enable first");
            return;
        }

        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);
        mode = jResponse[AF_MODE_PARAMS].asInt();
        if (mode != CAM_ENGINE_AF_MODE_AUTO) {
            strError = ("af mode need set to auto first");
            return;
        }

        algo = std::atoi(str.c_str());
        if (algo <= CAM_ENGINE_AUTOFOCUS_SEARCH_ALGORITHM_INVALID || algo >= CAM_ENGINE_AUTOFOCUS_SEARCH_ALGORITHM_MAX) {
            strError = ("input out of range");
            return;
        }

        jRequest = jResponse;
        jRequest[AF_ALGORITHM_PARAMS] = algo;
        viv_private_ioctl(IF_AF_S_CFG, jRequest, jResponse);
    }
};

struct PageAfLength : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);

        printf("af lens position:%d\n", jResponse[AF_LENGTH_PARAMS].asUInt());
        printf("please input param : [0 ~ 100]\n:");
    }

    void process(std::string &str) {
        int length = 0;
        int mode = 0;
        bool enable = false;
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AF_G_EN, jRequest, jResponse);
        enable = jResponse[AF_ENABLE_PARAMS].asBool();
        if (enable == false) {
            strError = ("af is disabled, need enable first");
            return;
        }

        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);
        mode = jResponse[AF_MODE_PARAMS].asInt();
        if (mode != CAM_ENGINE_AF_MODE_MANUAL) {
            strError = ("af mode need set to manual first");
            return;
        }

        length = std::atoi(str.c_str());
        if (length < 0 || length > 100) {
            strError = ("input out of range");
            return;
        }

        jRequest = jResponse;
        jRequest[AF_LENGTH_PARAMS] = length;
        viv_private_ioctl(IF_AF_S_CFG, jRequest, jResponse);
    }
};

struct PageAfOneShot : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        uint32_t width = 0, height = 0;
        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);

        printf("af oneshot and measure window params(oneshot, startX, startY, width, height):[%d, %d, %d, %d, %d]\n", 
                jResponse[AF_ONESHOT_PARAMS].asBool(), jResponse[AF_WIN_X_PARAMS].asUInt(), jResponse[AF_WIN_Y_PARAMS].asUInt(),
                jResponse[AF_WIN_WIDTH_PARAMS].asUInt(), jResponse[AF_WIN_HEIGHT_PARAMS].asUInt());

        jRequest.clear();
        jResponse.clear();
        viv_private_ioctl(IF_G_RESOLUTION, jRequest, jResponse);
        width = jResponse[DEVICE_IMAGE_WIDTH_PARAMS].asUInt();
        height = jResponse[DEVICE_IMAGE_HEIGHT_PARAMS].asUInt();

        printf("please input params: [0/1, (5~%d), (2~%d), (5~%d), (2~%d)]\n:", width-5, height-4, width-5, height-4);
    }

    void process(std::string &str) {
        bool enable = false;
        std::vector<float> params;
        uint32_t width = 0, height = 0;
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AF_G_EN, jRequest, jResponse);
        enable = jResponse[AF_ENABLE_PARAMS].asBool();
        if (enable == false) {
            strError = ("af is disabled, need enable first");
            return;
        }

        Splitter::exec(str, ",", params);
        if (params.size() < 5) {
            strError = ("input error [oneshot, startX, startY, width, height].");
            return;
        }

        if (params[0] < 0 || params[0] > 1) {
            strError = ("oneshot is error, 0/1");
            return;
        }

        jRequest.clear();
        jResponse.clear();
        viv_private_ioctl(IF_G_RESOLUTION, jRequest, jResponse);
        width = jResponse[DEVICE_IMAGE_WIDTH_PARAMS].asUInt();
        height = jResponse[DEVICE_IMAGE_HEIGHT_PARAMS].asUInt();
        if ((params[1] < 5 || params[1] > width -5) ||
            (params[2] < 2 || params[2] > height -2) ||
            (params[3] < 5 || params[3] > width -5) ||
            (params[4] < 2 || params[4] > height -2) ||
            (params[1] + params[3] > width) ||
            (params[2] + params[4] > height - 2)) {
            strError = ("af measure window range is error");
            return;
        }

        jRequest.clear();
        jResponse.clear();
        viv_private_ioctl(IF_AF_G_CFG, jRequest, jResponse);
        jRequest = jResponse;

        jRequest[AF_ONESHOT_PARAMS]      = params[0] == 0 ? false : true;
        jRequest[AF_WIN_X_PARAMS]        = params[1];
        jRequest[AF_WIN_Y_PARAMS]        = params[2];
        jRequest[AF_WIN_WIDTH_PARAMS]    = params[3];
        jRequest[AF_WIN_HEIGHT_PARAMS]   = params[4];
        viv_private_ioctl(IF_AF_S_CFG, jRequest, jResponse);
    }
};

struct PageAf : BasePage {
    using BasePage::BasePage;
    bool isAvailable = false;
    void dump() {
        if (!isAvailable) {
            strError = "sensor not support Af";
        } else {
            BasePage::dump();
        }
    }

    PageAf(IBasePage* page, const char* szName) : BasePage(page, szName) {
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_AF_G_AVI, jRequest, jResponse);
        isAvailable = jResponse[AF_AVAILABEL_PARAMS].asBool();
        if (isAvailable) {
            insert(new OptionAfEnable(this, "AF On/Off"));
            insert(new PageAfWorkMode(this, "AF WorkingMode"));
            insert(new PageAfOneShot(this, "AF OneShot"));
            insert(new PageAfSearchAlgo(this, "AF SearchAlgorithm"));
            insert(new PageAfLength(this, "AF Length"));
        }
    }
};

/*************************DWE**************************/
enum {
    DEWARP_MODEL_LENS_DISTORTION_CORRECTION = 1 << 0,
    DEWARP_MODEL_FISHEYE_EXPAND             = 1 << 1,
    DEWARP_MODEL_SPLIT_SCREEN               = 1 << 2,
    DEWARP_MODEL_FISHEYE_DEWARP             = 1 << 3,
};

typedef struct DewarpParams {
    int mode;
    bool hflip, vflip, bypass;
    double mat[17];
} DewarpParams;

DewarpParams dweParams;

void parseDewarpParams(Json::Value& node) {
    JH_READ_VAL(dweParams.mode, node, "mode");
    JH_READ_VAL(dweParams.hflip, node, "hflip");
    JH_READ_VAL(dweParams.vflip, node, "vflip");
    JH_READ_VAL(dweParams.bypass, node, "bypass");
    JH_READ_ARRAY(node, "mat", dweParams.mat);
}

void setDewarpParams() {
    Json::Value jRequest, jResponse;
    jRequest["dwe"]["mode"] = dweParams.mode;
    jRequest["dwe"]["hflip"] = dweParams.hflip;
    jRequest["dwe"]["vflip"] = dweParams.vflip;
    jRequest["dwe"]["bypass"] = dweParams.bypass;
    for (int i = 0; i < 17; i++) {
        jRequest["dwe"]["mat"][i] = dweParams.mat[i];
    }
    viv_private_ioctl(IF_DWE_S_PARAMS, jRequest, jResponse);
}
static void getDewarpVflip() {
    Json::Value jRequest, jResponse;
    viv_private_ioctl(IF_DWE_G_VFLIP, jRequest, jResponse);
    dweParams.vflip = jResponse["dwe"]["vflip"].asBool();
}

static void getDewarpHflip() {
    Json::Value jRequest, jResponse;
    viv_private_ioctl(IF_DWE_G_HFLIP, jRequest, jResponse);
    dweParams.hflip = jResponse["dwe"]["hflip"].asBool();
}

struct OptionDewarpMode0 : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        dweParams.mode = DEWARP_MODEL_LENS_DISTORTION_CORRECTION;
        setDewarpParams();
    }
};

struct OptionDewarpMode1 : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        dweParams.mode = DEWARP_MODEL_FISHEYE_EXPAND;
        setDewarpParams();
    }
};

struct OptionDewarpMode2 : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        dweParams.mode = DEWARP_MODEL_SPLIT_SCREEN;
        setDewarpParams();
    }
};

struct OptionDewarpMode3 : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        dweParams.mode = DEWARP_MODEL_FISHEYE_DEWARP;
        setDewarpParams();
    }
};

struct OptionDewarpHFlip : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        getDewarpHflip();
        dweParams.hflip = !dweParams.hflip;
        printf("set dwe hflip %d\n",dweParams.hflip);
        setDewarpParams();
    }
};

struct OptionDewarpVFlip : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        getDewarpVflip();
        dweParams.vflip = !dweParams.vflip;
        printf("set dwe vflip %d\n",dweParams.vflip);
        setDewarpParams();
    }
};

struct OptionDewarpBypass : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        dweParams.bypass = !dweParams.bypass;
        printf("set dwe bypass %d\n",dweParams.bypass);
        setDewarpParams();
    }
};

struct PageDewarpMap : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_DWE_G_PARAMS, jRequest, jResponse);
        parseDewarpParams(jResponse["dwe"]);

        printf("dewarp matrix:[");
        for (int i = 0; i < 16; i++) {
            printf("%lf,", dweParams.mat[i]);
        }
        printf("%lf]\n", dweParams.mat[16]);
        printf("please input 17 double number: (9 matrix + 8 coefficient)\n:");
    }
    void process(std::string &str) {
        std::vector<float> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 17) {
            strError = "input error number";
        } else {
            for (int i = 0; i < 17; i++) {
                dweParams.mat[i] = params[i];
            }
            setDewarpParams();
        }
    }
};

struct PageDwe : BasePage {
    PageDwe(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionDewarpMode0(this, "DEWARP_MODEL_LENS_DISTORTION_CORRECTION"));
        insert(new OptionDewarpMode1(this, "DEWARP_MODEL_FISHEYE_EXPAND"));
        // insert(new OptionDewarpMode2(this, "DEWARP_MODEL_SPLIT_SCREEN")); BUG here
        insert(new OptionDewarpMode3(this, "DEWARP_MODEL_FISHEYE_DEWARP"));
        insert(new OptionDewarpHFlip(this, "DEWARP HFLIP ON/OFF"));
        insert(new OptionDewarpVFlip(this, "DEWARP VFLIP ON/OFF"));
        insert(new OptionDewarpBypass(this, "DEWARP BYPASS ON/OFF"));
        insert(new PageDewarpMap(this, "DEWARP MATRIX"));
    }
};

/*************************CAPTRUE**************************/
struct OptionCapture : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CAPTURE, jRequest, jResponse);
        if (jResponse["result"].asInt() != 0) {
            printf("Capture %s failed\n",jResponse["CaptureImage"].asString().c_str());
        } else {
            printf("Capture %s success\n",jResponse["CaptureImage"].asString().c_str());
        }
    }
};

struct OptionPreload : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_SENSOR_LIB_PRELOAD, jRequest, jResponse);
    }
};

struct PageRegSet : BasePage {
    using BasePage::BasePage;
    void dump() {
        printf("please input params : addr, value\n:");
    }
    void process(std::string& str) {
        std::vector<uint32_t> params;
        Splitter::exec(str, ",", params);
        if (params.size() != 2) {
            strError = ("input error addr, value.");
        } else {
            Json::Value jRequest, jResponse;
            jRequest[SENSOR_ADDRESS_PARAMS] = params[0];
            jRequest[SENSOR_VALUE_PARAMS] = params[1];

            viv_private_ioctl(IF_SENSOR_S_REG, jRequest, jResponse);
        }
    }
};

struct PageRegGet : BasePage {
    using BasePage::BasePage;
    void dump() {
        printf("please input params : addr\n:");
    }
    void process(std::string& str) {
        std::vector<uint32_t> params;
        Splitter::exec(str, ",", params);
        if (params.size() != 1) {
            strError = ("input error addr");
        } else {
            Json::Value jRequest, jResponse;
            jRequest[SENSOR_ADDRESS_PARAMS] = params[0];
            viv_private_ioctl(IF_SENSOR_G_REG, jRequest, jResponse);
            uint32_t val = jResponse[SENSOR_VALUE_PARAMS].asUInt();
            printf("Register 0x%08x:0x%08x\n", params[0], val );

        }
    }
};



/*************************BLS**************************/
// always open bls, do not support on/off at video streaming.
struct PageBls : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        int r, gr, gb, b;
        viv_private_ioctl(IF_BLS_G_CFG, jRequest, jResponse);
        r  = jResponse[BLS_RED_PARAMS].asInt();
        gr = jResponse[BLS_GREEN_R_PARAMS].asInt();
        gb = jResponse[BLS_GREEN_B_PARAMS].asInt();
        b  = jResponse[BLS_BLUE_PARAMS].asInt();
        printf("bls params: %d, %d, %d, %d\n", r, gr, gb, b);
        printf("please input params : [r, gr, gb, b]\n:");
    }
    void process(std::string& str) {
        std::vector<int> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 4) {
            strError = ("input error [r, gb, gr, b].");
        } else {
            Json::Value jRequest, jResponse;
            jRequest[BLS_RED_PARAMS] = std::max(-4095, std::min(4095, params[0]));
            jRequest[BLS_GREEN_R_PARAMS] = std::max(-4095, std::min(4095, params[1]));
            jRequest[BLS_GREEN_B_PARAMS] = std::max(-4095, std::min(4095, params[2]));
            jRequest[BLS_BLUE_PARAMS] = std::max(-4095, std::min(4095, params[3]));
            viv_private_ioctl(IF_BLS_S_CFG, jRequest, jResponse);
        }
    }
};

/*************************LSC**************************/
struct OptionLscEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "lsc enabled" : "lsc disabled";
        Json::Value jRequest, jResponse;
        jRequest[LSC_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_LSC_S_EN, jRequest, jResponse);
    }
};

struct PageLsc : BasePage {
    using BasePage::BasePage;
    PageLsc(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionLscEnable(this, "LSC ON/OFF")); // (dwe bypass is better)
    }
};

/*************************DEMOSAIC**************************/
struct OptionDemosaicMode : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "demosaic normal" : "demosaic bypass";
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_DEMOSAIC_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        jRequest[DEMOSAIC_MODE_PARAMS] = enable ? 1 : 2;
        viv_private_ioctl(IF_DEMOSAIC_S_CFG, jRequest, jResponse);
    }
};

struct PageDemosaicThreshold : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_DEMOSAIC_G_CFG, jRequest, jResponse);

        printf("demosaic threshold:%d\n", jResponse[DEMOSAIC_THRESHOLD_PARAMS].asInt());
        printf("please input threshold : [0 to 255]\n:");
    }
    void process(std::string& str) {
        int threshold = atoi(str.c_str());
        if (threshold < 0 || threshold > 255) {
            strError = ("input error.");
        } else {
            Json::Value jRequest, jResponse;
            viv_private_ioctl(IF_DEMOSAIC_G_CFG, jRequest, jResponse);
            jRequest = jResponse;
            jRequest[DEMOSAIC_THRESHOLD_PARAMS] = threshold;
            viv_private_ioctl(IF_DEMOSAIC_S_CFG, jRequest, jResponse);
        }
    }
};

struct PageDemosaic : BasePage {
    using BasePage::BasePage;
    PageDemosaic(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionDemosaicMode(this, "DEMOSAIC SWITCH MODE"));
        insert(new PageDemosaicThreshold(this, "DEMOSAIC THRESHOLD"));
    }
};

/*************************GAMMA**************************/
struct OptionGammaEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "gamma enabled" : "gamma disabled";
        Json::Value jRequest, jResponse;
        jRequest[GC_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_GC_S_EN, jRequest, jResponse);
    }
};

struct OptionGammaScaling : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "gamma mode LOG" : "gamma mode EQU";
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_GC_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        jRequest[GC_MODE_PARAMS] = enable ? 1 : 2;
        viv_private_ioctl(IF_GC_S_CFG, jRequest, jResponse);
    }
};

struct PageGammaCurve : BasePage {
    using BasePage::BasePage;
    void dump() {
        printf("please input gamma : [0.1 to 4.0]\n:");
    }
    void process(std::string& str) {
        float fGamma = atof(str.c_str());
        if (fGamma < 0.1f || fGamma > 4.0f) {
            strError = ("input error.");
        } else {
            uint16_t curve[17] = {0};
            uint16_t gamma_x_equ[16] = {256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256};
            uint16_t gamma_x_log[16] = {64, 64, 64, 64, 128, 128, 128, 128, 256, 256, 256, 256, 512, 512, 512, 512};
            uint16_t *pTable;
            Json::Value jRequest, jResponse;
            viv_private_ioctl(IF_GC_G_CFG, jRequest, jResponse);
            int mode = 0;
            JH_READ_VAL(mode, jResponse, GC_MODE_PARAMS);
            jRequest = jResponse;
            float dinvgamma = 1.0f/fGamma;
            float sumx = 0;
            pTable = mode == 1 ? gamma_x_log : gamma_x_equ;
            for(int i = 0; i < 16; i++) {
                sumx += pTable[i];
                curve[i+1]= std::min(1023.0f, std::max(0.f, pow(((float)sumx)/4096.0f, dinvgamma) * 1024));
            }
            addArray(curve, jRequest, GC_CURVE_PARAMS);
            viv_private_ioctl(IF_GC_S_CURVE, jRequest, jResponse);
        }
    }
};

struct PageGamma : BasePage {
    using BasePage::BasePage;
    PageGamma(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionGammaEnable(this, "GAMMA ON/OFF"));
        insert(new OptionGammaScaling(this, "SWITCH GAMMA SCALING MODE"));
        insert(new PageGammaCurve(this, "GAMMA CURVE"));
    }
};


/*************************FILTER**************************/
struct OptionFilterEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "filter enabled" : "filter disabled";
        Json::Value jRequest, jResponse;
        jRequest[FILTER_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_FILTER_S_EN, jRequest, jResponse);
    }
};

struct OptionFilterMode : BasePage {
    using BasePage::BasePage;
    void dump() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);

        printf("filter mode:%d\n", jResponse[FILTER_AUTO_PARAMS].asBool());
        printf("please input mode : [0-manual/1-auto]\n:");
    }

    void process(std::string &str) {
        int mode = 0;
        bool enable = false;
        Json::Value jRequest, jResponse;

        viv_private_ioctl(IF_FILTER_G_EN, jRequest, jResponse);
        enable = jResponse[FILTER_ENABLE_PARAMS].asBool();
        if (enable == true) {
            strError = ("filter is enabled, need disable first");
            return;
        }

        mode = std::atoi(str.c_str());
        if (mode != 0 && mode != 1) {
            strError = ("input out of range");
            return;
        }

        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        jRequest[FILTER_AUTO_PARAMS] = mode ;
        viv_private_ioctl(IF_FILTER_S_CFG, jRequest, jResponse);
    }
};

struct PageFilterThreshold : BasePage {
    using BasePage::BasePage;
    void dump() override {
        uint8_t dl, sl;
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        dl = jResponse[FILTER_DENOISE_PARAMS].asUInt();
        sl = jResponse[FILTER_SHARPEN_PARAMS].asUInt();

        printf("filter level(denoise, sharpen): %d, %d\n", dl, sl);
        printf("please input new two levels : [1-10]\n:");
    }

    virtual void process(std::string &str) override {
        std::vector<int> params;
        Splitter::exec(str, ",", params);
        Json::Value jRequest, jResponse;
        bool enable = false;

        int ret = viv_private_ioctl(IF_FILTER_G_EN, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            enable = jResponse[FILTER_ENABLE_PARAMS].asBool();
            if (enable) {
                strError = ("filter is enabled, need disable first");
                return;
            }
        } else {
            ALOGE("filter enable get failed, return\n");
            return;
        }

        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        jRequest = jResponse;

        if (params.size() < 2) {
            strError = "Input Error.";
        } else {
            if ( (params[0] > 10 || params[0] < 1 ) ||
                 (params[1] > 10 || params[1] < 1 ))  {
                 strError = "Input Error.";
            } else {
                jRequest[FILTER_DENOISE_PARAMS] = params[0];
                jRequest[FILTER_SHARPEN_PARAMS] = params[1];
                viv_private_ioctl(IF_FILTER_S_CFG, jRequest, jResponse);
            }
        }
    }
};

struct PageFilterChrH : BasePage {
    using BasePage::BasePage;
    void dump() override {
        int chrH;
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        chrH = jResponse[FILTER_CHRHMODE_PARAMS].asInt();

        printf("filter chroma horizontal mode: %d\n", chrH);
        printf("please input new mode: [0-3]\n:");
    }

    virtual void process(std::string &str) override {
        int chrH;
        Json::Value jRequest, jResponse;
        bool enable = false;

        int ret = viv_private_ioctl(IF_FILTER_G_EN, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            enable = jResponse[FILTER_ENABLE_PARAMS].asBool();
            if (enable) {
                strError = ("filter is enabled, need disable first");
                return;
            }
        } else {
            ALOGE("filter enable get failed, return\n");
            return;
        }

        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        chrH = jResponse[FILTER_CHRHMODE_PARAMS].asInt();

        chrH = str.empty() ? chrH : std::stoi(str);
        if (chrH > 3 || chrH < 0) {
            strError = "Input Error.";
        } else {
            jRequest[FILTER_CHRHMODE_PARAMS] = chrH;
            viv_private_ioctl(IF_FILTER_S_CFG, jRequest, jResponse);
        }
    }
};

struct PageFilterChrV : BasePage {
    using BasePage::BasePage;
    void dump() override {
        int chrV;
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        chrV = jResponse[FILTER_CHRVMODE_PARAMS].asInt();

        printf("filter chroma vertical mode: %d\n", chrV);
        printf("please input new mode : [0-3]\n:");
    }

    virtual void process(std::string &str) override {
        int chrV;
        Json::Value jRequest, jResponse;
        bool enable = false;
        int ret = viv_private_ioctl(IF_FILTER_G_EN, jRequest, jResponse);
        if (S_EXT_FLAG != ret) {
            enable = jResponse[FILTER_ENABLE_PARAMS].asBool();
            if (enable) {
                strError = ("filter is enabled, need disable first");
                return;
            }
        } else {
            ALOGE("filter enable get failed, return\n");
            return;
        }

        viv_private_ioctl(IF_FILTER_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        chrV = jResponse[FILTER_CHRVMODE_PARAMS].asInt();

        chrV = str.empty() ? chrV : std::stoi(str);
        if (chrV > 3 || chrV < 0) {
            strError = "Input Error.";
        } else {
            jRequest[FILTER_CHRVMODE_PARAMS] = chrV;
            viv_private_ioctl(IF_FILTER_S_CFG, jRequest, jResponse);
        }
    }
};

struct PageFilter : BasePage {
    using BasePage::BasePage;
    PageFilter(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionFilterEnable(this, "FILTER ON/OFF"));
        insert(new OptionFilterMode(this, "FILTER Mode"));
        insert(new PageFilterThreshold(this, "Adjust denoise/sharpen level"));
        insert(new PageFilterChrH(this, "Adjust chroma horizontal mode"));
        insert(new PageFilterChrV(this, "Adjust chroma vertical mode"));
    }
};

/*************************CAC**************************/
struct OptionCacEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "cac enabled" : "cac disabled";
        Json::Value jRequest, jResponse;
        jRequest[CAC_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_CAC_S_EN, jRequest, jResponse);
    }
};

struct PageCac : BasePage {
    using BasePage::BasePage;
    PageCac(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionCacEnable(this, "CAC ON/OFF"));
    }
};

/*************************CNR**************************/
struct OptionCnrEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "cnr enabled" : "cnr disabled";
        Json::Value jRequest, jResponse;
        jRequest[CNR_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_CNR_S_EN, jRequest, jResponse);
    }
};

struct PageCnrThreshold : BasePage {
    using BasePage::BasePage;
    void dump() override {
        uint32_t tc1, tc2;
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CNR_G_CFG, jRequest, jResponse);
        tc1 = jResponse[CNR_TC1_PARAMS].asUInt();
        tc2 = jResponse[CNR_TC2_PARAMS].asUInt();

        printf("cnr params(thresh1, thresh2): %d, %d\n", tc1, tc2);
        printf("please input params : thresh1, thresh2\n:");
    }

    virtual void process(std::string& str) override {
        std::vector<int> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 2) {
            strError = ("input error.");
        } else {
            Json::Value jRequest, jResponse;
            jRequest[CNR_TC1_PARAMS] = params[0];
            jRequest[CNR_TC2_PARAMS] = params[1];
            viv_private_ioctl(IF_CNR_S_CFG, jRequest, jResponse);
        }
    }
};

struct PageCnr : BasePage {
    using BasePage::BasePage;
    PageCnr(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionCnrEnable(this, "CNR ON/OFF"));
        insert(new PageCnrThreshold(this, "Adjust CNR threshold"));
    }
};

struct OptionCprocEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "cproc enabled" : "cproc disabled";
        Json::Value jRequest, jResponse;
        jRequest[CPROC_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_CPROC_S_EN, jRequest, jResponse);
    }
};

struct OptionCprocSwitchLumain : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        enable = !enable;
        strError = enable ? "lumain bt.601" : "lumain full range";
        jRequest[CPROC_LUMA_IN_PARAMS] = enable ? 1 : 2;
        viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);
    }
};

struct OptionCprocSwitchLumaout : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        enable = !enable;
        strError = enable ? "lumaout bt.601" : "lumaout full range";
        jRequest[CPROC_LUMA_OUT_PARAMS] = enable ? 1 : 2;
        viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);

    }
};

struct OptionCprocSwitchChromaout : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
        jRequest = jResponse;
        enable = !enable;
        strError = enable ? "chromaout bt.601" : "chromaout full range";
        jRequest[CPROC_CHROMA_OUT_PARAMS] = enable ? 1 : 2;
        viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);
    }
};

struct PageCprocAdjustBrightness : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);

        printf("cproc brightness:%d\n", jResponse[CPROC_BRIGHTNESS_PARAMS].asInt());
        printf("please input new brightness gain: [-127 to 127]\n:");
    }
    void process(std::string &str) override {
        int brightness = atoi(str.c_str());
        if (brightness > -128 && brightness < 128) {
            Json::Value jRequest, jResponse;
            viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
            jRequest = jResponse;
            jRequest[CPROC_BRIGHTNESS_PARAMS] = brightness;
            viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);
        } else {
            strError = "input error.";
        }
    }
};

struct PageCprocAdjustContrast : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);

        printf("cproc contrast:%f\n", jResponse[CPROC_CONTRAST_PARAMS].asFloat());
        printf("please input new contrast: [0 to 1.99]\n:");
    }
    void process(std::string &str) override {
        float contrast = atof(str.c_str());
        if (contrast >= 0.0f && contrast <= 1.99f) {
            Json::Value jRequest, jResponse;
            viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
            jRequest = jResponse;
            jRequest[CPROC_CONTRAST_PARAMS] = contrast;
            viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);
        } else {
            strError = "input error.";
        }
    }
};

struct PageCprocAdjustSaturation : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);

        printf("cproc saturation:%f\n", jResponse[CPROC_SATURATION_PARAMS].asFloat());
        printf("please input new saturation :[0 to 1.99]\n:");
    }
    void process(std::string &str) override {
        float saturation = atof(str.c_str());
        if (saturation >= 0.0f && saturation <= 1.99f) {
            Json::Value jRequest, jResponse;
            viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
            jRequest = jResponse;
            jRequest[CPROC_SATURATION_PARAMS] = saturation;
            viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);
        } else {
            strError = "input error.";
        }
    }
};

struct PageCprocAdjustHue : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);

        printf("cproc hue:%f\n", jResponse[CPROC_HUE_PARAMS].asFloat());
        printf("please input new hue gain: [-127.0 ~ 127.0]\n:");
    }
    void process(std::string &str) override {
        float hue = atof(str.c_str());
        if (hue >= -127.0 && hue <= 127.0) {
            Json::Value jRequest, jResponse;
            viv_private_ioctl(IF_CPROC_G_CFG, jRequest, jResponse);
            jRequest = jResponse;
            jRequest[CPROC_HUE_PARAMS] = hue;
            viv_private_ioctl(IF_CPROC_S_CFG, jRequest, jResponse);
        } else {
            strError = "input error.";
        }
    }
};

struct PageCprocCoefficient : BasePage {
    using BasePage::BasePage;
    void dump() {
        printf("please input coefficient index 1/2\n");
        printf("index 1: 0.257812, 0.5,      0.101562, -0.148438, -0.289062, 0.4375, 0.4375, -0.367188, -0.070312\n");
        printf("index 2: 0.296875, 0.585938, 0.117188, -0.171875, -0.328125, 0.5,    0.5,    -0.421875, -0.078125\n:");
    }
    void process(std::string& str) {
        Json::Value jRequest, jResponse;
        int index = atoi(str.c_str());
        if (index > 0 && index < 3) {
            jRequest[CPROC_INDEX_PARAMS] = index;
            viv_private_ioctl(IF_CPROC_S_COEFF, jRequest, jResponse);
        }
    }
};

struct PageCproc : BasePage {
    using BasePage::BasePage;
    PageCproc(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionCprocEnable(this, "CPROC ON/OFF"));
        insert(new PageCprocAdjustBrightness(this, "Adjust brightness"));
        insert(new PageCprocAdjustContrast(this, "Adjust contrast"));
        insert(new PageCprocAdjustSaturation(this, "Adjust saturation"));
        insert(new PageCprocAdjustHue(this, "Adjust HUE"));
        insert(new PageCprocCoefficient(this, "CC coefficient"));
    }
};

/*************************DPCC**************************/
struct OptionDpccEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "dpcc enabled" : "dpcc disabled";
        Json::Value jRequest, jResponse;
        jRequest[DPCC_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_DPCC_S_EN, jRequest, jResponse);
    }
};

struct PageDpcc : BasePage {
    using BasePage::BasePage;
    PageDpcc(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionDpccEnable(this, "DPCC ON/OFF"));
    }
};

/*************************DPF**************************/
struct OptionDpfEnable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "dpf enabled" : "dpf disabled";
        Json::Value jRequest, jResponse;
        jRequest[DPF_ENABLE_PARAMS] = enable;
        viv_private_ioctl(IF_DPF_S_EN, jRequest, jResponse);
    }
};

struct PageDpf : BasePage {
    using BasePage::BasePage;
    PageDpf(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionDpfEnable(this, "DPF ON/OFF"));
    }
};

/*************************EE**************************/


/*************************DNR2**************************/


/*************************DNR3**************************/


/*************************WDR3**************************/
struct OptionWdr3Enable : BaseOption {
    using BaseOption::BaseOption;
    virtual void onSelected() {
        enable = !enable;
        strError = enable ? "wdr3 enabled" : "wdr3 disabled";
        Json::Value jRequest, jResponse;
        jRequest[WDR_ENABLE_PARAMS] = enable;
        jRequest[WDR_GENERATION_PARAMS] = 2; // wdr3
        viv_private_ioctl(IF_WDR_S_EN, jRequest, jResponse);
    }
};

struct OptionWdr3AutoCtrl : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;

        jRequest[WDR_GENERATION_PARAMS] = 2; // WDR3
        viv_private_ioctl(IF_WDR_G_CFG, jRequest, jResponse);
        printf("wdr3 Auto/Manual is %d\n", jResponse[WDR_AUTO_PARAMS].asBool());
        printf("please input  Auto:1 Manual:0\n");
    }

    virtual void process(std::string& str) override {
        Json::Value jRequest, jResponse;

        jRequest[WDR_GENERATION_PARAMS] = 2;
        viv_private_ioctl(IF_WDR_G_EN, jRequest, jResponse);
        if (jResponse[WDR_ENABLE_PARAMS].asBool()) {
            strError = "switch wdr auto/manual need in disable status\n";
            return;
        }

        jRequest.clear();
        jResponse.clear();
        jRequest[WDR_GENERATION_PARAMS] = 2; // WDR3
        viv_private_ioctl(IF_WDR_G_CFG, jRequest, jResponse);

        jRequest.clear();
        jRequest[WDR_GENERATION_PARAMS] = 2;
        jRequest[WDR_AUTO_PARAMS]       = (atoi(str.c_str()) ? true : false);
        jRequest[WDR_AUTO_LEVEL_PARAMS] = jResponse[WDR_AUTO_LEVEL_PARAMS].asInt();
        jRequest[WDR_STRENGTH_PARAMS]   = jResponse[WDR_STRENGTH_PARAMS].asInt();;
        jRequest[WDR_GAIN_MAX_PARAMS]   = jResponse[WDR_GAIN_MAX_PARAMS].asInt();;
        jRequest[WDR_STRENGTH_GLOBAL_PARAMS] = jResponse[WDR_STRENGTH_GLOBAL_PARAMS].asInt();
        jResponse.clear();
        viv_private_ioctl(IF_WDR_S_CFG, jRequest, jResponse);
    }
};

struct PageWdr3Strength : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;
        jRequest[WDR_GENERATION_PARAMS] = 2; // WDR3
        viv_private_ioctl(IF_WDR_G_CFG, jRequest, jResponse);

        printf("wdr3 params:[%d, %d, %d]\n", jResponse[WDR_STRENGTH_PARAMS].asInt(),
            jResponse[WDR_GAIN_MAX_PARAMS].asInt(), jResponse[WDR_STRENGTH_GLOBAL_PARAMS].asInt());
        printf("please input wdr3 strength:[strength, maxGain, globalStrength](0-128):\n");
    }

    virtual void process(std::string &str) override {
        std::vector<int> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 3) {
            strError = "Input Error.";
        } else {
            for (int i = 0; i < 3; i++) {
                if (params[i] < 0 || params[i] > 128) {
                    strError = "Input Error.";
                    return;
                }
            }
            Json::Value jRequest, jResponse;
            jRequest[WDR_GENERATION_PARAMS] = 2; // WDR3
            viv_private_ioctl(IF_WDR_G_CFG, jRequest, jResponse);

            jRequest.clear();
            jRequest[WDR_GENERATION_PARAMS] = 2;
            jRequest[WDR_AUTO_PARAMS]       = jResponse[WDR_AUTO_PARAMS].asBool();
            jRequest[WDR_AUTO_LEVEL_PARAMS] = jResponse[WDR_AUTO_LEVEL_PARAMS].asInt();
            jRequest[WDR_STRENGTH_PARAMS]   = params[0];
            jRequest[WDR_GAIN_MAX_PARAMS]   = params[1];
            jRequest[WDR_STRENGTH_GLOBAL_PARAMS] = params[2];
            jResponse.clear();

            viv_private_ioctl(IF_WDR_S_CFG, jRequest, jResponse);
        }
    }
};

struct PageWdr3 : BasePage {
    using BasePage::BasePage;
    PageWdr3(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new OptionWdr3Enable(this, "WDR3 ON/OFF"));
        insert(new OptionWdr3AutoCtrl(this, "WDR3 Auto/Manual"));
        insert(new PageWdr3Strength(this, "WDR3 STRENGTH INPUT"));
    }
};

/*************************HDR**************************/
struct PageHdr : BasePage {
    using BasePage::BasePage;
    void dump() override {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_HDR_G_CFG, jRequest, jResponse);

        printf("hdr params(extBit, ratio):[%d, %f]\n", 
                jResponse[HDR_EXTENSION_BIT_PARAMS].asUInt(), jResponse[HDR_EXPOSURE_RATIO_PARAMS].asFloat());
        printf("please input extension bit[0-4], hdr ratio[1.0-64.0]:\n");
    }
    void process(std::string &str) {
        std::vector<int> params;
        Splitter::exec(str, ",", params);
        if (params.size() < 2) {
            strError = "Input Error.";
        } else {
            if (params[0] < 0 || params[0] > 4) {
                strError = "Input Error.";
                return;
            }
            if (params[1] < 1 || params[1] > 64) {
                strError = "Input Error.";
                return;
            }
            Json::Value jRequest, jResponse;
            jRequest[HDR_EXTENSION_BIT_PARAMS] = params[0];
            jRequest[HDR_EXPOSURE_RATIO_PARAMS] = params[1];
            viv_private_ioctl(IF_HDR_S_CFG, jRequest, jResponse);
        }
    }
};

/*************************MAIN PAGE**************************/
// holder when one page is not enough.
struct PageMain3 : BasePage {
    PageMain3(IBasePage* page, const char* szName) : BasePage(page, szName) {
      insert(new PageRegGet(this,"REGGET"));
      insert(new PageRegSet(this,"REGSET"));
      insert(new OptionPreload(this,"PRELOAD"));
    }
};

struct PageMain2 : BasePage {
    PageMain2(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new PageDemosaic(this, "DEMOSAIC"));
        insert(new PageFilter(this, "FILTER"));
        insert(new PageCac(this, "CAC"));
        insert(new PageCnr(this, "CNR"));
        insert(new PageDpcc(this, "DPCC"));
        insert(new PageDpf(this, "DPF"));
        insert(new PageWdr3(this, "WDR3"));
        insert(new PageHdr(this, "HDR"));
        insert(new PageMain3(this, "NEXT->"));
    }
};

struct PageMain : BasePage {
    PageMain(IBasePage* page, const char* szName) : BasePage(page, szName) {
        insert(new PagePipeline(this, "PIPELINE"));
        insert(new OptionCapture(this, "CAPTURE"));
        insert(new PageDwe(this, "DWE"));
        insert(new PageFps(this, "FPS"));
        insert(new PageAec(this, "AEC"));
        insert(new PageAwb(this, "AWB"));
        insert(new PageAf(this, "AF"));
        insert(new PageBls(this, "BLS"));
        insert(new PageLsc(this, "LSC"));
        insert(new PageCproc(this, "CPROC"));
        insert(new PageGamma(this, "GAMMA"));
        insert(new PageMain2(this, "NEXT->"));
    }
};

/*************************MAIN LOOP**************************/
void loop() {
    while (1) {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_DWE_G_PARAMS, jRequest, jResponse);
        parseDewarpParams(jResponse["dwe"]);
        if ( system("clear\n") < 0 ) {
            ALOGE("can't clear screen");
            break;
        }
#if 0
        printf("dewarp:  mode %d, hflip %d, vflip %d, bypass %d\n", dweParams.mode, dweParams.hflip, dweParams.vflip, dweParams.bypass);
        printf("   camera_matrix :  ");
        for (int i = 0; i < 17; i++) {
            printf("%.3f, ", dweParams.mat[i]);
        }
        printf("\n");
#endif
        printf("\n");
        pCurrentPage->dump();
        if (!strError.empty())
            ALOGW("%s", strError.c_str());
        strError.clear();
        std::string inputString;
        std::getline(std::cin, inputString);
        if (inputString.empty()) {
            pCurrentPage->navigateBack();
            continue;
        }
        pCurrentPage->process(inputString);
    }
}

static int is_non_digit(const char* str)
{
    const char* temp = str;

    while (temp != NULL && *temp != '\0') {
        if (*temp < '0' || *temp > '9') {
            return 1;
        }
        temp++;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        ALOGE("usage: ./vvext videoid");
        ALOGE("OR");
        ALOGE("usage: ./vvext videoid 'windowname' 'param string' ");
        return 1;
    }

    if (is_non_digit(argv[1])) {
        ALOGE("wrong videoid data type");
        return 1;
    }

    int videoid = atoi(argv[1]);
    char szFile[256] = {0};
    sprintf(szFile, "/dev/video%d", videoid);
    fd = ::open(szFile, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        ALOGE("can't open video file %s", szFile);
        return 1;
    }
    v4l2_capability caps;
    int result = ::ioctl(fd, VIDIOC_QUERYCAP, &caps);
    if (result  < 0) {
        ALOGE("failed to get device caps for %s (%d = %s)", szFile, errno, strerror(errno));
        return 1;
    }

    ALOGI("Open Device: %s (fd=%d)", szFile, fd);
    ALOGI("  Driver: %s", caps.driver);

    if (strcmp((const char*)caps.driver, "viv_v4l2_device") == 0) {
        ALOGI("found viv video dev %s", szFile);
        int streamid = -1;
        ::ioctl(fd, VIV_VIDIOC_S_STREAMID, &streamid);
    } else {
        ALOGE("Open wrong type of viv video dev");
        return 1;
    }

    PageMain pageMain(NULL, "MAIN PAGE");

    if (argc == 4) {
        Json::Value jRequest, jResponse;
        viv_private_ioctl(IF_DWE_G_PARAMS, jRequest, jResponse);
        parseDewarpParams(jResponse["dwe"]);
        /* command mode  */
        ALOGI("get command %s %s", argv[2], argv[3]);
        std::string strParams = argv[3];
        pCurrentPage = pageMain.search(argv[2]);
        if (!pCurrentPage) {
            ALOGE("failed to search window: %s", argv[2]);
            goto end;
        }

        if (pCurrentPage->isPage) {
            pCurrentPage->process(strParams);
        } else {
            BaseOption* pOption = (BaseOption*)pCurrentPage;
            pOption->enable = ! atoi(strParams.c_str());
            pOption->onSelected();
        }
    } else{
        /* Window mode */
        pCurrentPage = (IBasePage*)&pageMain;
        std::thread dt(loop);
        sig.wait();
    }

end:
    close(fd);
    return 0;
}
