/**
 *  Copyright 2019-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "VideoDecoderBase"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>

#define API_TRACE
#ifdef API_TRACE
#define VDB_API_TRACE ALOGV
#else
#define VDB_API_TRACE(...)
#endif

#define VDB_INFO_TRACE
#ifdef VDB_INFO_TRACE
#define VDB_INFO ALOGV
#else
#define VDB_INFO(...)
#endif

#include <utils/Log.h>
//#include <cutils/properties.h>
#include <media/stagefright/foundation/AMessage.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <C2Config.h>
#include <C2Debug.h>
#include <C2PlatformSupport.h>

#include "graphics_ext.h"
#include "Memory.h"
#include "IMXUtils.h"

#include "VideoDecoderBase.h"

namespace android {

static void Reply(const sp<AMessage> &msg, int32_t *err = nullptr) {
    sp<AReplyToken> replyId;
    CHECK(msg->senderAwaitsResponse(&replyId));
    sp<AMessage> reply = new AMessage;
    if (err) {
        reply->setInt32("err", *err);
    }
    reply->postReply(replyId);
}

VideoRect::VideoRect(VideoRect &rect) {
    left = rect.left;
    right = rect.right;
    top = rect.top;
    bottom = rect.bottom;
}

VideoRect::VideoRect(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
    : left(left),
      top(top),
      right(right),
      bottom(bottom) {
}

VideoFormat::VideoFormat() {
    pixelFormat = 0;
    minBufferNum = 0;
    width = DEFAULT_FRM_WIDTH;
    height = DEFAULT_FRM_HEIGHT;
    stride = DEFAULT_FRM_WIDTH;
    bufferNum = 0;
    bufferSize = 0;
    rect = {0,0,0,0};
    interlaced = false;
}

VideoDecoderBase::IMXInputBuffer::IMXInputBuffer(
                    void* pBuffer,
                    int fd,
                    int id,
                    uint32_t size,
                    uint64_t ts,
                    bool eos,
                    bool codecdata)
    : pInBuffer(pBuffer),
      fd(fd),
      id(id),
      size(size),
      timestamp(ts),
      eos(eos),
      csd(codecdata){
}

VideoDecoderBase::VideoDecoderBase()
    : bInputEos(false),
      bOutputEos(false),
      bAdaptiveMode(false),
      bFirstResChange(false),
      bSecureMode(false),
      bReceiveError(false),
      bCodecDataQueued(false),
      mMaxWidth(0),
      mMaxHeight(0),
      mMaxOutputBufferSize(0),
      mMaxOutputBufferCnt(8),
      nOutBufferUsage(GRALLOC_USAGE_PRIVATE_2
                      | GRALLOC_USAGE_HW_TEXTURE
                      | GRALLOC_USAGE_HW_COMPOSER),
      pCodecDataBuf(nullptr),
      nCodecDataLen(0),
      mLooper(new ALooper),
      mClient(nullptr),
      bOutputFmtChangedPending(false),
      bReleasingDecoder(false),
      bFlushed(false){

    mLooper->setName("VideoDecoderBase");
    mLooper->start(false, false, ANDROID_PRIORITY_VIDEO);
}

VideoDecoderBase::~VideoDecoderBase() {
    if (mLooper != NULL) {
        mLooper->unregisterHandler(id());
        mLooper->stop();
    }
}

status_t VideoDecoderBase::init(Client* client/*const std::shared_ptr<C2BlockPool> &pool*/) {
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);

    (void)mLooper->registerHandler(this);
    mClient = client;

    sp<AMessage> reply;
    (new AMessage(kWhatInit, this))->postAndAwaitResponse(&reply);
    int32_t err;
    CHECK(reply->findInt32("err", &err));
    return err;
}

status_t VideoDecoderBase::start() {
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);
    sp<AMessage> reply;
    (new AMessage(kWhatStart, this))->postAndAwaitResponse(&reply);
    int32_t err;
    CHECK(reply->findInt32("err", &err));
    return err;
}

status_t VideoDecoderBase::stop() {
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);
    sp<AMessage> reply;
    (new AMessage(kWhatStop, this))->postAndAwaitResponse(&reply);
    int32_t err;
    CHECK(reply->findInt32("err", &err));
    return err;
}

