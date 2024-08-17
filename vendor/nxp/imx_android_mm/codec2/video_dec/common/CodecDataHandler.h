/**
 *  Copyright 2023 NXP
 *  All Rights Reserved.
*/
#ifndef CODECDATA_HANDLER
#define CODECDATA_HANDLER
#include <media/stagefright/foundation/AHandler.h>
#include "Allocator.h"
#include "Memory.h"

namespace android {

class CodecDataHandler{
public:
    CodecDataHandler();
    virtual ~CodecDataHandler();
    status_t allocBuffer(uint32_t size);
    status_t addData(uint8_t* pInBuf, uint32_t size);
    status_t getData(int* fd, uint32_t* size);
    status_t getData(uint8_t** ptr, uint32_t* size);
    bool sent();
    bool hasData();
    status_t flush();
    status_t clear();

private:
    uint8_t* pCodecDataBuf;
    uint32_t mSize;
    uint32_t mLen;
    bool mFlush;
    bool mSent;
    int mFd;
    uint64_t mAddr;
    fsl::Allocator * mAllocator;
};

}
#endif

