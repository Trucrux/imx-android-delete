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
#include <linux/videodev2.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/time.h>
#ifdef IMX_G2D
# include <imx/linux/dma-buf.h>
#endif

#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <MediaBuffer.h>
#include <linux/fb.h>

#include <json_helper.h>

#include "log.h"
#include "viv_video_kevent.h"

#define LOGTAG "VIDEOTEST"

#include "IDisplay.h"

std::string strDeviceName;

static std::unordered_map<std::string, int> string2MediaFormat = {
    {"YUYV", MEDIA_PIX_FMT_YUV422I},
    {"NV12", MEDIA_PIX_FMT_YUV420SP},
    {"NV16", MEDIA_PIX_FMT_YUV422SP},
    {"RAW8", MEDIA_PIX_FMT_RAW8},
    {"RAW10", MEDIA_PIX_FMT_RAW10},
    {"RAW12", MEDIA_PIX_FMT_RAW12},
};

static std::unordered_map<int, int> mediaFormat2V4l2Format = {
    {MEDIA_PIX_FMT_YUV422I, V4L2_PIX_FMT_YUYV},
    {MEDIA_PIX_FMT_YUV420SP, V4L2_PIX_FMT_NV12},
    {MEDIA_PIX_FMT_YUV422SP, V4L2_PIX_FMT_NV16},
};

static std::unordered_map<std::string, int> ctrlIDList = {
    {"sensor.mode", 0},
    {"sensor.resw", 0},
    {"sensor.resh", 0},
    /* To be added */
};

static struct v4l2_query_ext_ctrl vivExtQctrl; /* query for viv_ext_ctrl */

static void updateCtrlIDList(int fd) {
    struct v4l2_query_ext_ctrl qctrl;
    int id = 0;
    do {
        memset(&qctrl, 0xff, sizeof(qctrl));
        qctrl.id = id |
                V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;

        if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) < 0) {
            ALOGD("VIDIOC_QUERY_EXT_CTRL: %s", strerror(errno));
            break;
        } else if (qctrl.type == V4L2_CTRL_TYPE_STRING) {
            ALOGD("V4L2 ext ctrl id: 0x%x", qctrl.id);
            ALOGD("V4L2 ext ctrl name: %s", qctrl.name);
            ALOGD("V4L2 ext ctrl elem size: %d", qctrl.elem_size);
            memcpy(&vivExtQctrl, &qctrl, sizeof(qctrl));
        } else {
            if (ctrlIDList.find(qctrl.name) != ctrlIDList.end()) {
                ctrlIDList[qctrl.name] = qctrl.id;
                ALOGD("V4L2 ext ctrl id: 0x%x", qctrl.id);
                ALOGD("V4L2 ext ctrl name: %s", qctrl.name);
                ALOGD("V4L2 ext ctrl elem size: %d", qctrl.elem_size);
            }
        }
        id = qctrl.id;
    } while (true);
}

static int g_ctrl_id = -1;
#define VIV_CTRL_NAME "viv_ext_ctrl"
static int openDevice(int id) {
    int fd;
    char szFile[64];
    sprintf(szFile, "/dev/video%d", id);
    fd = ::open(szFile, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        ALOGE("can't open video file %s", szFile);
        return -1;
    }
    v4l2_capability caps;
    int result = ioctl(fd, VIDIOC_QUERYCAP, &caps);
    if (result  < 0) {
        ALOGE("failed to get device caps for %s (%d = %s)", szFile, errno, strerror(errno));
        close(fd);
        return -1;
    }

    ALOGI("Open Device: %s (fd=%d)", szFile, fd);
    ALOGI("  Driver: %s", caps.driver);

    if (strcmp((const char*)caps.driver, "viv_v4l2_device")) {
        close(fd);
        return -1;
    }

    ALOGI("found viv video dev %s", szFile);
    strDeviceName = szFile;

    // get viv ctrl id by it's name "viv_ext_ctrl"
    struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof(queryctrl));

    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
            continue;

        ALOGI("%s Control %s", __func__, queryctrl.name);
        if (strcmp((char *)queryctrl.name, VIV_CTRL_NAME) == 0) {
            g_ctrl_id = queryctrl.id;
            ALOGI("%s, find viv ctrl id 0x%x", __func__, g_ctrl_id);
            break;
        }

        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    return fd;
}

static int openCamera(int id) {
    int fd;
    if (id < 0) {
        for (int i = 0; i < 20; i++) {
            if ((fd = openDevice(i)) >= 0)
                break;
        }
    } else
        fd = openDevice(id);
    return fd;
}

