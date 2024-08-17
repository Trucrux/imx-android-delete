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

#ifndef DEVELOPER_MEDIACONTROL_INCLUDE_DEV_H_
#define DEVELOPER_MEDIACONTROL_INCLUDE_DEV_H_

#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static inline int openVideoDevice(const char *devname, const char *vid) {
    int filep;
    v4l2_capability caps;
    if (!devname)
        return -1;

    filep = open(devname, O_RDWR | O_NONBLOCK);
    if (filep < 0) {
        return filep;
    }

    if (ioctl(filep, VIDIOC_QUERYCAP, &caps) < 0) {
        close(filep);
        return -1;
    }

    if (strcmp(reinterpret_cast<char*>(caps.driver), "viv_v4l2_device") ||
        (vid && strcmp(reinterpret_cast<char*>(caps.bus_info), vid))) {
        close(filep);
        return -1;
    }
    return filep;
}

static inline int openVideoDevice(int id) {
    int filep;
    char vid[64], szFile[64];
    sprintf(vid, "platform:viv%d", id);
    for (int i = 0; i < 20; i++) {
        sprintf(szFile, "/dev/video%d", i);
        filep = openVideoDevice(szFile, vid);
        if (filep >= 0)
            return filep;
    }
    return -1;
}

static inline void closeVideoDevice(int *filep) {
    if (filep && *filep >= 0) {
        close(*filep);
        *filep = -1;
    }
}

#endif //DEVELOPER_MEDIACONTROL_INCLUDE_DEV_H_

