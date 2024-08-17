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
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>

#include <iostream>

#include "log.h"
#include "V4l2Event.h"
#include "MediaPipeline.h"

#if defined(__ANDROID__)
#include <sys/types.h>
#include <dirent.h>
#endif

#define LOGTAG "ISP_MEDIA_SERVER"
#if defined(__ANDROID__)
#define LOCKFILE "/data/vendor/isp/isp_media_server.pid"
#else
#define LOCKFILE "/var/run/isp_media_server.pid"
#endif
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

enum {
    MEDIA_CAMERA0 = 0,
    MEDIA_CAMERA1,
    MEDIA_DUAL_CAMERA,
};


static std::unordered_map<std::string, int> string2Camera = {
    {"CAMERA0", MEDIA_CAMERA0},
    {"CAMERA1", MEDIA_CAMERA1},
    {"DUAL_CAMERA", MEDIA_DUAL_CAMERA},
};

static std::unordered_map<std::string, bool> string2Warmup = {
    {"WARMUP", true},
    {"warmup", true},
};
static std::unordered_map<std::string, bool> string2Version = {
    {"-v", true},
    {"-V", true},
};

int server_already_running(void)
{
    int     result;
    int     fd;
    char    pid_buf[16];

    fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE);
    if(fd < 0)
    {
        ALOGE("can't open %s\n", LOCKFILE);
        return -2;
    }

    if(flock(fd, LOCK_EX | LOCK_NB) < 0)
    {
        ALOGE("isp_media_server is running(errno=%d)!\n", errno);
        close(fd);
        return -2;
    }

    result = ftruncate(fd, 0);
    if(result < 0)
    {
        ALOGE("ftruncate error!\n");
        return fd;
    }
    sprintf(pid_buf, "%ld\n", (long)getpid());
    result = write(fd, pid_buf, strlen(pid_buf)+1);
    if(result < 0)
    {
        ALOGE("write pid error!\n");
        return fd;
    }
    return fd;
}

int  fd_running = -1;
std::unique_ptr<V4l2Event> event0;
std::unique_ptr<V4l2Event> event1;
static void signal_handler(int sig);

#ifdef ANDROID
int v4l2_daemon(int argc, const char* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    if (argc ==  2){
         if(!(string2Version.find(argv[1])==string2Version.end())){
             std::cout << "Version : "<<MEDIA_SERVER_VERSION<<std::endl;
#ifndef ANDROID
             std::cout << "Build Time : "<< BUILD_TIME <<std::endl;
#endif
             return 0;
         }
    }

    fd_running = server_already_running();
    if(fd_running >= 0)
    {
        ALOGI("******************************************************************");
#ifndef ANDROID
        ALOGI("VIV ISP Media Control Framework V%s (%s)",
              MEDIA_SERVER_VERSION, BUILD_TIME);
#else
        ALOGI("VIV ISP Media Control Framework V%s", MEDIA_SERVER_VERSION);
#endif
        ALOGI("******************************************************************\n");

        if (argc < 2) {
            std::cout << "Usage: " << argv[0] << " CAMERA0 or CAMERA1 or DUAL_CAMERA\n";
            close(fd_running);
            return -1;
        }

        bool bWarmup = false;
        const char* szCamera = NULL;
        const char* szWarmup = NULL;
        int CameraType = -1;
        int i = 1;
        signal(SIGINT, signal_handler);
        while( i < argc ){
                if(!(string2Version.find(argv[i])==string2Version.end())){
                    std::cout << "Version : "<<MEDIA_SERVER_VERSION<<std::endl;
#ifndef ANDROID
                    std::cout << "Build Time : "<< BUILD_TIME <<std::endl;
#endif
                    i++;
                    continue;
                 }

                if (string2Camera.find(argv[i]) != string2Camera.end())
                   {
                     szCamera = argv[i];
                     CameraType = string2Camera[szCamera];
                     i++;
                     continue;
                   }
               if (string2Warmup.find(argv[i]) != string2Warmup.end())
                  {
                   szWarmup = argv[i];
                   bWarmup = string2Warmup[szWarmup];
                     i++;
                    continue;

                   }
                 close(fd_running);
                 return 1;
           }



        if(CameraType == MEDIA_CAMERA0 || CameraType == MEDIA_CAMERA1)
        {
            try {
                event0.reset(new V4l2Event(CameraType, bWarmup));
            } catch(...){
                close(fd_running);
                ALOGE(" isp_media_server exit!");
                return -1;
            }
        } else {
            try {
                event0.reset(new V4l2Event(MEDIA_CAMERA0, bWarmup));
                event1.reset(new V4l2Event(MEDIA_CAMERA1, bWarmup));
            } catch(...) {
                close(fd_running);
                ALOGE(" isp_media_server exit!");
                return -1;
            }
        }

        ESignal sig;
        sig.wait();
    }
    return 0;
}