status_t VideoDecoderBase::flush() {
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);
    sp<AMessage> reply;
    (new AMessage(kWhatFlush, this))->postAndAwaitResponse(&reply);
    int32_t err;
    CHECK(reply->findInt32("err", &err));
    return err;
}

status_t VideoDecoderBase::destroy() {
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);
    sp<AMessage> reply;
    sp<AMessage> msg = new AMessage(kWhatDestroy, this);
    status_t err = msg->postAndAwaitResponse(&reply);
    if (err == OK && reply != NULL) {
        CHECK(reply->findInt32("err", &err));
    }
    return err;
}

status_t VideoDecoderBase::queueInput(
        uint8_t *pInBuf, uint32_t size, uint64_t timestamp, uint32_t flags, int fd, int id) {

    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);

    bool codecdata = ((flags & C2FrameData::FLAG_CODEC_CONFIG) != 0);

    #if 0
    // in secure mode, codec data should be sent with physical address, so don't memcpy here
    if (codecdata && !bSecureMode) {

        //for codec data, ccodec will merge csd0 & csd1 to one codec data buffer,
        //however, developer can still send 2 codec data buffer before normal frame.
        //suppose has codec data, here are some call sequences:
        //case 1. flush, queue normal frame
        //   codec data send to decoder
        //case 2. flush, queue codec data, then normal frame
        //   clear codec data and clear privious codec data and store new codec data
        //case 3. flush, queue codec data, queue codec data, then normal frame
        //   clear codec data and clear privious codec data and store new codec data,
        //   then append second codec data
        //case 4. codec data, normal frame
        //   playback start for first time
        //case 5. codec data, codec data, normal frame

        // new codecdata is arrived, reset bCodecDataQueued to false and clear previous codecdata
        //only accept codec data before queue normal frame
        if (bCodecDataQueued) {
            bCodecDataQueued = false;
            nCodecDataLen = 0;
        }else if(bFlushed && nCodecDataLen > 0){//case 2: clear codec data
            nCodecDataLen = 0;
        }//else case 3: append second codec data

        if(bFlushed)
            bFlushed = false;

        if (!pCodecDataBuf) {
            pCodecDataBuf = (uint8_t*)malloc(size);
        } else {
            pCodecDataBuf = (uint8_t*)realloc(pCodecDataBuf, nCodecDataLen + size);
        }

        if (!pCodecDataBuf) {
            ALOGE("malloc pCodecDataBuf falied, size=%d", size);
            return BAD_VALUE;
        }

        memcpy(pCodecDataBuf + nCodecDataLen, pInBuf, size);
        nCodecDataLen += size;
        return OK;
    }
    #endif

    bInputEos = ((flags & C2FrameData::FLAG_END_OF_STREAM) != 0);
    if(bFlushed)
        bFlushed = false;

    {
        Mutexed<InputBufferQueue>::Locked queue(mInputQueue);
        queue->push_back(std::make_unique<IMXInputBuffer>(pInBuf, fd, id, size, timestamp, bInputEos, codecdata));
    }


    sp<AMessage> reply;
    (new AMessage(kWhatDecode, this))->postAndAwaitResponse(&reply);
    int32_t err;
    CHECK(reply->findInt32("err", &err));
    return err;
}
status_t VideoDecoderBase::queueOutput(int32_t blockId){
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);

    sp<AMessage> msg = new AMessage(kWhatQueueOutput, this);
    msg->setInt32("blockId", blockId);
    msg->post();

    return OK;
}

status_t VideoDecoderBase::setConfig(DecConfig index, void* pConfig) {
    if (!pConfig)
        return BAD_VALUE;

    switch (index) {
        case DEC_CONFIG_OUTPUT_FORMAT:
            memcpy(&mOutputFormat, pConfig, sizeof(VideoFormat));
            break;
        case DEC_CONFIG_INPUT_FORMAT:
            memcpy(&mInputFormat, pConfig, sizeof(VideoFormat));
            break;
        case DEC_CONFIG_OUTPUT_USAGE:{
            uint64_t* usage = (uint64_t*)pConfig;
            nOutBufferUsage |= *usage;
            break;
            }
        case DEC_CONFIG_OUTPUT_MAX_WIDTH:{
            uint32_t* width = (uint32_t*)pConfig;
            mMaxWidth = *width;
            break;
            }
        case DEC_CONFIG_OUTPUT_MAX_HEIGHT:{
            uint32_t* height = (uint32_t*)pConfig;
            mMaxHeight = *height;
            mMaxOutputBufferSize = mMaxWidth * mMaxHeight * 3/2;
            break;
            }
        default:
            return DoSetConfig(index, pConfig);
    }
    return OK;
}

