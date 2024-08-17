/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef G2D_DEVICE_H
#define G2D_DEVICE_H
#include "g2d.h"
#include "g2dExt.h"
#include <cstdint>
#include <utils/Errors.h>
#include <mutex>

namespace android {

typedef struct g2d_surfaceEx G2DSurfaceEx;

typedef int (*g2d_func1)(void* handle);
typedef int (*g2d_func2)(void *handle, G2DSurfaceEx *srcEx, G2DSurfaceEx *dstEx);

typedef struct {
    void * g2dHandle;
} G2D_HANDLE;

typedef struct G2dParam {
	uint32_t format;
	uint32_t width;
    uint32_t height;
	uint32_t stride;
    uint32_t cropWidth;
	uint32_t cropHeight;
    uint32_t uvOffset; // for nv12
	bool interlace;
}G2D_PARAM;

typedef struct {
    void * hLibHandle;
    g2d_func1 mG2dOpen;
    g2d_func1 mG2dFinish;
    g2d_func1 mG2dClose;
    g2d_func2 mG2dBlitEx;
} G2D_MODULE;

class G2DDevice {
public:
    G2DDevice(bool getTime);
    ~G2DDevice();

    status_t setSrcParam(G2D_PARAM * param);
    status_t setDstParam(G2D_PARAM * param);
	status_t blit(uint64_t src_addr, uint64_t tar_addr);
	uint32_t getRunningTimeAndFlush();

private:

    std::mutex mMutex;

	G2D_HANDLE* pPPHandle;
    G2D_MODULE sG2dModule;
    G2DSurfaceEx sSrcSurface;
    G2DSurfaceEx sDstSurface;
    uint32_t mSrcUVOffset;
    uint32_t mDstUVOffset;

    enum {
        UNINITED,
        INITED,
        READY,
    };

	int mStatus;
	bool mGetTime;
	uint32_t mOutCnt;
	uint64_t mTotalBlitTime;

	status_t openDevice();
	void closeDevice();

	status_t setParam(G2D_PARAM * param, G2DSurfaceEx *tar);
};

}
#endif
