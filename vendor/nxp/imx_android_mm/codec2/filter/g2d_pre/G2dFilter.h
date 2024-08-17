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
#include "G2dFilterInterface.h"
#include "G2dDevice.h"
#include <queue>
#include "FrameFetcher.h"

namespace android {


class G2dFilter : public IMXC2ComponentBase{
public:
    explicit G2dFilter(c2_node_id_t id, C2String name, const std::shared_ptr<C2ReflectorHelper>& helper, const std::shared_ptr<G2dFilterInterface> &intfImpl);

    virtual ~G2dFilter();

    // From IMXC2Component
    c2_status_t onInit() override;
    void onStart() override;
    c2_status_t onStop() override;
    c2_status_t onFlush_sm() override;
    c2_status_t drainInternal(uint32_t drainMode) override;
    void onReset() override;
    void onRelease() override;
    void processWork(const std::unique_ptr<C2Work> &work) override;


private:
    // The pointer of component interface.
    const std::shared_ptr<G2dFilterInterface> mIntfImpl;
    sp<FrameFetcher> mFetcher;
    std::shared_ptr<C2BlockPool> mBlockPool;

    std::unique_ptr<G2DDevice> pDev;

    uint32_t mSourceFormat;
    uint32_t mTargetFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mWidthWithAlign;
    uint32_t mHeightWithAlign;
    uint32_t mSrcBufferSize;
    uint32_t mTarBufferSize;

    uint32_t mDebugFlag;
    uint32_t mOutCnt;

    bool mPassthrough;
    bool mStart;
    bool mEOS;//empty buffer with eos flag

    bool mSendConfig;
    bool mSyncFrame;
    bool mBitrateChange;

    std::shared_ptr<C2StreamRequestSyncFrameTuning::output> mRequestSync; //always true
    std::shared_ptr<C2StreamBitrateInfo::output> mBitrate;


    status_t verifyFormat(bool source, uint32_t format);
    status_t createFrameFetcher();
    void handleEmptyBuffer(const std::unique_ptr<C2Work> &work);
    void handlePassThroughBuffer(const std::unique_ptr<C2Work> &work);
    status_t checkDynamicConfigParam();
    status_t updateParam(const std::unique_ptr<C2Work> &work);
    status_t setTargetFormat();
    status_t startInternal();
    status_t setParameters();

    void dumpInputBuffer(int fd, uint32_t size);
    void dumpOutputBuffer(int fd, uint32_t size);
};
} // namespace android
#endif
