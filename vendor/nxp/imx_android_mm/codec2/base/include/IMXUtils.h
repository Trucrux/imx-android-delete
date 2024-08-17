/**
 *  Copyright 2019-2020 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef IMX_UUTILS_H_
#define IMX_UUTILS_H_

#include <media/stagefright/foundation/ColorUtils.h>

namespace android {


const char* Name2MimeType(const char* name);
int pxlfmt2bpp(int pxlfmt);
int bytesPerLine(int pxlfmt, int width);

int GetSocId(char* socId, int size);

int IMXAllocMem(int size);
int IMXGetBufferAddr(int fd, int size, uint64_t& addr, bool isVirtual);
int IMXGetBufferType(int fd);
void getGpuPhysicalAddress(int fd, uint32_t size, uint64_t* physAddr);
bool getDefaultG2DLib(char *libName, int size);

}

#endif // IMX_UUTILS_H_
