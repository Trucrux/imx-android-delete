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
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct Vom2 : ItfBufferCb {
    virtual ~Vom2() {}

    void bufferCb(MediaBuffer_t *) override {
    // TODO: Process YUV frame
    cout << "------------------------------------" << std::endl;
    cout << "[TUNING-LITE]: Media buffer incoming" << std::endl;
    cout << "------------------------------------" << std::endl;
    }
};

int32_t main(int32_t argc, char *argv[]) {
    if (argc < 2) {
        cout << "Please input XML file name as argument" << std::endl;
        return -1;
    }

    char *pXmlFileName = argv[1];

    pCalibration = new clb::Calibration();
    DCT_ASSERT(pCalibration);

    pCalibration->restore(pXmlFileName);

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
#endif

    pCamera = new camera::Camera();
    DCT_ASSERT(pCamera);

    // TODO: Update sensor driver file name and calibration file name
    int32_t ret =
        pCamera->sensor().driverChange("./ov2775.drv", "OV2775_fisheye.xml");
    DCT_ASSERT(ret == RET_SUCCESS);

    pCalibration->module<clb::Inputs>().input().config.type = clb::Input::Sensor;

    Vom2 *pVom2 = new Vom2;

    auto ret = pCamera->pipelineConnect(true, pVom2);
    DCT_ASSERT(ret == RET_SUCCESS);

    ret = pCamera->previewStart();
    DCT_ASSERT(ret == RET_SUCCESS);

    for (;;) {
        osSleep(1000);
    }

    ret = pCamera->previewStop();
    DCT_ASSERT(ret == RET_SUCCESS);

    if (pVom2) {
        delete pVom2;
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