static void signal_handler(int sig)
{
    if (fd_running < 0)
        exit(0);

    if (event0)
        event0.reset();

    if (event1)
        event1.reset();

    close(fd_running);
    fd_running = -1;
    exit(0);
}


#ifdef ANDROID
// Hint for the para of v4l2_daemon()
// 0: no isp camera, just exit.
// 1: Only CSI0 connect isp camera isp camera, use para "CAMERA0"
// 2: Only CSI1 connect isp camera, use para "CAMERA1"
// 3: Both CSI0/CSI1 connect isp cameras, use para "DUAL_CAMERA"

#define VVCAM_ISP0_NAME  "vvcam-isp.0"
#define VVCAM_ISP1_NAME  "vvcam-isp.1"
#define BUFFER_SIZE 64
static int IspCameraHint() {
    DIR *vidDir = NULL;
    struct dirent *dirEntry;
    char videoNamePath[BUFFER_SIZE];
    char videoName[BUFFER_SIZE];
    int fd;
    int ret;
    bool csi0_has_isp = false;
    bool csi1_has_isp = false;

    vidDir = opendir("/sys/class/video4linux");
    if (vidDir == NULL) {
        return 0;
    }

    while ((dirEntry = readdir(vidDir)) != NULL) {
        if (strncmp(dirEntry->d_name, "video", 5) == 0) {
            continue;
        }

        snprintf(videoNamePath, BUFFER_SIZE, "/sys/class/video4linux/%s/name", dirEntry->d_name);
        videoNamePath[BUFFER_SIZE-1] = 0;

        fd = open(videoNamePath, O_RDONLY);
        if(fd < 0) {
            ALOGW("%s, can't open %s", __func__, videoNamePath);
            continue;
        }

        memset(videoName, 0, BUFFER_SIZE);
        ret = read(fd, videoName, BUFFER_SIZE);
        close(fd);
        if (ret < 0) {
            ALOGW("%s, read %s failed, ret %d, %s", __func__, videoNamePath, ret, strerror(errno));
            continue;
        }

        videoName[BUFFER_SIZE-1] = 0;
        ALOGI("%s, %s name is %s", __func__, dirEntry->d_name, videoName);

        if(strncmp(videoName, VVCAM_ISP0_NAME, strlen(VVCAM_ISP0_NAME)) == 0)
            csi0_has_isp = true;

        if(strncmp(videoName, VVCAM_ISP1_NAME, strlen(VVCAM_ISP1_NAME)) == 0)
            csi1_has_isp = true;
    }

    closedir(vidDir);

    ALOGI("%s, csi0_has_isp %d, csi1_has_isp %d", __func__, csi0_has_isp, csi1_has_isp);

    if (!csi0_has_isp && !csi1_has_isp)
        return 0;
    else if (csi0_has_isp && !csi1_has_isp)
        return 1;
    else if (!csi0_has_isp && csi1_has_isp)
        return 2;
    else
        return 3;
}

int main() {
    int cameraHint = IspCameraHint();
    ALOGI("cameraHint %d", cameraHint);

    if ((cameraHint <= 0) || (cameraHint > 3)) {
        ALOGE("No ISP camera found, cameraHint %d", cameraHint);
        return -1;
    }

    int argc = 2;
    const char *argv[2];
    argv[0] = "isp_media_server";

    if(cameraHint == 1)
        argv[1] = "CAMERA0";
    else if(cameraHint == 2)
        argv[1] = "CAMERA1";
    else
        argv[1] = "DUAL_CAMERA";

    int ret = v4l2_daemon(argc, argv);
    return ret;
}

#endif