status_t VideoDecoderBase::getConfig(DecConfig index, void* pConfig) {
    if (!pConfig)
        return BAD_VALUE;

    switch (index) {
        case DEC_CONFIG_OUTPUT_FORMAT:
            memcpy(pConfig, &mOutputFormat, sizeof(VideoFormat));
            break;
        case DEC_CONFIG_INPUT_FORMAT:
            memcpy(pConfig, &mInputFormat, sizeof(VideoFormat));
            break;
        default:
            return DoGetConfig(index, pConfig);
    }
    return OK;
}

status_t VideoDecoderBase::setGraphicBlockPool(const std::shared_ptr<C2BlockPool> &pool) {
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);

    if (pool.get() == nullptr) {
        ALOGE("setGraphicBlockPool get nullptr ! \n");
        return BAD_VALUE;
    }

    mBlockPool = pool;
    return OK;
}

status_t VideoDecoderBase::onInit() {
    /* implement by sub class */
    return OK;
}

status_t VideoDecoderBase::onStart() {
    /* implement by sub class */
    return OK;
}

status_t VideoDecoderBase::onStop() {
    /* implement by sub class */
    return OK;
}

status_t VideoDecoderBase::onFlush() {
    /* implement by sub class */
    return OK;
}

status_t VideoDecoderBase::onDestroy() {
    /* implement by sub class */
    return OK;
}

// input == nullptr, drain output
status_t VideoDecoderBase::decodeInternal(std::unique_ptr<IMXInputBuffer> input) {
    /* implement by sub class */
    (void)input;
    return OK;
}

void VideoDecoderBase::GraphicBlockSetState(int32_t blockId, GraphicBlockInfo::State state)
{
    GraphicBlockInfo* pInfo = getGraphicBlockById(blockId);
    if (pInfo) {
        Mutex::Autolock autoLock(mGBLock);
        pInfo->mState = state;
    }
}


GraphicBlockInfo* VideoDecoderBase::getGraphicBlockById(int32_t blockId) {
    if (blockId < 0 || blockId >= static_cast<int32_t>(mGraphicBlocks.size())) {
        ALOGE("getGraphicBlockById failed: id=%d", blockId);
        return nullptr;
    }
    auto blockIter = std::find_if(mGraphicBlocks.begin(), mGraphicBlocks.end(),
                                  [blockId](const GraphicBlockInfo& gb) {
                                      return gb.mBlockId == blockId;
                                  });

    if (blockIter == mGraphicBlocks.end()) {
        ALOGV("%s line %d: failed: blockId=%d", __FUNCTION__, __LINE__, blockId);
        return nullptr;
    }
    return &(*blockIter);
}

GraphicBlockInfo* VideoDecoderBase::getGraphicBlockByPhysAddr(unsigned long physAddr) {
    if (physAddr == 0) {
        ALOGE("%s line %d: invalid physical address=%p", __FUNCTION__, __LINE__, (void*)physAddr);
        return nullptr;
    }
    auto blockIter = std::find_if(mGraphicBlocks.begin(), mGraphicBlocks.end(),
                                  [physAddr](const GraphicBlockInfo& gb) {
                                      return gb.mPhysAddr == physAddr;
                                  });

    if (blockIter == mGraphicBlocks.end()) {
        ALOGV("%s line %d: failed: physical address=%p", __FUNCTION__, __LINE__, (void*)physAddr);
        return nullptr;
    }
    return &(*blockIter);
}

GraphicBlockInfo* VideoDecoderBase::getFreeGraphicBlock() {
    Mutex::Autolock autoLock(mGBLock);
    auto blockIter = std::find_if(mGraphicBlocks.begin(), mGraphicBlocks.end(),
                                  [](const GraphicBlockInfo& gb) {
                                      return gb.mState == GraphicBlockInfo::State::OWNED_BY_COMPONENT;;
                                  });

    if (blockIter == mGraphicBlocks.end()) {
        ALOGV("%s line %d: failed: no free Graphic Block",  __FUNCTION__, __LINE__);
        return nullptr;
    }

    ALOGV("getFreeGraphicBlock blockId %d", blockIter->mBlockId);
    return &(*blockIter);
}

