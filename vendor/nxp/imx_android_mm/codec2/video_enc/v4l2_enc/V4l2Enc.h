/**
 *  Copyright 2019-2021 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#ifndef V4L2_ENCODER_H
#define V4L2_ENCODER_H

#include "VideoEncoderBase.h"
#include "V4l2Dev.h"
#include "FrameConverter.h"

namespace android {

class V4l2Enc : public VideoEncoderBase{
public:
    V4l2Enc(const char* mime);

    status_t prepareOutputBuffers();
    status_t destroyOutputBuffers();
    //status_t queueInput(C2FrameData * input, int32_t input_id);
    status_t queueOutput(int buffer_id);
    //status_t setOutputBuffer(int bufferId, unsigned long pPhyAddr, unsigned long pVirtAddr, uint32_t capacity)
    status_t dequeueInputBuffer();
    status_t dequeueOutputBuffer();


protected:
    virtual ~V4l2Enc();
    status_t DoSetConfig(EncConfig index, void* pConfig) override;
    status_t DoGetConfig(EncConfig index, void* pConfig) override;
    void initEncInputParamter(EncInputParam *pInPara) override;
    status_t getCodecData(uint8_t** pCodecData, uint32_t* size) override;
    bool checkIfPreProcessNeeded(int pixelFormat) override;
    status_t OpenDevice() override;

    status_t onInit() override;
    status_t onStart();
    status_t onStop() override;//idle to loaded
    status_t onFlush() override;//flush
    status_t onDestroy() override;//free Buffers

    status_t encodeInternal(std::unique_ptr<IMXInputBuffer> input) override;

private:
    enum {
        kMaxInputBufferPlaneNum = 3,
        kInputBufferPlaneNum = 1,
        kOutputBufferPlaneNum = 1,
    };

    struct VideoFramePlane {
        int32_t fd;
        uint64_t addr;
        uint32_t size;
        uint32_t length;
        uint32_t offset;
    };

    // Record for input buffers.
    struct InputRecord {
        InputRecord();
        ~InputRecord();
        bool at_device;    // held by device.
        VideoFramePlane planes[kMaxInputBufferPlaneNum];
        int32_t input_id;
        int64_t ts;
    };


    struct OutputRecord {
        OutputRecord();
        OutputRecord(OutputRecord&&) = default;
        ~OutputRecord();
        bool at_device;
        VideoFramePlane plane;
        int32_t picture_id;     // picture buffer id as returned to PictureReady().
        uint32_t flag;
        //std::shared_ptr<C2GraphicBlock> mGraphicBlock;
    };

    enum {
        UNINITIALIZED,
        PREPARED,
        STOPPED,
        RUNNING,
        STOPPING,
        FLUSHING,
    };

    const char* mMime;
    pthread_t mPollThread;

    V4l2Dev* pDev;
    int32_t mFd;

    enum v4l2_memory mInMemType;//support userptr and dma
    enum v4l2_memory mOutMemType;//support userptr and dma

    enum v4l2_buf_type mInBufType;
    enum v4l2_buf_type mOutBufType;

    uint32_t mInPlaneNum;
    uint32_t mOutPlaneNum;

    V4l2EncInputParam mEncParam;
    uint32_t mTargetFps;


    uint32_t mInFormat;//v4l2 output format
    uint32_t mOutFormat;//v4l2 capture format
    uint32_t mInputPlaneSize[kMaxInputBufferPlaneNum];
    uint32_t mWidthAlign;
    uint32_t mHeightAlign;
    uint32_t mCropWidth;
    uint32_t mCropHeight;

    std::vector<InputRecord> mInputBufferMap;
    std::vector<OutputRecord> mOutputBufferMap;

    Mutex mLock;

    bool bPollStarted;

    bool bInputStreamOn;
    bool bOutputStreamOn;

    bool bSyncFrame;
    bool bHasCodecData;
    bool bStarted;
    bool bPreProcess;
    bool bNeedFrameConverter;
    FrameConverter * mConverter;

    uint32_t mFrameInNum;
    uint32_t mFrameOutNum;
    uint32_t nDebugFlag;
    int mState;

    VideoColorAspect mIsoColorAspect;

    status_t prepareInputParams();
    status_t SetInputFormats();
    status_t prepareOutputParams();
    status_t SetOutputFormats();

    status_t prepareInputBuffers();
    status_t createInputBuffers();
    status_t destroyInputBuffers();

    status_t createPollThread();
    status_t destroyPollThread();
    status_t createFetchThread();
    status_t destroyFetchThread();

    status_t startInputStream();
    status_t stopInputStream();
    status_t startOutputStream();
    status_t stopOutputStream();

    status_t onDequeueEvent();

    static void *PollThreadWrapper(void *);
    status_t HandlePollThread();
    static void *FetchThreadWrapper(void *);
    status_t HandleFetchThread();

    status_t handleFormatChanged();
    status_t allocateOutputBuffer(int32_t index);
    status_t freeOutputBuffers();

    void ParseVpuLogLevel();
    void dumpInputBuffer(int fd, uint32_t size);
    void dumpOutputBuffer(void* buf, uint32_t size);
};



}
#endif
