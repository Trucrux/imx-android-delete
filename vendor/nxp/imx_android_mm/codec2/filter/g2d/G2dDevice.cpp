/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "G2dDevice"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>

#include "G2dDevice.h"
#include <utils/Log.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include "graphics_ext.h"
#include "IMXUtils.h"

namespace android {

G2DDevice::G2DDevice(bool getTime):
    mSrcUVOffset(0),
    mDstUVOffset(0),
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
    sG2dModule.mG2dBlitEx = (g2d_func2)dlsym(sG2dModule.hLibHandle, "g2d_blitEx");

    if (!sG2dModule.mG2dOpen || !sG2dModule.mG2dClose ||
            !sG2dModule.mG2dFinish || !sG2dModule.mG2dBlitEx) {
        ALOGE("dlsym failed, err: %s", dlerror());
        return ret;
    }

    pPPHandle = (G2D_HANDLE*)malloc(sizeof(G2D_HANDLE));
    if (!pPPHandle)
        return ret;

    if (sG2dModule.mG2dOpen(&pPPHandle->g2dHandle) == -1) {
        ALOGE("g2d_open failed\n");
        return ret;
    } else if (nullptr == pPPHandle->g2dHandle) {
        ALOGE("g2d_open return null handle\n");
        return ret;
    }

    ret = OK;
    return ret;
}
void G2DDevice::closeDevice(){

    if (pPPHandle && pPPHandle->g2dHandle){
        if(sG2dModule.mG2dClose){
            sG2dModule.mG2dClose(pPPHandle->g2dHandle);
        }
        pPPHandle->g2dHandle = NULL;
    }

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

    sSrcSurface.base.format = G2D_NV12;
    mSrcUVOffset = param->uvOffset;

    switch(param->format){
        case HAL_PIXEL_FORMAT_NV12_TILED:
            sSrcSurface.tiling = G2D_AMPHION_TILED;
            break;
        case HAL_PIXEL_FORMAT_P010_TILED:
            sSrcSurface.tiling = G2D_AMPHION_TILED_10BIT;
            break;
        default:
            ALOGE("source format %x not supported",param->format);
            return INVALID_OPERATION;
    }

    if (param->interlace) {
        sSrcSurface.tiling = (enum g2d_tiling)(sSrcSurface.tiling | G2D_AMPHION_INTERLACED);
    }

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

    switch(param->format){
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            sDstSurface.base.format = G2D_NV12;
            break;
        default:
            sDstSurface.base.format = G2D_YUYV;
            break;
    }

    sDstSurface.tiling = G2D_LINEAR;
    mDstUVOffset = param->uvOffset;
    mStatus = READY;

    return OK;
}
status_t G2DDevice::setParam(G2D_PARAM * param, G2DSurfaceEx *tar){

    if(param == nullptr || tar == nullptr)
        return BAD_VALUE;
    
    tar->base.width = param->width;
    tar->base.height = param->height;
    tar->base.stride = param->stride;

    tar->base.left = 0;
    tar->base.top = 0;
    tar->base.right = param->cropWidth;
    tar->base.bottom = param->cropHeight;

    return OK;
}

status_t G2DDevice::blit(uint64_t src_addr, uint64_t tar_addr)
{
    ATRACE_CALL();
    struct timeval tv, tv1;
    std::lock_guard<std::mutex> lock(mMutex);

    if(src_addr == 0 || tar_addr == 0)
        return BAD_VALUE;

    if(mStatus != READY)
        return UNKNOWN_ERROR;

    sSrcSurface.base.planes[0] = static_cast<int>(src_addr);
    sSrcSurface.base.planes[1] = static_cast<int>(src_addr) + mSrcUVOffset;

    sDstSurface.base.planes[0] = static_cast<int>(tar_addr);
    if(sDstSurface.base.format == G2D_NV12){
        sDstSurface.base.planes[1] = static_cast<int>(tar_addr) + mDstUVOffset;
    }

    if(mGetTime)
        gettimeofday (&tv, nullptr);

    if (sG2dModule.mG2dBlitEx(pPPHandle->g2dHandle, &sSrcSurface, &sDstSurface) != 0) {
        ALOGE("g2d_blitEx failed\n");
        return BAD_VALUE;
    }

    if (sG2dModule.mG2dFinish(pPPHandle->g2dHandle) != 0) {
        ALOGE("g2d_finish failed\n");
        return BAD_VALUE;
    }

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


}// namespace android
