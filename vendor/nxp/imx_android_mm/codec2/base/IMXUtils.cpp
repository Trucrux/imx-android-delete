/**
 *  Copyright 2019-2020 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#include <stdio.h>
#include <string.h>
#include <graphics.h>
#include <media/stagefright/MediaDefs.h>
#include <C2Config.h>
#include "Imx_ext.h"
#include "IMXUtils.h"
#include "graphics_ext.h"

#include "Allocator.h"
#include "Composer.h"
#include "MemoryDesc.h"
#include <cutils/properties.h>


namespace android {


typedef struct {
    const char* name;
    const char* mime;
} NameMime;

static NameMime NameMimeMap[] = {
        {"c2.imx.avc.decoder", MEDIA_MIMETYPE_VIDEO_AVC},
        {"c2.imx.avc.decoder.secure", MEDIA_MIMETYPE_VIDEO_AVC},
        {"c2.imx.avc.encoder", MEDIA_MIMETYPE_VIDEO_AVC},
        {"c2.imx.hevc.decoder", MEDIA_MIMETYPE_VIDEO_HEVC},
        {"c2.imx.hevc.decoder.secure", MEDIA_MIMETYPE_VIDEO_HEVC},
        {"c2.imx.hevc.encoder", MEDIA_MIMETYPE_VIDEO_HEVC},
        {"c2.imx.vp8.decoder", MEDIA_MIMETYPE_VIDEO_VP8},
        {"c2.imx.vp8.encoder", MEDIA_MIMETYPE_VIDEO_VP8},
        {"c2.imx.vp9.decoder", MEDIA_MIMETYPE_VIDEO_VP9},
        {"c2.imx.vp9.decoder.secure", MEDIA_MIMETYPE_VIDEO_VP9},
        {"c2.imx.mpeg2.decoder", MEDIA_MIMETYPE_VIDEO_MPEG2},
        {"c2.imx.mpeg4.decoder", MEDIA_MIMETYPE_VIDEO_MPEG4},
        {"c2.imx.h263.decoder", MEDIA_MIMETYPE_VIDEO_H263},
        {"c2.imx.mjpeg.decoder", MEDIA_MIMETYPE_VIDEO_MJPEG},
        {"c2.imx.xvid.decoder", MEDIA_MIMETYPE_VIDEO_XVID},
        {"c2.imx.vc1.decoder", MEDIA_MIMETYPE_VIDEO_VC1},
        {"c2.imx.rv.decoder", MEDIA_MIMETYPE_VIDEO_REAL},
        {"c2.imx.sorenson.decoder", MEDIA_MIMETYPE_VIDEO_SORENSON},
};

const char* Name2MimeType(const char* name) {
    int i;
    for (i = 0; i < sizeof(NameMimeMap)/sizeof(NameMime); i++) {
        if (strcmp(NameMimeMap[i].name, name) == 0) {
            return NameMimeMap[i].mime;
        }
    }

    return nullptr;
}

int pxlfmt2bpp(int pxlfmt) {
    int bpp; // bit per pixel

    switch(pxlfmt) {
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_YCBCR_420_888:
        case HAL_PIXEL_FORMAT_NV12_TILED:
        case HAL_PIXEL_FORMAT_YV12:
          bpp = 12;
          break;
        case HAL_PIXEL_FORMAT_P010:
        case HAL_PIXEL_FORMAT_P010_TILED:
        case HAL_PIXEL_FORMAT_P010_TILED_COMPRESSED:
          bpp = 15;
          break;
        case HAL_PIXEL_FORMAT_RGB_565:
        case HAL_PIXEL_FORMAT_YCbCr_422_P:
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        case HAL_PIXEL_FORMAT_YCBCR_422_I:
            bpp = 16;
            break;

        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            bpp = 32;
            break;
        case HAL_PIXEL_FORMAT_YCBCR_P010:
            bpp = 24;
            break;
        default:
          bpp = 0;
          break;
    }
    return bpp;
}

int bytesPerLine(int pxlfmt, int width) {
    int bytes;

    switch(pxlfmt) {
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_YCBCR_420_888:
        case HAL_PIXEL_FORMAT_NV12_TILED:
        case HAL_PIXEL_FORMAT_YV12:
          bytes = 1;
          break;
        case HAL_PIXEL_FORMAT_RGB_565:
        case HAL_PIXEL_FORMAT_YCbCr_422_P:
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        case HAL_PIXEL_FORMAT_YCBCR_422_I:
            bytes = 2;
            break;
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            bytes = 4;
            break;
        default:
            bytes = 1;
            break;
    }

    return bytes * width;
}

int GetSocId(char* socId, int size) {
    int ret = 1; // default set failed;

    if ((socId == NULL) || (size < 10))
        return false;

    memset(socId, 0, size);

    FILE *f = fopen("/sys/devices/soc0/soc_id", "r");
    if (f != NULL) {
        if (fgets(socId, size, f) != NULL) {
            ret = 0; // success
        }
        fclose(f);
    }

    return ret;
}

int IMXAllocMem(int size) {
    int flags = fsl::MFLAGS_CONTIGUOUS;
    int align;
    align = MEM_ALIGN;
    fsl::Allocator * pAllocator = fsl::Allocator::getInstance();

    return pAllocator->allocMemory(size, align, flags);
}

int IMXGetBufferAddr(int fd, int size, uint64_t& addr, bool isVirtual) {
    fsl::Allocator * pAllocator = fsl::Allocator::getInstance();
    int ret = 0;

    if (isVirtual)
        ret = pAllocator->getVaddrs(fd, size, addr);
    else
        ret = pAllocator->getPhys(fd, size, addr);

    if (ret != 0) {
        addr = 0;
        ALOGE("get %s address failed, fd %d, size %d, ret %d",
                isVirtual ? "virtual" : "physical", fd, size, ret);
    }

    return ret;
}
int IMXGetBufferType(int fd){
    fsl::Allocator * pAllocator = fsl::Allocator::getInstance();
    return pAllocator->getHeapType(fd);
}

void getGpuPhysicalAddress(int fd, uint32_t size, uint64_t* physAddr)
{
    fsl::Memory *srcHandle = NULL;
    fsl::Composer *mComposer = fsl::Composer::getInstance();
    fsl::MemoryDesc desc;
    desc.mFlag = 0;
    desc.mWidth = desc.mStride = size / 4;
    desc.mHeight = 1;
    desc.mFormat = HAL_PIXEL_FORMAT_RGBA_8888;
    desc.mFslFormat = fsl::FORMAT_RGBA8888;
    desc.mSize = size;
    desc.mProduceUsage = 0;

    if(fd < 0 || size <= 0) {
        ALOGE("%s line %d: invalid fd=%d, size=%d", __FUNCTION__, __LINE__, fd, size);
        return;
    }

    srcHandle = new fsl::Memory(&desc ,fd, -1);
    srcHandle->phys = *physAddr;
    mComposer->lockSurface(srcHandle);
    *physAddr = srcHandle->phys;
    mComposer->unlockSurface(srcHandle);
    if (srcHandle->fd > 0) {
        close(srcHandle->fd);
        srcHandle->fd = 0;
    }
    delete srcHandle;
}

#if defined(__LP64__)
#define G2DENGINE "/vendor/lib64/libg2d"
#else
#define G2DENGINE "/vendor/lib/libg2d"
#endif
bool getDefaultG2DLib(char *libName, int size) {
    char value[PROPERTY_VALUE_MAX];

    //suppose that string length is 10 when get prop
    if((libName == NULL)||(size < strlen(G2DENGINE) + strlen(".so") + 10))
        return false;

    memset(libName, 0, size);
    property_get("vendor.imx.default-g2d", value, "");
    if(strcmp(value, "") == 0) {
        ALOGE("No g2d lib available to be used!");
        return false;
    } else {
        strncpy(libName, G2DENGINE, strlen(G2DENGINE));
        strcat(libName, "-");
        strcat(libName, value);
        strcat(libName, ".so");
    }
    ALOGV("Default g2d lib: %s", libName);
    return true;
}

} // namespace android