static void dump_args(const char *test) {
    std::stringstream ss;
    auto it = string2MediaFormat.cbegin();
    if (!string2MediaFormat.empty()) {
        ss << it->first;
        for (++it; it != string2MediaFormat.cend(); ++it)
            ss << "/" << it->first;
    } else
        return;
    std::cout << "Usage: " << test << " [option]\n";
    std::cout << "  -w  output frame width.\n";
    std::cout << "  -h  output frame height.\n";
    std::cout << "  -r  output frame fps.\n";
    std::cout << "  -f  output frame format (" << ss.str() << ").\n";
    std::cout << "  -t  output type, 0(dryrun)/1(drm)/2(file)/3(framebuffer)/4(wayland).\n";
    std::cout << "  -m  sensor mode index (-m0 or -m 0).\n";
    std::cout << "  -d  driver device id (-d0 or -d 0, will auto-search by default).\n";
    std::cout << "  -s  enable scale\n";
    std::cout << "  -c  enable crop\n";
    std::cout << "e.g.\n";
    std::cout << "Display: " << test << " -w 1920 -h 1080 -f YUYV -t drm -m 0\n";
    std::cout << "         " << test << " -w 1920 -h 1080 -f YUYV -t drm -m 0 -d 2\n";
    std::cout << "Capture raw: The width and height must set to sensor mode width and height.\n";
    std::cout << "         Not support crop and scale\n";
    std::cout << "         Take mode0 represents the sensor output resolution is 1080P as an example\n";
    std::cout << "         " << test << " -w 1920 -h 1080 -f RAW12 -t file -m 0 -d 2\n";
    std::cout << "         " << test << " -w 1920 -h 1080 -f RAW12 -t 2 -m 0 -d 2\n";
    std::cout << "Crop test:\n";
    std::cout << "         Take mode0 represents the sensor crop output resolution wxh is 1280x720 with start point (x,y) is (0,0) as an example\n";
    std::cout << "         " << test << " -w 1280 -h 720 -f YUYV -t drm -m 0 -d 2 -c 0_0_1280_720\n";
    std::cout << "Scale test:\n";
    std::cout << "         Take mode0 represents the sensor scale output resolution wxh is 3840x2160 as an example\n";
    std::cout << "         " << test << " -w 3840 -h 2160 -f YUYV -t drm -m 0 -d 2 -s\n";
    std::cout << "Crop then Scale test:\n";
    std::cout << "         Take mode0 represents the sensor crop wxh is 1280x720 with start point (x,y) is (0,0),then scale wxh is 1920x1080 as an example\n";
    std::cout << "         " << test << " -w 1920 -h 1080 -f YUYV -t drm -m 0 -d 2 -c 0_0_1280_720 -s\n";
}

static inline int parseOutputType(const char *arg) {
    int type = -1;
    if (*arg >= '0' && *arg <= '9')
        type = atoi(arg);
    if (type < 0) {
        if (strcmp(arg, "dryrun") == 0)
            type = 0;
        else if (strcmp(arg, "drm") == 0)
            type = 1;
        else if (strcmp(arg, "file") == 0)
            type = 2;
        else if (strcmp(arg, "framebuffer") == 0)
            type = 3;
        else if (strcmp(arg, "wayland") == 0)
            type = 4;
    }
    return type;
}

static void stop(int sig);
static int camDevice = -1;

#define DWE_OFF (char *)"{<id>:<pipeline.s.dwe.onoff>;<enable>: false}"

int setFeature(const char *value)
{
    int ret = 0;
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl;

    if(value == NULL)
        return -1;

    if ((camDevice <= 0) || (g_ctrl_id <= 0))
        return -1;

    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = g_ctrl_id;
    ctrl.size = strlen (value) + 1;
    ctrl.string = strdup(value);

    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.which = V4L2_CTRL_ID2WHICH(ctrl.id);
    ctrls.count = 1;
    ctrls.controls = &ctrl;

    ALOGI("setFeature, fd %d, id 0x%x, str %s", camDevice, g_ctrl_id, value);

    ret = ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ctrls);
    ALOGI("setFeature, ret %d", ret);
    if(ret < 0)
        ALOGE("%s VIDIOC_S_EXT_CTRLS failed, value %s, errno %d, %s",
            __func__, value, errno, strerror(errno));

    free(ctrl.string);

    return ret;
}

#define US_PER_SEC      1000000
#define STAT_ITVL_US    (30 * US_PER_SEC)
static void fpsStat() {
    uint64_t curUS;
    static uint64_t preUS;
    struct timeval val = {0};
    uint64_t itvl = 0;
    float fps;
    static uint64_t frames;

    frames++;
    gettimeofday(&val, NULL);
    curUS = val.tv_sec * US_PER_SEC + val.tv_usec;
    itvl = curUS - preUS;

    if(itvl >= STAT_ITVL_US) {
        fps = (float)(frames * US_PER_SEC) / itvl;
        if(preUS > 0)
            ALOGI("######## fps %.2f ########\n", fps);

        preUS = curUS;
        frames = 0;
    }

    return;
}

