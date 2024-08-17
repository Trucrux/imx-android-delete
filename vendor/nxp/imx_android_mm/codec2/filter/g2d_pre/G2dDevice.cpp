/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "G2dDevice"

#include "G2dDevice.h"
#include <utils/Log.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include "graphics_ext.h"
#include "IMXUtils.h"

namespace android {

G2DDevice::G2DDevice(bool getTime):
    mGetTime(getTime),
    mOutCnt(0),
    mTotalBlitTime(0){

    mStatus = UNINITED;

    if(openDevice() == OK)
        mStatus = INITED;
}
G2DDevice::~G2DDevice(){
    closeDevice();
}
status_t G2DDevice::openDevice(){

    status_t ret = BAD_VALUE;
    char g2dlibName[PATH_MAX] = {0};

    if(getDefaultG2DLib(g2dlibName, PATH_MAX)){
        sG2dModule.hLibHandle = dlopen(g2dlibName, RTLD_NOW);
    }

    if (nullptr == sG2dModule.hLibHandle) {
        ALOGE("dlopen %s failed, err: %s", g2dlibName, dlerror());
        return ret;
    }

    sG2dModule.mG2dOpen = (g2d_func1)dlsym(sG2dModule.hLibHandle, "g2d_open");
    sG2dModule.mG2dClose = (g2d_func1)dlsym(sG2dModule.hLibHandle, "g2d_close");
    sG2dModule.mG2dFinish = (g2d_func1)dlsym(sG2dModule.hLibHandle, "g2d_finish");
    sG2dModule.mG2dBlitEx = (g2d_func2)dlsym(sG2dModule.hLibHandle, "g2d_blit");

    if (!sG2dModule.mG2dOpen || !sG2dModule.mG2dClose ||
            !sG2dModule.mG2dFinish || !sG2dModule.mG2dBlitEx) {
        ALOGE("dlsym failed, err: %s", dlerror());
        return ret;
    }

    pPPHandle = (G2D_HANDLE*)malloc(sizeof(G2D_HANDLE));
    if (!pPPHandle)
        return ret;

    ret = OK;
    return ret;
}
void G2DDevice::closeDevice(){

    if (pPPHandle) {
        free(pPPHandle);
        pPPHandle = nullptr;
    }

    if (sG2dModule.hLibHandle) {
        dlclose(sG2dModule.hLibHandle);
        memset(&sG2dModule, 0, sizeof(G2D_MODULE));
    }
    mStatus = UNINITED;

    ALOGV("closeDevice end");
}

status_t G2DDevice::setSrcParam(G2D_PARAM * param){

    status_t ret = OK;
    std::lock_guard<std::mutex> lock(mMutex);

    if(param == nullptr || mStatus == UNINITED)
        return BAD_VALUE;

    ret = setParam(param, &sSrcSurface);
    if(ret)
        return ret;

    sSrcSurface.format = getColorFormat(param->format);

    return OK;
}
status_t G2DDevice::setDstParam(G2D_PARAM * param){

    status_t ret = OK;
    std::lock_guard<std::mutex> lock(mMutex);

    if(param == nullptr || mStatus == UNINITED)
        return BAD_VALUE;

    ret = setParam(param, &sDstSurface);
    if(ret)
        return ret;

    sDstSurface.format = G2D_YUYV;
    mStatus = READY;

    return OK;
}
status_t G2DDevice::setParam(G2D_PARAM * param, G2DSurface *tar){

    if(param == nullptr || tar == nullptr)
        return BAD_VALUE;
    
    tar->width = param->width;
    tar->height = param->height;
    tar->stride = param->stride;

    tar->left = 0;
    tar->top = 0;
    tar->right = param->cropWidth;
    tar->bottom = param->cropHeight;

    return OK;
}

status_t G2DDevice::blit(uint64_t src_addr, uint64_t tar_addr)
{
    struct timeval tv, tv1;
    std::lock_guard<std::mutex> lock(mMutex);

    if(src_addr == 0 || tar_addr == 0){
        ALOGE("blit bad addr\n");
        return BAD_VALUE;
    }

    if(mStatus != READY){
        ALOGE("blit bad state\n");
        return UNKNOWN_ERROR;
    }

    sSrcSurface.planes[0] = static_cast<int>(src_addr);
    if (sSrcSurface.format == G2D_YV12) {
        int Ysize = sSrcSurface.stride * sSrcSurface.height;
        sSrcSurface.planes[1] = sSrcSurface.planes[0] + Ysize;
        sSrcSurface.planes[2] = sSrcSurface.planes[0] + Ysize + Ysize / 4;
    }

    sDstSurface.planes[0] = static_cast<int>(tar_addr);

    if(mGetTime)
        gettimeofday (&tv, nullptr);

    if (sG2dModule.mG2dOpen(&pPPHandle->g2dHandle) == -1) {
        ALOGE("g2d_open failed\n");
        return BAD_VALUE;
    } else if (nullptr == pPPHandle->g2dHandle) {
        ALOGE("g2d_open return null handle\n");
        return BAD_VALUE;
    }

    if (sG2dModule.mG2dBlitEx(pPPHandle->g2dHandle, &sSrcSurface, &sDstSurface) != 0) {
        ALOGE("g2d_blitEx failed\n");
        return BAD_VALUE;
    }

    if (sG2dModule.mG2dFinish(pPPHandle->g2dHandle) != 0) {
        ALOGE("g2d_finish failed\n");
        return BAD_VALUE;
    }

    if (sG2dModule.mG2dClose(pPPHandle->g2dHandle) != 0) {
        ALOGE("g2d_close failed\n");
        return BAD_VALUE;
    }
    pPPHandle->g2dHandle = NULL;

    if(mGetTime){
        gettimeofday (&tv1, nullptr);
        mTotalBlitTime += (tv1.tv_sec-tv.tv_sec)*1000000+(tv1.tv_usec-tv.tv_usec);
    }

    mOutCnt++;

    return OK;
}
uint32_t G2DDevice::getRunningTimeAndFlush(){

    uint32_t ts = 0;
    if(mGetTime && mOutCnt > 0){
        ts = (uint32_t)(mTotalBlitTime/mOutCnt);
        ALOGD("g2d average blit time=%d us",ts);
    }

    mTotalBlitTime = 0;
    mOutCnt = 0;
    return ts;
}
g2d_format G2DDevice::getColorFormat(int pixelColorFmt) {
  g2d_format g2dColorFmt = G2D_YUYV;

  switch(pixelColorFmt) {
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCBCR_420_888:
        g2dColorFmt = G2D_NV12;
        break;
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
        g2dColorFmt = G2D_I420;
        break;
    case HAL_PIXEL_FORMAT_YV12:
        g2dColorFmt = G2D_YV12;
        break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        g2dColorFmt = G2D_YUYV;
        break;
    case HAL_PIXEL_FORMAT_RGB_565:
        g2dColorFmt = G2D_RGB565;
        break;
    case HAL_PIXEL_FORMAT_RGB_888:
        g2dColorFmt = G2D_RGB888;
        break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        g2dColorFmt = G2D_RGBA8888;
        break;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        g2dColorFmt = G2D_RGBX8888;
        break;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        g2dColorFmt = G2D_BGRA8888;
        break;
    default:
      ALOGE("unknown pixelColorFmt %d\n", pixelColorFmt);
      break;
  }

  return g2dColorFmt;
}


}// namespace android
