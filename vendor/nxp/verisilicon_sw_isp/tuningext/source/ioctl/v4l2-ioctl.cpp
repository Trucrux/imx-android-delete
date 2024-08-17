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

#ifdef APPMODE_V4L2

#include "ioctl/v4l2-ioctl.hpp"

#define S_EXT_FLAG 555

#define VIV_CUSTOM_CID_BASE (V4L2_CID_USER_BASE | 0xf000)
#define V4L2_CID_VIV_EXTCTRL (VIV_CUSTOM_CID_BASE + 1)

int fd;
int streamid = 0;

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
    ec.size = VIV_JSON_BUFFER_SIZE;
    ecs.controls = &ec;
    ecs.count = 1;

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
        //return jsonResponse["MC_RET"].asInt();
        return 0;
    }

end:
     delete ec.string;
     ec.string = NULL;
     return S_EXT_FLAG;
}

#endif
