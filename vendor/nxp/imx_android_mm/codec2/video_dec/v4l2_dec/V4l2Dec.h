/**
 *  Copyright 2019-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#ifndef V4L2_DECODER_H
#define V4L2_DECODER_H

#include <queue>
#include <map>
#include <semaphore.h>
#include "VideoDecoderBase.h"
#include "V4l2Dev.h"
#include "V4l2Poller.h"
#include "CodecDataHandler.h"

namespace android {

class V4l2Dec : public VideoDecoderBase{
public:
    V4l2Dec(const char* mime);
    status_t onInit() override;
    status_t onStart() override;
    status_t onFlush() override;
    status_t onStop() override;
    status_t onDestroy() override;
    bool canProcessWork() override;

    status_t prepareOutputBuffers();
    status_t destroyOutputBuffers();
    status_t decodeInternal(std::unique_ptr<IMXInputBuffer> input);
    //status_t queueInput(C2FrameData * input, int32_t input_id);
    status_t queueOutput(GraphicBlockInfo* pInfo);
    status_t queueOutput(int index);
    status_t dequeueInputBuffer();
    status_t dequeueOutputBuffer();

    status_t importOutputBuffers() override;

protected:
    virtual ~V4l2Dec();
    status_t DoSetConfig(DecConfig index, void* pConfig) override;
    status_t DoGetConfig(DecConfig index, void* pConfig) override;

    status_t freeOutputBuffers() override;
    bool OutputBufferFull() override;
    void detectPostProcess(int pixelFormat);

private:

    class TimestampHandler {
        public:
            TimestampHandler();
            ~TimestampHandler();
            void pushTimestamp(uint64_t timestamp, uint32_t size, uint32_t id);
            void validTimestamp(uint32_t id);
            uint64_t popTimestamp();
            void flushTimestamp();
            void dropTimestampInc(uint32_t count);
        private:
            Mutex mTsLock;
            std::map<uint32_t, uint32_t> mInputIdLenMap;
            void* mTsmHandle;
            bool bResyncTsm;
            uint32_t mDroppedCount;
    };

    enum {
        kInputBufferPlaneNum = 1,
        kOutputBufferPlaneNum = 1,

    };

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


    struct OutputRecord {
        OutputRecord();
        OutputRecord(OutputRecord&&) = default;
        ~OutputRecord();
        bool at_device;
        VideoFramePlane planes[kOutputBufferPlaneNum];
        int32_t picture_id;     // picture buffer id as returned to PictureReady().
        uint32_t flag;
    };

    enum {
        UNINITIALIZED,
        STOPPED,
        RUNNING,
        STOPPING,
        FLUSHING,
        RES_CHANGING,
        RES_CHANGED
    };

    const char* mMime;
    pthread_t mFetchThread;

    V4l2Dev* pDev;
    int32_t mFd;
    enum v4l2_buf_type mOutBufType;
    enum v4l2_buf_type mCapBufType;

    enum v4l2_memory mInMemType;//support userptr and dma
    enum v4l2_memory mOutMemType;//support userptr and dma


    uint32_t mInFormat;//v4l2 input format
    std::vector<InputRecord> mInputBufferMap;
    std::vector<OutputRecord> mOutputBufferMap;
    std::queue<int> mFreeInputIndex;

    Mutex mLock;
    Mutex mThreadLock;
    sem_t mSemaphore;

    bool bFetchStarted;
    bool bFetchStopped;

    bool bInputStreamOn;
    bool bOutputStreamOn;

    bool bH264;
    bool bLowLatency;

    bool bSawInputEos;
    bool bNewSegment;
    bool bPendingFlush;
    bool bForcePixelFormat;
    bool bHasColorAspect;
    bool bHasResChange;

    int mState;

    uint32_t mVpuOwnedOutputBufferNum;
    uint32_t mRegisteredOutBufNum;

    uint32_t mOutputPlaneSize[kOutputBufferPlaneNum];
    uint32_t mOutFormat;//v4l2 output format

    uint32_t mVc1Format;

    uint32_t mInCnt;
    uint32_t mOutCnt;
    uint32_t mLastOutputSeq;
    uint32_t nDebugFlag;
    uint32_t mUVOffset;
    uint32_t mFrameAlignW;
    uint32_t mFrameAlignH;
    uint32_t mPollTimeoutCnt;

    VideoColorAspect mIsoColorAspect;

    bool bHasHdr10StaticInfo;
    DecStaticHDRInfo sHdr10StaticInfo;

    sp<V4l2Poller> mPoller;
    TimestampHandler * mTsHandler;
    std::unique_ptr<CodecDataHandler> mCodecDataHandler;

    status_t getV4l2MinBufferCount(enum v4l2_buf_type type);

    status_t prepareInputParams();
    status_t SetInputFormats();
    status_t prepareOutputParams();
    status_t SetOutputFormats();

    status_t prepareInputBuffers();
    status_t createInputBuffers();
    status_t destroyInputBuffers();

    status_t createFetchThread();
    status_t destroyFetchThread();

    status_t startInputStream();
    status_t stopInputStream();
    status_t startOutputStream();
    status_t stopOutputStream();

    status_t onDequeueEvent();

    void handleDecEos();

    static status_t PollThreadWrapper(void *me, status_t poll_ret);
    status_t HandlePollThread(status_t poll_ret);

    static void *FetchThreadWrapper(void *);
    status_t HandleFetchThread();

    status_t handleFormatChanged();
    status_t getOutputParams();

    void ParseVpuLogLevel();
    void dumpInputBuffer(void* inBuf, uint32_t size);
    void dumpOutputBuffer(void* buf, uint32_t size);
    void dumpInputBuffer(int fd, uint32_t size);

    void migrateOutputBuffers();
    bool allInputBuffersInVPU();
    bool isDeviceHang(uint32_t pollTimeOutCnt);
    bool isDeviceEos();
    status_t getVPXHeader(uint32_t width, uint32_t height, uint32_t format, char* header);
};



}
#endif