IDisplay* pDisplay = NULL;
int main(int argc, char* argv[]) {
    int ch;
    int width = 0;
    int height = 0;
    int displayType = -1;
    int deviceId = -1;
    int sensorMode = -1;
    char szFormat[16] = {0};
    int crop_enable = 0;
    int scale_enable = 0;
    int crop_left = 0;
    int crop_top = 0;
    int crop_width = 0;
    int crop_height = 0;
    char* crop_token = NULL;
    int isRawData = 0;
    float gain = 0;
    float exp = 0;
    int stability_count = 0;
    int fps = 0;

    if (argc == 1) {
        dump_args(argv[0]);
        return -1;
    }

    while ((ch = getopt(argc, argv, "w:h:r:f:t:m:d:c:s::")) != -1) {
        switch (ch) {
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'r':
            fps = atoi(optarg);
            break;
        case 'f':
            strncpy(szFormat, optarg, 15);
            break;
        case 't':
            displayType = parseOutputType(optarg);
            break;
        case 'm':
            if(optarg != NULL)
            {
                sensorMode = atoi(optarg);
            }
            else
            {
                std::cout << "input format of -m is error!\n";
                dump_args(argv[0]);
                return 0;
            }
            break;
        case 'd':
            if(optarg != NULL)
            {
                deviceId = atoi(optarg);
            }
            else
            {
                std::cout << "input format of -d is error!\n";
                dump_args(argv[0]);
                return 0;
            }
            break;
        case 's':
            scale_enable = 1;
            break;
        case 'c':
            crop_enable = 1;
            if (optarg != NULL) {
                crop_token = strtok(optarg, "_");
                crop_left = atoi(crop_token);
                crop_token = strtok(NULL, "_");
                crop_top = atoi(crop_token);
                crop_token = strtok(NULL, "_");
                crop_width = atoi(crop_token);
                crop_token = strtok(NULL, "_");
                crop_height = atoi(crop_token);
                if (crop_left < 0 || crop_top < 0 || crop_width <= 0 || crop_height <= 0) {
                    ALOGE("invalid crop from (%d, %d), size (wxh): %dx%d", crop_left, crop_top, crop_width, crop_height);
                    dump_args(argv[0]);
                    return -1;
                }
            }
            else
            {
                std::cout << "input format of -c is error!\n";
                dump_args(argv[0]);
                return 0;
            }
            break;
        default:
            dump_args(argv[0]);
            return 0;
        }
    }

    if (width <= 0 || height <= 0) {
        ALOGE("invalid frame size (wxh): %dx%d", width, height);
        dump_args(argv[0]);
        return -1;
    }

    if (string2MediaFormat.find(szFormat) == string2MediaFormat.end()) {
        ALOGE("unsupported format: %s",
                strlen(szFormat) > 0 ? szFormat : "(null)");
        dump_args(argv[0]);
        return -1;
    }

    if (displayType < 0 || !(pDisplay = IDisplay::createObject(displayType))) {
        ALOGE("unsupported outputtype: %d", displayType);
        dump_args(argv[0]);
        return -1;
    }

    signal(SIGINT, stop);

    camDevice = openCamera(deviceId);
    ALOGI("open camdev ret : %d", camDevice);
    if (camDevice < 0) {
        ALOGE("failed to open camera device (%d)!", deviceId);
        dump_args(argv[0]);
        return -1;
    }

    v4l2_capability caps;
    {
        int result = ioctl(camDevice, VIDIOC_QUERYCAP, &caps);
        if (result  < 0) {
            ALOGE("failed to get device caps for %s (%d = %s)", strDeviceName.c_str(), errno, strerror(errno));
            return -1;
        }
    }

    // Report device properties
    ALOGI("Open Device: %s (fd=%d)", strDeviceName.c_str(), camDevice);
    ALOGI("  Driver: %s", caps.driver);
    ALOGI("  Card: %s", caps.card);
    ALOGI("  Version: %u.%u.%u",
            (caps.version >> 16) & 0xFF,
            (caps.version >> 8)  & 0xFF,
            (caps.version)       & 0xFF);
    ALOGI("  All Caps: %08X", caps.capabilities);
    ALOGI("  Dev Caps: %08X", caps.device_caps);

    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(caps.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("Streaming capture not supported by %s.", strDeviceName.c_str());
        return -1;
    }

    struct viv_caps_supports caps_supports;
    int ret, i = 0;
    do
    {
        memset(&caps_supports,0,sizeof(caps_supports));
        ret = ioctl(camDevice, VIV_VIDIOC_GET_CAPS_SUPPORTS, &caps_supports);
        i++;
    }while(ret && i < 2);
    if (ret == 0) {
        printf("Video Test:caps supports:{\n");
        printf("\tcount = %d\n",caps_supports.count);
        for(unsigned int i=0; i<caps_supports.count; i++) {
            printf("\t{\n");
            printf("\tindex            = %d\n",caps_supports.mode[i].index);
            printf("\tbounds_width     = %d\n",caps_supports.mode[i].bounds_width);
            printf("\tbounds_height    = %d\n",caps_supports.mode[i].bounds_height);
            printf("\ttop              = %d\n",caps_supports.mode[i].top);
            printf("\tleft             = %d\n",caps_supports.mode[i].left);
            printf("\twidth            = %d\n",caps_supports.mode[i].width);
            printf("\theight           = %d\n",caps_supports.mode[i].height);
            printf("\thdr_mode         = %d\n",caps_supports.mode[i].hdr_mode);
            printf("\tstitching_mode   = %d\n",caps_supports.mode[i].stitching_mode);
            printf("\tbit_width        = %d\n",caps_supports.mode[i].bit_width);
            printf("\tbayer_pattern    = %d\n",caps_supports.mode[i].bayer_pattern);
            printf("\tfps              = %d\n",caps_supports.mode[i].fps);
            printf("\t}\n");
        }
        printf("}\n");
    } else {
        ALOGE("Video Test:Get Caps Supports Failed[%d]\n",ret);
        return -1;
    }

    if (sensorMode >= 0) {
        struct viv_caps_mode_s caps_mode;
        memset(&caps_mode,0,sizeof(caps_mode));
        caps_mode.mode = sensorMode;
        if (ioctl(camDevice, VIV_VIDIOC_S_CAPS_MODE, &caps_mode) == 0) {
            ALOGI("Video Test: Set mode[%d]\n",caps_mode.mode);
        } else {
            ALOGE("Video Test: Set mode[%d] Failed\n",caps_mode.mode);
            return -1;
        }
    }


    v4l2_fmtdesc formatDescriptions;
    formatDescriptions.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (int i=0; true; i++) {
        formatDescriptions.index = i;
        if (ioctl(camDevice, VIDIOC_ENUM_FMT, &formatDescriptions) == 0) {
            ALOGI("  %2d: %s 0x%08X 0x%X",
                   i,
                   formatDescriptions.description,
                   formatDescriptions.pixelformat,
                   formatDescriptions.flags
            );
            switch(formatDescriptions.pixelformat)
            {
                case V4L2_PIX_FMT_SBGGR8:
                case V4L2_PIX_FMT_SGBRG8:
                case V4L2_PIX_FMT_SGRBG8:
                case V4L2_PIX_FMT_SRGGB8:
                    mediaFormat2V4l2Format[MEDIA_PIX_FMT_RAW8] = formatDescriptions.pixelformat;
                    break;
                case V4L2_PIX_FMT_SBGGR10:
                case V4L2_PIX_FMT_SGBRG10:
                case V4L2_PIX_FMT_SGRBG10:
                case V4L2_PIX_FMT_SRGGB10:
                    mediaFormat2V4l2Format[MEDIA_PIX_FMT_RAW10] = formatDescriptions.pixelformat;
                    break;
                case V4L2_PIX_FMT_SBGGR12:
                case V4L2_PIX_FMT_SGBRG12:
                case V4L2_PIX_FMT_SGRBG12:
                case V4L2_PIX_FMT_SRGGB12:
                    mediaFormat2V4l2Format[MEDIA_PIX_FMT_RAW12] = formatDescriptions.pixelformat;
                    break;
                case V4L2_PIX_FMT_NV16:
                    mediaFormat2V4l2Format[MEDIA_PIX_FMT_YUV422SP] = formatDescriptions.pixelformat;
                    break;
                case V4L2_PIX_FMT_YUYV:
                    mediaFormat2V4l2Format[MEDIA_PIX_FMT_YUV422I] = formatDescriptions.pixelformat;
                    break;
                case V4L2_PIX_FMT_NV12:
                    mediaFormat2V4l2Format[MEDIA_PIX_FMT_YUV420SP] = formatDescriptions.pixelformat;
                    break;
            }

        } else {
            break;
        }
    }

    ALOGI("\nEnum Format[%s]Framesize:",szFormat);
    bool isSupportFrameSize = false;
    for (int i = 0; true; i++) {
        struct v4l2_frmsizeenum fsize;
        memset(&fsize, 0, sizeof(fsize));
        fsize.index = i;
        fsize.pixel_format = mediaFormat2V4l2Format[string2MediaFormat[szFormat]];

        if (ioctl(camDevice, VIDIOC_ENUM_FRAMESIZES, &fsize) == 0) {
            if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                ALOGI("Enum Discreate FrameSize:"
                    "width:%u height:%u\n",
                    fsize.discrete.width, fsize.discrete.height);
                if (fsize.discrete.width == (u32)width &&
                    fsize.discrete.height == (u32)height) {
                    isSupportFrameSize = true;
                }
            } else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                ALOGI("Enum Continus FrameSize:"
                    "min{width:%u height:%u}"
                    "max{width:%u height:%u}\n",
                    fsize.stepwise.min_width,fsize.stepwise.min_height,
                    fsize.stepwise.max_width,fsize.stepwise.max_height);
                if (((u32)width >= fsize.stepwise.min_width) &&
                    ((u32)width <= fsize.stepwise.max_width) &&
                    ((u32)height >= fsize.stepwise.min_height) &&
                    ((u32)height <= fsize.stepwise.max_height)) {
                    isSupportFrameSize = true;
                }
            } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                ALOGI("Enum StepWise FrameSize:"
                    "min{width:%u height:%u}"
                    "max{width:%u height:%u}"
                    "stepsize{width:%u height:%u}\n",
                    fsize.stepwise.min_width,fsize.stepwise.min_height,
                    fsize.stepwise.max_width,fsize.stepwise.max_height,
                    fsize.stepwise.step_width,fsize.stepwise.step_height);
                if (((u32)width >= fsize.stepwise.min_width) &&
                    ((u32)width <= fsize.stepwise.max_width) &&
                    ((u32)height >= fsize.stepwise.min_height) &&
                    ((u32)height <= fsize.stepwise.max_height) &&
                    (((u32)width - fsize.stepwise.min_width) % fsize.stepwise.step_width) == 0 &&
                    (((u32)height - fsize.stepwise.min_height) % fsize.stepwise.step_height) == 0) {
                    isSupportFrameSize = true;
                }
            }
        } else {
            break;
        }
    }
    if (isSupportFrameSize == false) {
        ALOGE("Not Support format:%s {width:%d height:%d} \n",szFormat,width,height);
        return -1;
    }

    if (crop_enable)
    {
        struct v4l2_selection sel;
        memset(&sel,0,sizeof(sel));
        sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        sel.target = V4L2_SEL_TGT_CROP_BOUNDS;
        ret = ioctl(camDevice, VIDIOC_G_SELECTION, &sel);
        if (ret)
        {
            ALOGE("Video Test: Get Crop Bounds Failed\n");
            return -1;
        }
        ALOGI("Video Test: Get Crop Bounds from (%d, %d), size: %dx%d\n", sel.r.left, sel.r.top, sel.r.width,sel.r.height);

        sel.target = V4L2_SEL_TGT_CROP_DEFAULT;
        ret = ioctl(camDevice, VIDIOC_G_SELECTION, &sel);
        if (ret)
        {
            ALOGE("Video Test: Get Default Crop Size Failed\n");
            return -1;
        }
        ALOGI("Video Test: Get Default Crop from (%d, %d), size: %dx%d\n", sel.r.left, sel.r.top, sel.r.width,sel.r.height);

        sel.target = V4L2_SEL_TGT_CROP;
        if (crop_left >= 0 && crop_top >= 0 && crop_width > 0 && crop_height > 0 &&
            (unsigned int)(crop_left + crop_width) <= sel.r.width &&
            (unsigned int)(crop_top + crop_height) <= sel.r.height) {
            sel.r.left = crop_left;
            sel.r.top = crop_top;
            sel.r.height = crop_height;
            sel.r.width = crop_width;
            ALOGI("Video Test: Set Crop from (%d, %d), Size %dx%d\n", crop_left, crop_top, crop_width, crop_height);
        } else if (crop_width == 0 && crop_height == 0){
            sel.r.left = 0;
            sel.r.top = 0;
            sel.r.height = height;
            sel.r.width = width;
            ALOGI("Video Test: Set Crop from (0, 0), Size %dx%d\n", width, height);
        } else {
            ALOGE("Not Support crop from (%d, %d), size: {crop_width:%d crop_height:%d} \n", crop_left, crop_top, crop_width, crop_height);
            return -1;
        }

        ALOGI("==== VIDIOC_S_SELECTION, crop %dx%d", sel.r.width, sel.r.height);
        ret = ioctl(camDevice, VIDIOC_S_SELECTION, &sel);
        if (ret)
        {
            ALOGE("Video Test: Set Set Crop Size Failed %d\n",ret);
            return -1;
        }
    }

    if (scale_enable)
    {
        struct v4l2_selection sel;
        memset(&sel,0,sizeof(sel));
        sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        sel.target = V4L2_SEL_TGT_COMPOSE_DEFAULT;

        ret = ioctl(camDevice, VIDIOC_G_SELECTION, &sel);
        if (ret)
        {
            ALOGE("Video Test: Get Default Scale Size Failed\n");
            return -1;
        }

        ALOGI("Video Test: Get Default Scale Size %dX%d\n",sel.r.width,sel.r.height);
        ALOGI("Video Test: Set Scale Size %dX%d\n",width,height);

        sel.target = V4L2_SEL_TGT_COMPOSE;
        sel.r.top = 0;
        sel.r.left = 0;
        sel.r.height = height;
        sel.r.width = width;
        ret = ioctl(camDevice, VIDIOC_S_SELECTION, &sel);
        if (ret)
        {
            ALOGE("Video Test:Set Scale Size Failed %d\n",ret);
            return -1;
        }
    }

    // disable dwe if capture raw format
    if ((string2MediaFormat[szFormat] == MEDIA_PIX_FMT_RAW8) ||
        (string2MediaFormat[szFormat] == MEDIA_PIX_FMT_RAW10) ||
        (string2MediaFormat[szFormat] == MEDIA_PIX_FMT_RAW12)) {
       setFeature(DWE_OFF);
       isRawData = 1;
    }

    struct v4l2_streamparm param;
    memset(&param, 0, sizeof(param));
    param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    param.parm.capture.timeperframe.numerator = 1;
    param.parm.capture.timeperframe.denominator = fps;
    ALOGI("==== VIDIOC_S_PARM, fps %d\n", fps);
    ret = ioctl(camDevice, VIDIOC_S_PARM, &param);
    if (ret < 0) {
        ALOGW("%s: VIDIOC_S_PARM Failed: %s, fps %d", __func__, strerror(errno), fps);
    }

    unsigned int pixelFormat = isRawData ? V4L2_PIX_FMT_YUYV : mediaFormat2V4l2Format[string2MediaFormat[szFormat]];
    v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = pixelFormat;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;

    ALOGI("==== VIDIOC_S_FMT %dx%d, 0x%x", width, height, pixelFormat);
    if (ioctl(camDevice, VIDIOC_S_FMT, &format) < 0) {
        ALOGE("VIDIOC_S_FMT: %s", strerror(errno));
        return -1;
    }

    updateCtrlIDList(camDevice);

    /* Sample for querying sensor caps */
    while (vivExtQctrl.id > 0) {
        struct v4l2_ext_controls ectrls;
        struct v4l2_ext_control ectrl;
        memset(&ectrls, 0, sizeof(ectrls));
        memset(&ectrl, 0, sizeof(ectrl));
        ectrl.string = new char [vivExtQctrl.elem_size];
        if (!ectrl.string)
            break;
        ectrl.id = vivExtQctrl.id;
        ectrl.size = vivExtQctrl.elem_size;
        ectrls.controls = &ectrl;
        ectrls.count = 1;

        /* JSON format string request */
        strncpy(ectrl.string, "{<id>:<sensor.query>}", vivExtQctrl.elem_size - 1);

        /* Set the control: 'sensor.query' */
        if (ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ectrls) < 0) {
            ALOGE("VIDIOC_S_EXT_CTRLS: %s", strerror(errno));
            delete [] ectrl.string;
            break;
        }

        /* Get the result */
        if (ioctl(camDevice, VIDIOC_G_EXT_CTRLS, &ectrls) < 0) {
            ALOGE("VIDIOC_G_EXT_CTRLS: %s", strerror(errno));
        }
        ALOGD("Sensor's caps (JSON format string):%s", ectrl.string);
        delete [] ectrl.string;
        break;
    }

    /* Sample for how to use G/S_CTRL */
    /*set sensor mode need updata xml file, so now use VIV_VIDIOC_S_CAPS_MODE.not use VIDIOC_S_CTRL
    if (ctrlIDList["sensor.mode"] > 0) {
        struct v4l2_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.id = ctrlIDList["sensor.mode"];
        if (ioctl(camDevice, VIDIOC_G_CTRL, &ctrl) < 0) {
            ALOGE("VIDIOC_G_CTRL: %s", strerror(errno));
        } else {
            ALOGI("Current sensor's mode: %d", ctrl.value);
        }

        if (ioctl(camDevice, VIDIOC_S_CTRL, &ctrl) < 0) {
            ALOGE("VIDIOC_S_CTRL: %s", strerror(errno));
        } else {
            ALOGI("Change sensor's mode to: %d", ctrl.value);
        }
    }*/

    /*{
        struct v4l2_ext_controls ectrls;
        struct v4l2_ext_control ectrl;
        memset(&ectrls, 0, sizeof(ectrls));
        memset(&ectrl, 0, sizeof(ectrl));
        ectrl.string = new char [vivExtQctrl.elem_size];
        ectrl.id = vivExtQctrl.id;
        ectrl.size = vivExtQctrl.elem_size;
        ectrls.controls = &ectrl;
        ectrls.count = 1;
        strncpy(ectrl.string, "{<id>:<sensor.s.sec>;<exposure>:0.01}", vivExtQctrl.elem_size - 1);
        if (ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ectrls) < 0){
            ALOGE("set ext ctrl to set ae start exposure error\n");
        }
        if (ioctl(camDevice, VIDIOC_G_EXT_CTRLS, &ectrls) < 0){
            ALOGE("get ext ctrl to set ae start exposure return error\n");
        } else {
            Json::Value jResponse;
            Json::Reader reader;
            reader.parse(ectrl.string, jResponse, true);
            if(jResponse["result"].asInt() != 0){
                ALOGE("get ext ctrl to get ae info error\n");
            }
        }
        delete [] ectrl.string;
    }*/

    {
        struct v4l2_ext_controls ectrls;
        struct v4l2_ext_control ectrl;
        memset(&ectrls, 0, sizeof(ectrls));
        memset(&ectrl, 0, sizeof(ectrl));
        ectrl.string = new char [vivExtQctrl.elem_size];
        ectrl.id = vivExtQctrl.id;
        ectrl.size = vivExtQctrl.elem_size;
        ectrls.controls = &ectrl;
        ectrls.count = 1;
        strncpy(ectrl.string, "{<id>:<awb.s.cfg>;<mode>:2;<index>:1;<damping>:1}", vivExtQctrl.elem_size - 1);
        if (ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ectrls) < 0){
            ALOGE("set ext ctrl to set awb start wb config\n");
        }
        delete [] ectrl.string;
    }

    static const int BUFFER_COUNT = 4;
    v4l2_requestbuffers bufrequest;
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = BUFFER_COUNT;
    if (ioctl(camDevice, VIDIOC_REQBUFS, &bufrequest) < 0) {
        ALOGE("VIDIOC_REQBUFS: %s", strerror(errno));
        return -1;
    }

    if (displayType == VIV_DISPLAY_TYPE_WL) {
        ret = pDisplay->connect_v4l2(camDevice);
        if (ret < 0) {
            ALOGE("wayland connect v4l2 failed");
        }
    }

    std::vector<void*> vData;
    v4l2_buffer buffer = {};
