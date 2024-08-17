/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */


//#define LOG_NDEBUG 0
#define LOG_TAG "FrameFetcher"

#include <utils/Log.h>
#include "FrameFetcher.h"
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ADebug.h>


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

FrameFetcher::FrameFetcher():
    mLooper(new ALooper),
    mState(UNINITIALIZED),
    mBlockPool(nullptr),
    mWidth(0),
    mHeight(0),
    mFormat(0),
    mUsage(0)
{

    mLooper->setName("FrameFetcher");
    mLooper->start(false, false, ANDROID_PRIORITY_VIDEO);

    ALOGV("FrameFetcher::FrameFetcher");
}
status_t FrameFetcher::Init(const std::shared_ptr<C2BlockPool>& pool){
    (void)mLooper->registerHandler(this);
    mBlockPool = std::move(pool);
    ALOGV("FrameFetcher::Init pool id=%lld",(long long)mBlockPool->getLocalId());
    return OK;
}
status_t FrameFetcher::SetParam(uint32_t width, uint32_t height, uint32_t format, uint64_t usage){
    mWidth = width;
    mHeight = height;
    mFormat = format;
    mUsage = usage;
    ALOGV("SetParam mWidth=%d,mHeight=%d,mFormat=%x,usage=%llx",
        mWidth, mHeight, mFormat, (long long)mUsage);

    return OK;
}

FrameFetcher::~FrameFetcher() {
    if (mLooper != nullptr) {
        mLooper->unregisterHandler(id());
        mLooper->stop();
    }
    while(!mReadyBlocks.empty())
        mReadyBlocks.pop();
    ALOGV("FrameFetcher::~FrameFetcher");
}
status_t FrameFetcher::Start(){

    if(mState == RUNNING)
        return OK;

    sp<AMessage> reply;
    (new AMessage(kWhatStart, this))->postAndAwaitResponse(&reply);

    int32_t err = 0;
    CHECK(reply->findInt32("err", &err));
    postFetch();
    return err;
}
status_t FrameFetcher::Stop() {

    if(mState == STOPPED)
        return OK;

    sp<AMessage> reply;
    (new AMessage(kWhatStop, this))->postAndAwaitResponse(&reply);
    int32_t err;
    CHECK(reply->findInt32("err", &err));

    return err;
}
bool FrameFetcher::Running(){
    return (mState == RUNNING);
}

status_t FrameFetcher::HasFrames(){
    Mutex::Autolock autoLock(mBufferLock);

    if(mState == ERROR)
        return UNKNOWN_ERROR;

    if(mReadyBlocks.empty())
        return TIMED_OUT;
    else
        return OK;
}
status_t FrameFetcher::GetFrameBlock(std::shared_ptr<C2GraphicBlock>*block){
    Mutex::Autolock autoLock(mBufferLock);
    if(mReadyBlocks.size() > 0){
        *block = mReadyBlocks.front();
        mReadyBlocks.pop();
        return OK;
    }
    return UNKNOWN_ERROR;
}
void FrameFetcher::postFetch(int64_t delay){
    (new AMessage(kWhatFetch, this))->post(delay);
}

void FrameFetcher::onMessageReceived(const sp<AMessage> &msg){
    switch (msg->what()) {
        case kWhatStart: {

            int32_t err = 0;
            mState = RUNNING;

            Reply(msg, &err);
            break;
        }
        case kWhatStop: {
            int32_t err = 0;
            mState = STOPPING;
            {
                Mutex::Autolock autoLock(mBufferLock);
                while(!mReadyBlocks.empty())
                    mReadyBlocks.pop();
            }

            mState = STOPPED;
            Reply(msg, &err);
            break;
        }
        case kWhatFetch: {

            if(mState != RUNNING)
                break;

            {
                Mutex::Autolock autoLock(mBufferLock);
                if (mReadyBlocks.size() > kMaxBufferCount){
                    postFetch(kPostDelay);
                    break;
                }
            }

            std::shared_ptr<C2GraphicBlock> block;
            C2MemoryUsage tar_usage(mUsage);
            c2_status_t c2_err = mBlockPool->fetchGraphicBlock(
                mWidth, mHeight, mFormat, tar_usage, &block);

            if(c2_err == OK){
                Mutex::Autolock autoLock(mBufferLock);
                mReadyBlocks.push(block);
                postFetch();
                break;
            }else if(c2_err != C2_BLOCKING){
                ALOGE("fetchGraphicBlock failed c2_err=%d",c2_err);
                mState = ERROR;
                break;
            }else{
                postFetch(kPostDelay);
                break;
            }
        }
        default: {
            ALOGE("unknown msg=%d",msg->what());
            break;
        }
    }
}

}