status_t VideoDecoderBase::removeGraphicBlockById(int32_t blockId) {
    Mutex::Autolock autoLock(mGBLock);
    if (blockId < 0) {
        ALOGE("getGraphicBlockById failed: id=%d", blockId);
        return BAD_INDEX;
    }
    auto blockIter = std::find_if(mGraphicBlocks.begin(), mGraphicBlocks.end(),
                                  [blockId](const GraphicBlockInfo& gb) {
                                      return gb.mBlockId == blockId;
                                  });

    if (blockIter == mGraphicBlocks.end()) {
        ALOGE("%s line %d: failed: blockId=%d", __FUNCTION__, __LINE__, blockId);
        return BAD_INDEX;
    }

    if ((*blockIter).mVirtAddr > 0 && (*blockIter).mCapacity > 0)
        munmap((void*)(*blockIter).mVirtAddr, (*blockIter).mCapacity);

    (*blockIter).mGraphicBlock.reset();
    mGraphicBlocks.erase(blockIter);
    return OK;
}

void VideoDecoderBase::addGraphicBlock(GraphicBlockInfo &info) {
    Mutex::Autolock autoLock(mGBLock);
    mGraphicBlocks.push_back(std::move(info));
}

status_t VideoDecoderBase::outputFormatChanged() {

    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);

    bOutputFmtChangedPending = true;
    status_t err = onOutputFormatChanged();
    if (err == OK)
        bOutputFmtChangedPending = false;

    return err;
}

status_t VideoDecoderBase::fetchOutputBuffer() {
    ATRACE_CALL();
    VDB_API_TRACE("%s, format=%x, line %d", __FUNCTION__, mOutputFormat.pixelFormat, __LINE__);

    C2MemoryUsage usage(nOutBufferUsage);
    uint32_t width = mOutputFormat.width;
    uint32_t height = mOutputFormat.height;

    std::shared_ptr<C2GraphicBlock> outBlock;

    if(bAdaptiveMode){
        width = mMaxWidth;
        height = mMaxHeight;
    }

    c2_status_t c2_err = mBlockPool->fetchGraphicBlock(width, height,
                                                    mOutputFormat.pixelFormat, usage, &outBlock);

    if (c2_err == C2_BLOCKING)
        return WOULD_BLOCK;
    else if (c2_err != C2_OK) {
        ALOGE("fetchGraphicBlock for Output failed with status %d", c2_err);
        return UNKNOWN_ERROR;
    }

    int32_t blockId;
    status_t err = appendOutputBuffer(outBlock, &blockId);
    if (err == NO_MEMORY) {
        // sleep 5ms to wait graphic buffer return from display
        usleep(5000);
        err = WOULD_BLOCK;
    }

    return err;
}

void VideoDecoderBase::returnOutputBufferToDecoder(int32_t blockId) {
    GraphicBlockInfo *gbInfo = getGraphicBlockById(blockId);
    if (gbInfo) {
        ALOGV("%s: blockId %d", __FUNCTION__, blockId);
        gbInfo->mState = GraphicBlockInfo::State::OWNED_BY_COMPONENT;
    } else {
        ALOGE("%s: invalid blockId %d", __FUNCTION__, blockId);
    }
}

void VideoDecoderBase::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatDecode: {
            std::unique_ptr<IMXInputBuffer> input;
            {
                Mutexed<InputBufferQueue>::Locked queue(mInputQueue);
                // there is always have at least one input
                input = std::move(queue->front());
                queue->pop_front(); // pop in NotifyInputBufferUsed ?
            }
            status_t err = decodeInternal(std::move(input));
            Reply(msg, &err);
            break;
        }
        case kWhatInit: {
            int32_t err = onInit();
            Reply(msg, &err);
            break;
        }
        case kWhatStart: {
            int32_t err = onStart();
            bFirstResChange = true;
            Reply(msg, &err);
            break;
        }
        case kWhatStop: {
            int32_t err = onStop();
            Reply(msg, &err);
            break;
        }
        case kWhatFlush: {
            ALOGV("kWhatFlush");
            Mutexed<InputBufferQueue>::Locked queue(mInputQueue);
            while (!queue->empty()) {
                queue->pop_front();
            }

            bCodecDataQueued = false;
            bReleasingDecoder = false;
            bFlushed = true;

            int32_t err = onFlush();
            Reply(msg, &err);
            break;
        }
        case kWhatDestroy: {
            // release resources
            Mutexed<InputBufferQueue>::Locked queue(mInputQueue);
            while (!queue->empty()) {
                queue->pop_front();
            }

            if (pCodecDataBuf) {
                free(pCodecDataBuf);
                pCodecDataBuf = nullptr;
                nCodecDataLen = 0;
            }

            bReleasingDecoder = true;

            int32_t err = onDestroy();
            freeOutputBuffers();
            Reply(msg, &err);
            break;
        }
        case kWhatQueueOutput:
            int32_t blockId;
            CHECK(msg->findInt32("blockId", &blockId));
            returnOutputBufferToDecoder(blockId);
            break;

        default: {
            ALOGW("Unrecognized msg: %d", msg->what());
            break;
        }
    }
}

