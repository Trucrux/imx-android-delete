/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef G2D_FILTER_H
#define G2D_FILTER_H

#include "IMXC2ComponentBase.h"
#include "IsiFilterInterface.h"
#include <queue>
#include "FrameFetcher.h"
#include "V4l2Dev.h"
#include "V4l2Poller.h"

namespace android {

class IsiFilter : public IMXC2ComponentBase{
public:
    explicit IsiFilter(c2_node_id_t id, C2String name, const std::shared_ptr<C2ReflectorHelper>& helper, const std::shared_ptr<IsiFilterInterface> &intfImpl);

    virtual ~IsiFilter();

    // From IMXC2Component
    c2_status_t onInit() override;
    void onStart() override;
    c2_status_t onStop() override;
    c2_status_t onFlush_sm() override;
    c2_status_t drainInternal(uint32_t drainMode) override;
    void onReset() override;
    void onRelease() override;
    void processWork(const std::unique_ptr<C2Work> &work) override;
    c2_status_t canProcessWork() override;

private:

    enum {
        kInputBufferPlaneNum = 1,
        kOutputBufferPlaneNum = 1,
        kInputBufferNum = 16,
        kOutputBufferNum = 3,
    };

    // The pointer of component interface.
    const std::shared_ptr<IsiFilterInterface> mIntfImpl;
    int32_t mFd;

    V4l2Dev * mDev;
    sp<V4l2Poller> mPoller;
    sp<FrameFetcher> mFetcher;
    std::shared_ptr<C2BlockPool> mBlockPool;

    Mutex mIsiLock;

    enum v4l2_memory mInMemType;//support userptr and dma
    enum v4l2_memory mOutMemType;//support userptr and dma

    uint32_t mInFormat;//v4l2 output format
    uint32_t mOutFormat;//v4l2 capture format
    uint32_t mOutputPlaneSize[kOutputBufferPlaneNum];

    uint32_t mWidthAlign;
    uint32_t mHeightAlign;

    uint32_t mSourceFormat;//input hal format
    uint32_t mTargetFormat;//output hal format
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mWidthWithAlign;
    uint32_t mHeightWithAlign;
    uint32_t mSrcBufferSize;
    uint32_t mTarBufferSize;

    uint32_t mDebugFlag;
    uint32_t mIsiOutNum;
    uint32_t mInNum;
    uint32_t mOutNum;
    int32_t mConfigID;

    bool mPassthrough;
    bool mStart;
    bool mEOS;//empty buffer with eos flag

    bool mInputStreamOn;
    bool mOutputStreamOn;

    bool mSendConfig;
    bool mSyncFrame;
    bool mBitrateChange;

    std::queue<int> mFreeInputIndex;
    std::queue<int> mFreeOutputIndex;

    std::queue<int32_t> mInputQueue;
    std::queue<int> mOutputQueue;

    std::shared_ptr<C2StreamRequestSyncFrameTuning::output> mRequestSync; //always true
    std::shared_ptr<C2StreamBitrateInfo::output> mBitrate;

    struct VideoFramePlane {
        int32_t fd;
        uint64_t vaddr;
        uint64_t paddr;
        uint32_t size;
        uint32_t length;
        uint32_t offset;
    };

    // Record for input buffers.
    struct InputRecord {
        InputRecord();
        ~InputRecord();
        bool at_device;    // held by device.
        VideoFramePlane plane;
        int32_t input_id;
        int64_t ts;
    };

    std::vector<InputRecord> mInputBufferMap;
    std::vector<std::shared_ptr<C2GraphicBlock>> mOutputBufferMap;

    void handleEmptyBuffer(const std::unique_ptr<C2Work> &work);
    void handlePassThroughBuffer(const std::unique_ptr<C2Work> &work);
    status_t updateParam(const std::unique_ptr<C2Work> &work);
    status_t verifyFormat(bool source, uint32_t format);
    status_t startInternal();
    uint32_t getV4l2Format(uint32_t color_format);
    status_t prepareInputParams();
    status_t prepareOutputParams();
    status_t setInputFormats();
    status_t setOutputFormats();
    status_t prepareInputBuffers();
    status_t prepareOutputBuffers();
    status_t createFrameFetcher();
    status_t createPoller();
    static status_t PollThreadWrapper(void *me, status_t poll_ret);
    status_t HandlePollThread(status_t poll_ret);
    status_t getDynamicConfigParam(int32_t index);
    status_t queueInput(const std::unique_ptr<C2Work> &work);
    status_t queueOutput();
    status_t dequeueInputBuffer();
    status_t dequeueOutputBuffer();
    status_t handleOutputFrame(uint32_t index, uint64_t timestamp, bool updateConfig);
    status_t startInputStream();
    status_t stopInputStream();
    status_t startOutputStream();
    status_t stopOutputStream();
    status_t destroyInputBuffers();
    status_t destroyOutputBuffers();
    bool allInputBuffersInIsi();
    void dumpInputBuffer(int fd, uint32_t size);
    void dumpOutputBuffer(int fd, uint32_t size);

};
} // namespace android
#endif
