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

#include "camera/camera.hpp"
#include "camera/halholder.hpp"
#include "common/macros.hpp"
#include "dom/dom.hpp"
#include "network/file-server.hpp"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DAEMON 0

#if DAEMON
static void daemonize(void);
static void signalHandler(int32_t);
#endif

int32_t main(int32_t, char **) {
#if DAEMON
    daemonize();

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#endif

    pCalibration = new clb::Calibration();
    DCT_ASSERT(pCalibration);

    pHalHolder = new HalHolder();
    DCT_ASSERT(pHalHolder);
    if(!pHalHolder || !pHalHolder->hHal) {
        if (pCalibration) {
            delete pCalibration;
        }
        printf("Get the handle of HAL Layer error!\n");
        printf("tuning-server exit!\n");
        return -1;
    }
#if defined ISP_DOM
    pDom = new Dom();
    DCT_ASSERT(pDom);
#endif

    pCamera = new camera::Camera();
    DCT_ASSERT(pCamera);

    FileServer *pFileServer = new FileServer();
    DCT_ASSERT(pFileServer);

    if (pFileServer) {
        delete pFileServer;
    }

    if (pCamera) {
        delete pCamera;
    }

#if defined ISP_DOM
    if (pDom) {
        delete pDom;
    }
#endif

    if (pHalHolder) {
        delete pHalHolder;
    }

    if (pCalibration) {
        delete pCalibration;
    }

    return 0;
}

#if DAEMON
static void daemonize(void) {
#ifdef SIGTTOU
    signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
    signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_IGN);
#endif
    if (0 != fork()) {
        exit(0);
    }

    if (-1 == setsid()) {
        exit(0);
    }

    signal(SIGHUP, SIG_IGN);

    if (0 != fork()) {
        exit(0);
    }

    if (0 != chdir("/")) {
        exit(0);
    }
}

static void signalHandler(int32_t signal) {
    fprintf(stdout, "Get signal: %d\n", signal);

    switch (signal) {
    case SIGTERM: /*软件终止信号*/
    case SIGINT:  /*中断进程*/
        printf("SocketHost exit\n");
        exit(0);
        break;

    case SIGCHLD:
        break;

    default:
        break;
    }
}
#endif