status_t VideoDecoderBase::appendOutputBuffer(std::shared_ptr<C2GraphicBlock> block, int32_t* blockId) {
    fsl::Memory *prvHandle = (fsl::Memory*)block->handle();

    ALOGV("appendOutputBuffer handle %p, fd %d virt %p, phys %p, format 0x%x, size %d",
        prvHandle, prvHandle->fd, (void*)prvHandle->base, (void*)prvHandle->phys, prvHandle->fslFormat, prvHandle->size);

    GraphicBlockInfo* pInfo = getGraphicBlockByPhysAddr(prvHandle->phys);
    if (pInfo) {
        // previous output buffer returned to decoder
        ALOGV("previous output buffer returned to decoder, blockId %d", pInfo->mBlockId);
        pInfo->mDMABufFd = prvHandle->fd;
        pInfo->mGraphicBlock = std::move(block);
        GraphicBlockSetState(pInfo->mBlockId, GraphicBlockInfo::State::OWNED_BY_COMPONENT);
        *blockId = pInfo->mBlockId;

    } else {
        if (true == OutputBufferFull()) {
            return NO_MEMORY;
        }
        GraphicBlockInfo info;

        if (bSecureMode) {
            info.mVirtAddr  = 0;
        } else {
            uint64_t vaddr;
            int ret = IMXGetBufferAddr(prvHandle->fd, prvHandle->size, vaddr, true);
            if (ret != 0) {
                ALOGE("Ion get virtual address failed, fd %d", prvHandle->fd);
                return BAD_VALUE;
            }
            info.mVirtAddr = (unsigned long)vaddr;
        }

        info.mDMABufFd = prvHandle->fd;
        info.mCapacity = prvHandle->size;
        info.mPhysAddr = prvHandle->phys;
        info.mPixelFormat = prvHandle->fslFormat;
        info.mState = GraphicBlockInfo::State::OWNED_BY_COMPONENT;
        info.mBlockId = static_cast<int32_t>(mGraphicBlocks.size());
        info.mGraphicBlock = std::move(block);
        *blockId = info.mBlockId;
        ALOGI("fetch a new buffer, blockId %d fd %d phys %p virt %p capacity %d",
            info.mBlockId, info.mDMABufFd, (void*)info.mPhysAddr, (void*)info.mVirtAddr, info.mCapacity);

        addGraphicBlock(info);
    }

    return OK;
}

status_t VideoDecoderBase::importOutputBuffers()
{
    return OK;
}

