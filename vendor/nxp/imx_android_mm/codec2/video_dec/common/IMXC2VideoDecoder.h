/**
 *  Copyright 2019-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef IMX_VIDEO_DECODER_H_
#define IMX_VIDEO_DECODER_H_

#include <log/log.h>

#include <media/stagefright/foundation/MediaDefs.h>

#include <C2Debug.h>
#include <C2PlatformSupport.h>
#include <IMXC2Interface.h>

#include "IMXC2ComponentBase.h"
#include "VideoDecoderBase.h"

namespace android {

class IMXC2VideoDecoder : public IMXC2ComponentBase,
						        public VideoDecoderBase::Client {
public:

	class IntfImpl;

    IMXC2VideoDecoder(const char* name, c2_node_id_t id, const std::shared_ptr<IntfImpl> &intfImpl);
    virtual ~IMXC2VideoDecoder();

	// from VideoDecoderBase
	void notifyVideoInfo(VideoFormat *pformat) override;
    void notifyPictureReady(int32_t pictureId, uint64_t timestamp) override;
    void notifyInputBufferUsed(int32_t input_id) override;
    void notifySkipInputBuffer(int32_t input_id) override;
    void notifyFlushDone() override;
    void notifyResetDone() override;
    void notifyError(status_t err) override;
    void notifyEos() override;

    void outputBufferReturned(int32_t pictureId);

protected:

	// From IMXC2Component
    c2_status_t onInit() override;
    c2_status_t onStop() override;
    c2_status_t onFlush_sm() override;
    void onReset() override;
    void onRelease() override;
    c2_status_t canProcessWork() override;
    void processWork(const std::unique_ptr<C2Work> &work) override;
    c2_status_t drainInternal(uint32_t drainMode) override;
    void onStart() override;


private:

	sp<VideoDecoderBase> mDecoder;
	std::shared_ptr<IntfImpl> mIntf;
	// The vector of storing allocated output graphic block information.
    std::vector<std::shared_ptr<C2GraphicBlock>> mOutputBufferMap;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mCropWidth;
    uint32_t mCropHeight;
    uint32_t nOutBufferNum;

	C2String mName; //component name or role name ?
    bool bPendingFmtChanged;
    bool bGetGraphicBlockPool;
    bool bSignalOutputEos;
    bool bSignalledError;
    bool bFlushDone;
    bool bSupportColorAspects;
    bool bSecure;
    bool bFirstAfterStart;
    bool bFirstAfterFlush;
    bool bDmaInput;
    bool bImx8m;
    bool bImx8mSecure;

    status_t initInternalParam();    // init internel paramters
    status_t setInternalParam();
    void releaseDecoder();    // release decoder instance

    void handleOutputPicture(GraphicBlockInfo* info, uint64_t timestamp, uint32_t flag);
};


} // namespace android

#endif  // IMX_VIDEO_DECODER_H_