#ifdef IMX_G2D
    struct dma_buf_phys buf_addrs[BUFFER_COUNT];
    memset(&buf_addrs, 0, sizeof(buf_addrs));
#endif
    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory   = V4L2_MEMORY_MMAP;
        buffer.index    = i;
        if (ioctl(camDevice, VIDIOC_QUERYBUF, &buffer) < 0) {
            ALOGE("VIDIOC_QUERYBUF: %s", strerror(errno));
            return -1;
        }
        ALOGI("Buffer description:");
        ALOGI("  offset: %d", buffer.m.offset);
        ALOGI("  length: %d", buffer.length);
        if (ioctl(camDevice, VIDIOC_QBUF, &buffer) < 0) {
            ALOGE("VIDIOC_QBUF: %s", strerror(errno));
            return -1;
        }
#ifdef IMX_G2D
        struct v4l2_exportbuffer expbuf;
        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        expbuf.index = i;
        if (ioctl(camDevice, VIDIOC_EXPBUF, &expbuf) < 0) {
            ALOGE("VIDIOC_EXPBUF: %s", strerror(errno));
            return -1;
        }
        if (ioctl(expbuf.fd, DMA_BUF_IOCTL_PHYS, &buf_addrs[i]) < 0) {
            ALOGE("DMA_BUF_IOCTL_PHYS: %s", strerror(errno));
            return -1;
        }