status_t VideoDecoderBase::onOutputFormatChanged() {
    ATRACE_CALL();
    VDB_API_TRACE("%s, line %d", __FUNCTION__, __LINE__);

    ALOGI("New format(pixelfmt=0x%x, min buffers=%u, request buffers %d, w*h=%d x %d, crop=(%d %d %d %d), interlaced %d)",
          mOutputFormat.pixelFormat, mOutputFormat.minBufferNum, mOutputFormat.bufferNum,
          mOutputFormat.width, mOutputFormat.height,
          mOutputFormat.rect.left, mOutputFormat.rect.top,
          mOutputFormat.rect.right, mOutputFormat.rect.bottom, mOutputFormat.interlaced);

    status_t err = OK;
    bool freeBuffer = true;

    if(bAdaptiveMode){
        ALOGI("onOutputFormatChanged mMaxWidth=%d,mMaxHeight=%d,mMaxOutputBufferSize=%d,mMaxOutputBufferCnt=%d",
            mMaxWidth, mMaxHeight, mMaxOutputBufferSize, mMaxOutputBufferCnt);

        freeBuffer = false;

        if(mMaxWidth < mOutputFormat.width || mMaxHeight < mOutputFormat.height){
            mMaxWidth = mOutputFormat.width;
            mMaxHeight = mOutputFormat.height;
            freeBuffer = true;
            ALOGV("mMaxWidth freeBuffer");
        }

        if(mMaxOutputBufferSize < mOutputFormat.bufferSize){
            mMaxOutputBufferSize = mOutputFormat.bufferSize;
            freeBuffer = true;
            ALOGV("mMaxOutputBufferSize freeBuffer");
        }

        //fetch buffer can increase buffer count dynamically, so no need to free buffers
        if(mMaxOutputBufferCnt < mOutputFormat.bufferNum){
            mMaxOutputBufferCnt = mOutputFormat.bufferNum;
        }

        if(bFirstResChange){
            bFirstResChange = false;
            freeBuffer = true;
            ALOGV("bFirstResChange");
        }

    }

    mClient->notifyVideoInfo(&mOutputFormat);

    for (auto& info : mGraphicBlocks) {
        if (info.mState == GraphicBlockInfo::State::OWNED_BY_VPU)
            info.mState = GraphicBlockInfo::State::OWNED_BY_COMPONENT;
        if(info.mPixelFormat != mOutputFormat.pixelFormat){
            ALOGV("pixelFormat freeBuffer");
            freeBuffer = true;
        }
    }

    for (const auto& info : mGraphicBlocks) {
        CHECK(info.mState != GraphicBlockInfo::State::OWNED_BY_VPU);
    }

    if(freeBuffer){
        err = freeOutputBuffers();
        ALOGD("onOutputFormatChanged freeOutputBuffers");
    }else{
        ALOGD("onOutputFormatChanged use old buffers");
    }

    if (err) {
        NotifyError(err);
        return err;
    }

    importOutputBuffers();

    return OK;
}

void VideoDecoderBase::NotifyFlushDone () {
    mClient->notifyFlushDone();
}

void VideoDecoderBase::NotifyInputBufferUsed(int32_t input_id) {
    mClient->notifyInputBufferUsed(input_id);
}

void VideoDecoderBase::NotifySkipInputBuffer(int32_t input_id) {
    mClient->notifySkipInputBuffer(input_id);
}

void VideoDecoderBase::NotifyPictureReady(int32_t pictureId, uint64_t timestamp) {
    if (bReleasingDecoder)
        returnOutputBufferToDecoder(pictureId);
    else {
        GraphicBlockInfo* info = getGraphicBlockById(pictureId);
        if (!info) {
            /* notify error */
            ALOGE("%s line %d: wrong pictureId %d", __FUNCTION__, __LINE__, pictureId);
            return;
        }

        if (info->mState != GraphicBlockInfo::State::OWNED_BY_VPU) {
            ALOGE("%s line %d: error graphic block state, expect OWNED_BY_VPU but get %d", __FUNCTION__, __LINE__, info->mState);
            return;
        }

        GraphicBlockSetState(pictureId, GraphicBlockInfo::State::OWNED_BY_CLIENT);

        Mutex::Autolock autoLock(mGBLock);
        mClient->notifyPictureReady(pictureId, timestamp);
    }
}

void VideoDecoderBase::NotifyEOS() {
    mClient->notifyEos();
}

void VideoDecoderBase::NotifyError(status_t err) {
    mClient->notifyError(err);
}

int VideoDecoderBase::getBlockPoolAllocatorId()
{
    if(mBlockPool != nullptr)
        return mBlockPool->getAllocatorId();

    return -1;
}

int VideoDecoderBase::migrateGraphicBuffers()
{
    int32_t i = 0;
    std::vector<int32_t> droppedBlockIds;

    for (auto& info : mGraphicBlocks) {
        if (info.mState == GraphicBlockInfo::State::OWNED_BY_CLIENT)
            droppedBlockIds.push_back(info.mBlockId);
    }

    for (auto& id : droppedBlockIds) {
        status_t ret = removeGraphicBlockById(id);
        ALOGV("remove block %d, ret %d", id, ret);
    }

    for (auto& info : mGraphicBlocks) {
        info.mBlockId = i++;
    }

    ALOGV("migrateGraphicBuffers %d", i);

    return i;
}

} // namespace android

/* end of file */
