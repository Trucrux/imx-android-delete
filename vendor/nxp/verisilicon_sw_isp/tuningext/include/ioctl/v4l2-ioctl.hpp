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

#ifdef APPMODE_V4L2

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
#include <json_helper.h>

#include "viv_video_kevent.h"
#include "ioctl_cmds.h"
#include "base64.hpp"

#include "log.h"

#define LOGTAG "TUNINGEXT"

extern int fd;

#ifndef MIN
#define MIN(a, b)   ( ((a)<=(b)) ? (a) : (b) )
#endif /* MIN */

#ifndef MAX
#define MAX(a, b)   ( ((a)>=(b)) ? (a) : (b) )
#endif /* MAX */

extern int viv_private_ioctl(const char *cmd, Json::Value& jsonRequest, Json::Value& jsonResponse);

#endif