#endif
        void* data = mmap(
                NULL,
                buffer.length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                camDevice,
                buffer.m.offset
        );
        if (data == MAP_FAILED) {
            ALOGE("mmap: %s", strerror(errno));
            return -1;
        }
        ALOGI("map buffer %p", data);
        vData.push_back(data);
    }

    int type = buffer.type;
    if (ioctl(camDevice, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("VIDIOC_STREAMON: %s", strerror(errno));
        return -1;
    }

    while (1) {
        fd_set fds;
        FD_ZERO (&fds);
        FD_SET (camDevice, &fds);
        select (camDevice + 1, &fds, NULL, NULL, NULL);
        if (ioctl(camDevice, VIDIOC_DQBUF, &buffer) < 0) {
            ALOGE("VIDIOC_DQBUF: %s", strerror(errno));
        } else {
            fpsStat();
            ALOGI("VIDIOC_DQBUF success");
            if (!isRawData){
#ifdef IMX_G2D
            pDisplay->showBufferExt((unsigned char*)vData[buffer.index], buf_addrs[buffer.index].phys,
                            width, height, pixelFormat, buffer.length);
#else
            pDisplay->showBuffer((unsigned char*)vData[buffer.index], width, height, pixelFormat, buffer.length);
#endif
            } else {
                struct v4l2_ext_controls ectrls;
                struct v4l2_ext_control ectrl;
                memset(&ectrls, 0, sizeof(ectrls));
                memset(&ectrl, 0, sizeof(ectrl));
                ectrl.string = new char [vivExtQctrl.elem_size];
                if (!ectrl.string)
                    break;
                ectrl.id = vivExtQctrl.id;
                ectrl.size = vivExtQctrl.elem_size;
                ectrls.controls = &ectrl;
                ectrls.count = 1;
                strncpy(ectrl.string, "{<id>:<ec.g.cfg>}", vivExtQctrl.elem_size - 1);
                if (ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ectrls) < 0){
                    ALOGE("set ext ctrl to get ae info error\n");
                }
                if (ioctl(camDevice, VIDIOC_G_EXT_CTRLS, &ectrls) < 0){
                    ALOGE("get ext ctrl to get ae info error\n");
                } else {
                    Json::Value jResponse;
                    Json::Reader reader;
                    reader.parse(ectrl.string, jResponse, true);
                    if (exp == jResponse["time"].asFloat() && gain == jResponse["gain"].asFloat()){
                        stability_count++;
                    }else{
                        stability_count = 0;
                    }
                    exp = jResponse["time"].asFloat();
                    gain = jResponse["gain"].asFloat();

                }
                delete [] ectrl.string;
                if (stability_count > 100)
                {
                    ioctl(camDevice, VIDIOC_QBUF, &buffer);
                    break;
                }
            }
            ioctl(camDevice, VIDIOC_QBUF, &buffer);
        }
    }
    ioctl(camDevice, VIDIOC_STREAMOFF, &type);
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = 0;
    if (ioctl(camDevice, VIDIOC_REQBUFS, &bufrequest) < 0) {
        ALOGE("VIDIOC_REQBUFS: %s", strerror(errno));
        exit(0);
    }
    for (int i = 0; i < BUFFER_COUNT; i++) {
        munmap(vData[i],buffer.length);
    }
    close(camDevice);
    camDevice = -1;
    sleep(2);
    printf("***********start capture raw**********\n");

    camDevice = openCamera(deviceId);
    if (sensorMode >= 0) {
        struct viv_caps_mode_s caps_mode;
        memset(&caps_mode,0,sizeof(caps_mode));
        caps_mode.mode = sensorMode;
        if (ioctl(camDevice, VIV_VIDIOC_S_CAPS_MODE, &caps_mode) == 0) {
            ALOGI("Video Test: Set mode[%d]\n",caps_mode.mode);
        } else {
            ALOGE("Video Test: Set mode[%d] Failed\n",caps_mode.mode);
            return -1;
        }
    }
    setFeature(DWE_OFF);

    pixelFormat = mediaFormat2V4l2Format[string2MediaFormat[szFormat]];
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = pixelFormat;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;

    ALOGI("====xx VIDIOC_S_FMT %dx%d, 0x%x", width, height, pixelFormat);
    if (ioctl(camDevice, VIDIOC_S_FMT, &format) < 0) {
        ALOGE("VIDIOC_S_FMT: %s", strerror(errno));
        return -1;
    }

    updateCtrlIDList(camDevice);

    {
        static const int BUFFER_COUNT = 4;
        v4l2_requestbuffers bufrequest;
        bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufrequest.memory = V4L2_MEMORY_MMAP;
        bufrequest.count = BUFFER_COUNT;
        if (ioctl(camDevice, VIDIOC_REQBUFS, &bufrequest) < 0) {
            ALOGE("VIDIOC_REQBUFS: %s", strerror(errno));
            return -1;
        }

        std::vector<void*> vData;
        v4l2_buffer buffer = {};
        for (int i = 0; i < BUFFER_COUNT; i++) {
            memset(&buffer, 0, sizeof(buffer));
            buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory   = V4L2_MEMORY_MMAP;
            buffer.index    = i;
            if (ioctl(camDevice, VIDIOC_QUERYBUF, &buffer) < 0) {
                ALOGE("VIDIOC_QUERYBUF: %s", strerror(errno));
                return -1;
            }
            ALOGI("Buffer description:");
            ALOGI("  offset: %d", buffer.m.offset);
            ALOGI("  length: %d", buffer.length);
            if (ioctl(camDevice, VIDIOC_QBUF, &buffer) < 0) {
                ALOGE("VIDIOC_QBUF: %s", strerror(errno));
                return -1;
            }
            void* data = mmap(
                    NULL,
                    buffer.length,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    camDevice,
                    buffer.m.offset
            );
            if (data == MAP_FAILED) {
                ALOGE("mmap: %s", strerror(errno));
                return -1;
            }
            ALOGI("map buffer %p", data);
            vData.push_back(data);
        }
        int type = buffer.type;
        if (ioctl(camDevice, VIDIOC_STREAMON, &type) < 0) {
            ALOGE("VIDIOC_STREAMON: %s", strerror(errno));
            return -1;
        }

        struct v4l2_ext_controls ectrls;
        struct v4l2_ext_control ectrl;
        memset(&ectrls, 0, sizeof(ectrls));
        memset(&ectrl, 0, sizeof(ectrl));
        ectrl.string = new char [vivExtQctrl.elem_size];
        ectrl.id = vivExtQctrl.id;
        ectrl.size = vivExtQctrl.elem_size;
        ectrls.controls = &ectrl;
        ectrls.count = 1;
        strncpy(ectrl.string, "{<id>:<ec.g.cfg>}", vivExtQctrl.elem_size - 1);
        if (ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ectrls) < 0){
            ALOGE("set ext ctrl to get ae info error\n");
        }
        if (ioctl(camDevice, VIDIOC_G_EXT_CTRLS, &ectrls) < 0){
            ALOGE("get ext ctrl to get ae info error\n");
        } else {
            Json::Value jResponse;
            Json::Reader reader;
            reader.parse(ectrl.string, jResponse, true);
            jResponse["id"] = "ec.s.cfg";
            jResponse["time"] = exp;
            jResponse["gain"] = gain;
             printf("[ae info]:exp:%f gain%f\n",exp,gain);

            strcpy(ectrl.string, jResponse.toStyledString().c_str());
            ioctl(camDevice, VIDIOC_S_EXT_CTRLS, &ectrls);
        }
        delete [] ectrl.string;

        while(1){
            fd_set fds;
            FD_ZERO (&fds);
            FD_SET (camDevice, &fds);
            select (camDevice + 1, &fds, NULL, NULL, NULL);
            if (ioctl(camDevice, VIDIOC_DQBUF, &buffer) < 0) {
                ALOGE("VIDIOC_DQBUF: %s", strerror(errno));
            } else{
                ALOGI("VIDIOC_DQBUF success");
                pDisplay->showBuffer((unsigned char*)vData[buffer.index], width, height, pixelFormat, buffer.length);
                ioctl(camDevice, VIDIOC_QBUF, &buffer);
            }
        }
    }


    return 0;
}

static void stop(int sig) {
    if (camDevice < 0)
        exit(0);

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camDevice, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("VIDIOC_STREAMOFF: %s", strerror(errno));
        exit(0);
    }

    v4l2_requestbuffers bufrequest;
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = 0;
    if (ioctl(camDevice, VIDIOC_REQBUFS, &bufrequest) < 0) {
        ALOGE("VIDIOC_REQBUFS: %s", strerror(errno));
        exit(0);
    }

    if (pDisplay) {
        delete(pDisplay);
        pDisplay = NULL;
    }

    close(camDevice);
    exit(0);
}
