/**
 *  Copyright 2023 NXP
 *  All Rights Reserved.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CodecData"
#include "CodecDataHandler.h"

namespace android {
CodecDataHandler::~CodecDataHandler(){
    clear();
}
CodecDataHandler::CodecDataHandler():
    pCodecDataBuf(nullptr),
    mSize(0),
    mLen(0),
    mFlush(true),
    mSent(false),
    mFd(0),
    mAddr(0),
    mAllocator(fsl::Allocator::getInstance()){
}
status_t CodecDataHandler::allocBuffer(uint32_t size){
    int flags = fsl::MFLAGS_CONTIGUOUS;
    int align = MEM_ALIGN;
    int ret = 0;

    if(!mAllocator)
        return NO_MEMORY;

    if(mAddr > 0 && mSize > 0)
        munmap((void*)mAddr, mSize);

    if(mFd > 0){
        close(mFd);
        mFd = 0;
    }

    mFd = mAllocator->allocMemory(size, align, flags);
    if(mFd <= 0){
        ALOGE("allocMemory failed");
        return NO_MEMORY;
    }
    mSize = size;
    ret = mAllocator->getVaddrs(mFd, mSize, mAddr);
    if(ret){
        mAddr = 0;
        ALOGE("getVaddrs failed");
        return NO_MEMORY;
    }
    ALOGV("allocBuffer mSize=%d",mSize);
    mLen = 0;
    return OK;
}
status_t CodecDataHandler::addData(uint8_t* pInBuf, uint32_t size){

    //assume max codec data cost 10K
    if(pInBuf == nullptr || size > 10240)
        return BAD_VALUE;

    if(mSent){
        mLen = 0;
        mSent = false;
    }

    if(mFlush){
        mLen = 0;
        mFlush = false;
    }

    if(size + mLen > mSize)
        return NO_MEMORY;

    if(mAddr == 0)
        return NO_MEMORY;

    memcpy((void*)(mAddr + mLen), pInBuf, size);

    ALOGV("queueCodecData input size=%d, offset=%d",size, mLen);

    mLen += size;

    return OK;
}
status_t CodecDataHandler::getData(int* fd, uint32_t* size){
    if(fd == nullptr || size == nullptr)
        return BAD_VALUE;

    mSent = true;

    if(mLen == 0)
        return INVALID_OPERATION;

    *size = mLen;
    *fd = mFd;
    ALOGV("CodecDataHandler::getData fd");

    return OK;
}
status_t CodecDataHandler::getData(uint8_t** ptr, uint32_t* size){
    if(ptr == nullptr || size == nullptr)
        return BAD_VALUE;

    mSent = true;

    if(mLen == 0)
        return INVALID_OPERATION;

    *size = mLen;
    *ptr = (uint8_t*)(mAddr);
    ALOGV("CodecDataHandler::getData ptr");

    return OK;
}
bool CodecDataHandler::sent(){
    return mSent;
}
bool CodecDataHandler::hasData(){
    return mLen > 0 ? true:false;
}
status_t CodecDataHandler::flush(){
    mFlush = true;
    mSent = false;
    return OK;
}
status_t CodecDataHandler::clear(){
    ALOGV("CodecDataHandler::clear");
    if(mAddr > 0 && mSize > 0)
        munmap((void*)mAddr, mSize);

    if(mFd > 0){
        close(mFd);
        mFd = 0;
    }

    if(pCodecDataBuf){
        free(pCodecDataBuf);
        pCodecDataBuf = nullptr;
    }

    mLen = 0;
    mSize = 0;

    mFlush = true;
    mSent = false;
    return OK;
}

}
