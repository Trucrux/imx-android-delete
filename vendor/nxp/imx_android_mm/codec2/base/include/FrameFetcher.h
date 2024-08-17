/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef FRAME_FECHER_H
#define FRAME_FECHER_H

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/Mutexed.h>
#include <queue>
#include <C2PlatformSupport.h>
#include <C2Buffer.h>

namespace android {

class FrameFetcher : public AHandler {
public:
    FrameFetcher();
    virtual ~FrameFetcher();
	status_t Init(const std::shared_ptr<C2BlockPool>& pool);
	status_t SetParam(uint32_t width, uint32_t height, uint32_t format, uint64_t usage);
    status_t Start();
    status_t Stop();
	bool Running();
    status_t HasFrames();
	status_t GetFrameBlock(std::shared_ptr<C2GraphicBlock>*block);

private:

    enum {
        kWhatStart,
        kWhatFetch,
        kWhatStop,
    };

    enum {
        UNINITIALIZED,
		RUNNING,
        STOPPED,
        STOPPING,
        ERROR,
    };

    enum {
        kMaxBufferCount = 2,
        kPostDelay = 5000,//us
    };

    sp<ALooper> mLooper;
    int mState;

    std::shared_ptr<C2BlockPool> mBlockPool;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mFormat;
    uint64_t mUsage;

    Mutex mBufferLock;
    std::queue<std::shared_ptr<C2GraphicBlock>> mReadyBlocks;

    void postFetch(int64_t delay = 0);
    void onMessageReceived(const sp<AMessage> &msg) override;
};
}
#endif//FRAME_FECHER_H
