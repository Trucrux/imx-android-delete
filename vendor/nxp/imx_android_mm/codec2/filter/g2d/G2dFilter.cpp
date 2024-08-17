/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "G2dFilter"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Trace.h>
#include <utils/Log.h>
#include "G2dFilter.h"
#include <IMXC2Interface.h>
#include "graphics_ext.h"
#include "Memory.h"
#include <C2PlatformSupport.h>
#include <C2Buffer.h>
#include <C2Work.h>
#include <cutils/properties.h>
#include "IMXUtils.h"


#define G2D_FILTER_DEBUG_FLAG "vendor.media.nxp.filter.g2d.debug"
#define G2D_FILTER_DUMP_INPUT 0x1
#define G2D_FILTER_DUMP_OUTPUT 0x2
#define G2D_FILTER_DEBUG_TS 0x4

#define G2D_FILTER_INPUT_FILE "/data/g2d_filter_in.yuv"
#define G2D_FILTER_OUTPUT_FILE "/data/g2d_filter_out.yuv"


namespace android {

constexpr uint32_t k10bitTargetFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;

static const uint32_t source_format_table[]={
    HAL_PIXEL_FORMAT_NV12_TILED,//0x104
    HAL_PIXEL_FORMAT_P010_TILED,//0x109
};
static const uint32_t destination_format_table[]={
    HAL_PIXEL_FORMAT_YCbCr_420_SP,//0x103, default
    HAL_PIXEL_FORMAT_NV12_TILED,  //0x104
    HAL_PIXEL_FORMAT_YCBCR_422_I, //0x14
};

G2dFilter::G2dFilter(c2_node_id_t id, C2String name, const std::shared_ptr<C2ReflectorHelper>& helper, const std::shared_ptr<G2dFilterInterface> &intfImpl)
    : IMXC2ComponentBase(std::make_shared<IMXInterface<G2dFilterInterface>>(name, id, intfImpl)),
    mIntfImpl(intfImpl),
    mFetcher(nullptr),
    pDev(nullptr),
    mSourceFormat(0),
    mTargetFormat(0),
    mWidth(0),
    mHeight(0),
    mSrcBufferSize(0),
    mTarBufferSize(0),
    mInterlace(0),
    mSrcUVOffset(0),
    mSrcStrideAlign(0),
    mCropWidth(0),
    mCropHeight(0),
    mOutCnt(0),
    mDevReady(false),
    mResChange(false),
    mPassthrough(false),
    mImx8qm(false)
{
    (void)helper;
}
G2dFilter::~G2dFilter(){
    ALOGV("%s", __FUNCTION__);
    onRelease();
}

c2_status_t G2dFilter::onInit() {
    ALOGV("%s", __FUNCTION__);

    mDebugFlag = property_get_int32(G2D_FILTER_DEBUG_FLAG, 0);

    if(pDev.get())
        pDev.release();

    pDev = std::make_unique<G2DDevice>(mDebugFlag & G2D_FILTER_DEBUG_TS);

    if(!pDev)
        return C2_CORRUPTED;

    // reset parameters
    mSourceFormat = 0;
    mTargetFormat = 0;
    mWidth = 0;
    mHeight = 0;
    mSrcBufferSize = 0;
    mTarBufferSize = 0;
    mInterlace = 0;
    mSrcUVOffset = 0;
    mSrcStrideAlign = 256; // TODO: get from parameter
    mCropWidth = 0;
    mCropHeight = 0;
    mOutCnt = 0;
    mDevReady = false;
    mResChange = false;
    mPassthrough = false;

    int32_t value = property_get_int32("vendor.media.vpu.output.format", 1);
    //debug for non-default format, 0x103: NV12 linear, 0x14:YUYV, 0x104: NV12 tile
    if(value != 1){
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        C2StreamVendorHalPixelFormat::output fmt(0u, value);
        (void)mIntfImpl->config({&fmt}, C2_MAY_BLOCK, &failures);
        ALOGI("debug filter output format 0x%x",value);
    }

    char socId[20];

    if (0 == GetSocId(socId, sizeof(socId))) {
        if (!strncmp(socId, "i.MX8QM", 7)) {
            mImx8qm = true;
        }
    }

    return C2_OK;
}
void G2dFilter::onStart()
{
    ALOGV("onStart");
}
c2_status_t G2dFilter::onStop()
{
    ALOGV("onStop");

    pDev->getRunningTimeAndFlush();
    if(mFetcher)
        mFetcher->Stop();

    return C2_OK;
}
c2_status_t G2dFilter::onFlush_sm(){
    ALOGV("onFlush_sm");
    if(mFetcher && mFetcher->Running())
        mFetcher->Stop();

    return C2_OK;
}
c2_status_t G2dFilter::drainInternal(uint32_t drainMode){
    ALOGV("drainInternal");
    (void)drainMode;
    return C2_OK;
}
void G2dFilter::onReset(){
    ALOGV("onReset");
}
status_t G2dFilter::handleConfigUpdate(std::vector<std::unique_ptr<C2Param>> &configUpdate)
{
    status_t ret = OK;
    ALOGV("handleInputConfigUpdate input size=%zu",configUpdate.size());

    for (const std::unique_ptr<C2Param> &param : configUpdate) {
        switch((*param).coreIndex().coreIndex()){
            case C2StreamPixelFormatInfo::CORE_INDEX:{
                C2StreamPixelFormatInfo::output format(0u, 0);
                if (format.updateFrom(*param)) {
                    if(mSourceFormat != format.value)
                        mResChange = true;
                    mSourceFormat = format.value;
                }
                break;
            }
            case C2StreamPictureSizeInfo::CORE_INDEX:{
                C2StreamPictureSizeInfo::output size(0u, 0, 0);
                if (size.updateFrom(*param)) {
                    if(mWidth != size.width || mHeight != size.height)
                        mResChange = true;
                    mWidth = size.width;
                    mHeight = size.height;
                }
                break;
            }
            case C2StreamCropRectInfo::CORE_INDEX:{
                C2StreamCropRectInfo::output crop(0u, C2Rect(0, 0));
                if (crop.updateFrom(*param)) {
                    mCropWidth = crop.width;
                    mCropHeight = crop.height;
                }
                break;
            }
            case C2StreamVendorInterlacedFormat::CORE_INDEX:{
                C2StreamVendorInterlacedFormat::output interlace(0u, 0);
                if (interlace.updateFrom(*param)) {
                    ALOGV("interlace change from %d to %d",mInterlace, interlace.value);
                    mInterlace = interlace.value;
                }
                break;
            }
            case C2StreamVendorUVOffset::CORE_INDEX: {
                C2StreamVendorUVOffset::output uvOffset(0u, 0);
                if (uvOffset.updateFrom(*param)) {
                    mSrcUVOffset = uvOffset.value;
                }
                break;
            }
            default:
                break;
        }
    }

    if(OK != verifyFormat(true, mSourceFormat)){
        ALOGW("%s: G2dFilter don't support mSourceFormat=0x%x", __func__, mSourceFormat);
        return BAD_VALUE;
    }

    uint32_t stride = mWidth;
    if(mSourceFormat == HAL_PIXEL_FORMAT_P010_TILED)
        stride = mWidth + ((mWidth + 3) >> 2);

    mSrcBufferSize = stride * mHeight * pxlfmt2bpp(mSourceFormat) / 8;

    ALOGI("handleInputConfigUpdate w=%d,h=%d, crop=[%d,%d], mSourceFormat=0x%x, mInterlace=%d, stride=%d",
        mWidth, mHeight, mCropWidth, mCropHeight, mSourceFormat, mInterlace, stride);

    ret = setTargetFormat();
    if (ret)
        return ret;

    ret = setParameters();
    if (ret)
        return ret;

    return OK;
}
status_t G2dFilter::verifyFormat(bool source, uint32_t format)
{
    if(source){
        for(auto fmt : source_format_table){
            if(fmt == format){
                return OK;
            }
        }
    }else{
        for(auto fmt : destination_format_table){
            if(fmt == format){
                return OK;
            }
        }
    }

    return NAME_NOT_FOUND;
}
status_t G2dFilter::setTargetFormat()
{
    status_t ret = OK;

    mTargetFormat = mIntfImpl->getVenderHalFormat();

    if(mSourceFormat == HAL_PIXEL_FORMAT_P010_TILED){
        mTargetFormat = k10bitTargetFormat;
    }

    if(mTargetFormat == mSourceFormat)
        mPassthrough = true;

    ret = verifyFormat(false, mTargetFormat);
    if(ret)
        return ret;

    c2_status_t err = C2_OK;
    C2StreamVendorHalPixelFormat::output vendor_fmt(0u, mTargetFormat);
    C2StreamPixelFormatInfo::output fmt(0u, mTargetFormat);

    std::vector<std::unique_ptr<C2SettingResult>> failures;
    err = mIntfImpl->config({&fmt, &vendor_fmt}, C2_MAY_BLOCK, &failures);
    if(err){
        ALOGE("setTargetFormat failed err = %d", err);
        return INVALID_OPERATION;
    }

    mTarBufferSize = mWidth * mHeight * pxlfmt2bpp(mTargetFormat) / 8;

    ALOGI("mTargetFormat=0x%x usage=0x%llx mPassthrough=%d", mTargetFormat, (long long)mIntfImpl->getOutputStreamUsage(),mPassthrough);

    return OK;
}
status_t G2dFilter::setParameters() {

    status_t ret = OK;
    G2D_PARAM src_param;
    G2D_PARAM tar_param;

    if(mPassthrough)
        return ret;

    src_param.format = mSourceFormat;
    src_param.width = mWidth;
    src_param.height = mHeight;
    if(mSourceFormat == HAL_PIXEL_FORMAT_P010_TILED)
        src_param.stride = mWidth + ((mWidth + 3) >> 2);
    else
        src_param.stride = mWidth;
    src_param.stride = (src_param.stride + mSrcStrideAlign - 1) & ~(mSrcStrideAlign - 1);
    src_param.cropWidth = mCropWidth;
    src_param.cropHeight = mCropHeight;
    src_param.uvOffset = mSrcUVOffset;
    src_param.interlace = mInterlace > 0 ? true:false;

    ret = pDev->setSrcParam(&src_param);
    if(ret)
        return ret;

    tar_param.format = mTargetFormat;
    tar_param.width = mWidth;
    tar_param.height = mHeight;
    tar_param.stride = mWidth;
    tar_param.cropWidth = mCropWidth;
    tar_param.cropHeight = mCropHeight;
    if (mTargetFormat == HAL_PIXEL_FORMAT_YCbCr_420_SP)
        tar_param.uvOffset = tar_param.stride * tar_param.height;
    else
        tar_param.uvOffset = 0;
    tar_param.interlace = false;

    ret = pDev->setDstParam(&tar_param);
    if(ret)
        return ret;

    mDevReady = true;
    return OK;
}
status_t G2dFilter::prepareOutputConfig(std::vector<std::unique_ptr<C2Param>> &in_config, std::vector<std::unique_ptr<C2Param>> &out_config)
{
    //clear filter internal parameters
    for (std::unique_ptr<C2Param> &param : in_config) {
        switch((*param).coreIndex().coreIndex()){
            case C2StreamPixelFormatInfo::CORE_INDEX:{
                C2StreamPixelFormatInfo::output fmt(0u, mTargetFormat);
                out_config.push_back(C2Param::Copy(fmt));
                break;
            }
            case C2StreamVendorInterlacedFormat::CORE_INDEX:{
                param.reset(nullptr);
                break;
            }
            case C2StreamVendorUVOffset::CORE_INDEX:{
                param.reset(nullptr);
                break;
            }
            default:
                out_config.push_back(C2Param::Copy(*param));
                break;
        }
    }

    ALOGV("configUpdate in.size=%zu outputConfig size=%zu",in_config.size(),out_config.size());

    return OK;
}
status_t G2dFilter::createFrameFetcher(){
    status_t ret = OK;

    if(mFetcher != nullptr){
        if(mFetcher->Running()){
            ret = mFetcher->Stop();
        }
    }else{
        mFetcher = new FrameFetcher();
        ret = mFetcher->Init(mOutputBlockPool);

        if(mFetcher == nullptr){
            ret = UNKNOWN_ERROR;
            return ret;
        }
    }

    ret = mFetcher->SetParam(mWidth, mHeight, mTargetFormat, mIntfImpl->getOutputStreamUsage());
    if(ret)
        return ret;

    ret = mFetcher->Start();

    return ret;
}

void G2dFilter::processWork(const std::unique_ptr<C2Work> &work){
    ATRACE_CALL();
    std::shared_ptr<C2Buffer> buffer;
    bool hasConfig = false;
    status_t ret = OK;

    if (!work->input.buffers.empty()) {
        buffer = work->input.buffers.front();
    }

    if(!work->input.configUpdate.empty())
        hasConfig = true;

    ALOGV("processWork ts=%d,flag=%x",(int)work->input.ordinal.timestamp.peeku(), work->input.flags);

    do{
        if(hasConfig){
            if (OK != handleConfigUpdate(work->input.configUpdate)) {
                // don't report error here cause g2dfilter can work in passthrough mode if not support src/dst format
                mPassthrough = true;
            }
        }

        if(buffer == nullptr){
            ALOGV("no buffer work->input.flags=%x",work->input.flags);
            work->worklets.front()->output.buffers.clear();
            break;
        }

        if(mPassthrough){
            work->worklets.front()->output.buffers.push_back(std::move(buffer));
            break;
        }

        if(!mDevReady){
            ret = INVALID_OPERATION;
            break;
        }

        if(mResChange){
            ret = createFrameFetcher();
            if(ret)
                break;

            mResChange = false;
        }

        if(mFetcher == nullptr){
            ret = UNKNOWN_ERROR;
            break;
        }

        if(!mFetcher->Running())
            mFetcher->Start();

        std::shared_ptr<C2GraphicBlock> dstBlock;

        while(true){
            ret = mFetcher->HasFrames();
            if(ret == OK)
                break;
            else if(ret == TIMED_OUT)
                usleep(3000);
            else
                break;
        }
        if(ret)
            break;

        ret = mFetcher->GetFrameBlock(&dstBlock);
        if(ret){
            ALOGE("GetFrameBlock failed");
            break;
        }

        const C2ConstGraphicBlock block = buffer->data().graphicBlocks().front();
        int src_fd = block.handle()->data[0];

        fsl::Memory *src_mem = (fsl::Memory*)block.handle();
        uint64_t src_addr = src_mem->phys;

        int tar_fd = dstBlock->handle()->data[0];
        fsl::Memory *tar_mem = (fsl::Memory*)dstBlock->handle();
        uint64_t tar_addr = tar_mem->phys;

        //add walk around for 8qm board, if not, cts android.media.decoder.cts.DecoderTest#testEOSBehaviorH264 always fail.
        if(mImx8qm){
            ret = IMXGetBufferAddr(tar_fd, mTarBufferSize, tar_addr, false);

            if(ret)
                break;
        }

        ret = pDev->blit(src_addr, tar_addr);
        if(ret)
            break;

        dumpInputBuffer(src_fd, mSrcBufferSize);
        dumpOutBuffer(tar_fd, mTarBufferSize);

        C2ConstGraphicBlock constBlock = dstBlock->share(C2Rect(mCropWidth, mCropHeight), C2Fence());
        std::shared_ptr<C2Buffer> outC2Buffer = C2Buffer::CreateGraphicBuffer(std::move(constBlock));

        work->worklets.front()->output.buffers.push_back(outC2Buffer);

        for (const std::shared_ptr<const C2Info> &info : buffer->info()) {
            std::shared_ptr<C2Param> param = std::move(C2Param::Copy(*info));
            outC2Buffer->setInfo(std::static_pointer_cast<C2Info>(param));
        }

    }while(0);

    if(hasConfig){
        ret = prepareOutputConfig(work->input.configUpdate, work->worklets.front()->output.configUpdate);
    }

    if(ret != OK){
        work->workletsProcessed = 1u;
        work->result = C2_CORRUPTED;
        ALOGE("processWork failed ret=%d",ret);
        return;
    }

    //copy ts & flag
    work->worklets.front()->output.flags = work->input.flags;
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->worklets.front()->output.infoBuffers = work->input.infoBuffers;
    ALOGV("processWork ts=%d,flag=%x success",(int)work->input.ordinal.timestamp.peeku(), work->input.flags);

    if(work->input.ordinal.customOrdinal.peekull() == (unsigned long long)-1)
        work->input.ordinal.timestamp = -1;

    if((work->input.flags & C2FrameData::FLAG_END_OF_STREAM) && mFetcher)
        mFetcher->Stop();

    work->workletsProcessed = 1;
    work->result = C2_OK;
    mOutCnt++;
    return;
}
void G2dFilter::onRelease() {

    if(mFetcher){
        mFetcher.clear();
    }

    if(pDev.get()){
        pDev.reset(nullptr);
    }

    ALOGV("%s END", __FUNCTION__);
    return;
}
void G2dFilter::dumpInputBuffer(int fd, uint32_t size)
{

    if(!(mDebugFlag & G2D_FILTER_DUMP_INPUT))
        return;

    //only dump one frame
    if(mOutCnt != 30)
        return;

    dumpBuffer(fd, size, G2D_FILTER_INPUT_FILE);

    return;
}

void G2dFilter::dumpOutBuffer(int fd, uint32_t size)
{

    if(!(mDebugFlag & G2D_FILTER_DUMP_OUTPUT))
        return;

    //only dump one frame
    if(mOutCnt != 30)
        return;

    dumpBuffer(fd, size, G2D_FILTER_OUTPUT_FILE);

    return;
}

}// namespace android
