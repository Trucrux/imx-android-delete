/**
*  Copyright 2022 NXP
*  All Rights Reserved.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "G2dDevTest"
#include <log/log.h>
#include <gtest/gtest.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/Errors.h>
#include <C2PlatformSupport.h>
#include <C2Buffer.h>
#include <C2BufferPriv.h>


#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/Mutexed.h>

#include "G2dDevice.h"
#include "graphics_ext.h"
#include "IMXUtils.h"




using namespace android;

struct CompToFiles {
    std::string inputFile;
    std::string outputFile;
    uint32_t srcformat;
    uint32_t tarformat;
    uint32_t width;
    uint32_t height;
    uint32_t crop_w;
    uint32_t crop_h;
    uint32_t interlace;
};
std::vector<CompToFiles> gCompToFiles = {
    //bbb_full.ffmpeg.1920x1080.mp4.libx265_6500kbps_30fps.libfaac_stereo_128kbps_48000Hz.mp4
    {"2048x1536_1080p_nv12_tile.yuv", "1.yuv", HAL_PIXEL_FORMAT_NV12_TILED, HAL_PIXEL_FORMAT_YCbCr_420_SP, 2048, 1536, 1920, 1080, false},
    {"2048x1536_1080p_nv12_tile.yuv", "2.yuv", HAL_PIXEL_FORMAT_NV12_TILED, HAL_PIXEL_FORMAT_YCBCR_422_I, 2048, 1536, 1920, 1080, false},
    //H264_HP41_1920x1088_30fps_55.8Mbps_shields_ter_Noaudio.mp4
    {"2048x1536_1080p_nv12_tile_interlace.yuv", "3.yuv", HAL_PIXEL_FORMAT_NV12_TILED, HAL_PIXEL_FORMAT_YCbCr_420_SP, 2048, 1536, 1920, 1080, true},
    {"2048x1536_1080p_nv12_tile_interlace.yuv", "4.yuv", HAL_PIXEL_FORMAT_NV12_TILED, HAL_PIXEL_FORMAT_YCBCR_422_I, 2048, 1536, 1920, 1080, true},
    //jellyfish-90-mbps-hd-hevc-10bit.mkv
    {"2048x1536_1080p_nv12_10bit_tile.yuv", "5.yuv", HAL_PIXEL_FORMAT_P010_TILED, HAL_PIXEL_FORMAT_YCbCr_420_SP, 2048, 1536, 1920, 1080, false},
};

std::vector<uint32_t> g_8qm_times = {
    1900,//us
    2830,
    5800,
    2913,
    3170,
};

std::vector<uint32_t> g_8qxp_times = {
    4059,//us
    5777,
    13301,
    6000,
    12296,
};

class G2dDevTest : public ::testing::Test {
public:
    std::string input = {"/sdcard/"};
    std::string output = {"/sdcard/g2d_output_"};

    const char * mInputFile;
    const char * mOutputFile;
    //const char * mTargetFile = "target.yuv";

    uint32_t mSourceFormat;
    uint32_t mTargetFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mSrcBufferSize;
    uint32_t mTarBufferSize;
    uint32_t mInterlace;

    uint32_t mCropWidth;
    uint32_t mCropHeight;
    uint32_t mOutCnt;

    C2BlockPool::local_id_t mBlockPoolId;
    std::shared_ptr<C2BlockPool> mGraphicPool;
    std::shared_ptr<C2Allocator> mGraphicAllocator;
    bool mDisableTest;
    std::vector<uint32_t> mCheckTS;

    void getFile(int32_t index);
    void setparams();

    std::unique_ptr<android::G2DDevice> mDev;

    const uint32_t mNum = 30;
    const uint32_t tolarance = 500;


    void setup() {
        mDisableTest = false;
        mDev = std::make_unique<G2DDevice>(true);

        std::shared_ptr<C2AllocatorStore> store = android::GetCodec2PlatformAllocatorStore();
        CHECK_EQ(store->fetchAllocator(C2AllocatorStore::DEFAULT_GRAPHIC, &mGraphicAllocator),
                 C2_OK);
        mGraphicPool = std::make_shared<C2PooledBlockPool>(mGraphicAllocator, mBlockPoolId++);
        ASSERT_NE(mGraphicPool, nullptr);

        mOutCnt = 0;

        char socId[20];
        if (0 == GetSocId(socId, sizeof(socId))) {
            if (!strncmp(socId, "i.MX8QM", 7)){
                mCheckTS = g_8qm_times;
                ALOGV("8QM");
            }else if(!strncmp(socId, "i.MX8QXP", 8)){
                mCheckTS = g_8qxp_times;
                ALOGV("8QXP");
            }else{
                mDisableTest = true;
            }
        }

        if(mDisableTest)
            ALOGE("[   WARN   ] Test Disabled \n");
    }

    status_t getParams(uint32_t index){

        for (size_t i = 0; i < gCompToFiles.size(); ++i) {
            if(i == index){
                input = input.append(gCompToFiles[i].inputFile);
                output = output.append(gCompToFiles[i].outputFile);
                mSourceFormat = gCompToFiles[i].srcformat;
                mTargetFormat = gCompToFiles[i].tarformat;
                mWidth = gCompToFiles[i].width;
                mHeight = gCompToFiles[i].height;
                mCropWidth = gCompToFiles[i].crop_w;
                mCropHeight = gCompToFiles[i].crop_h;
                mInterlace = gCompToFiles[i].interlace;
                mSrcBufferSize = mWidth * mHeight * pxlfmt2bpp(mSourceFormat) / 8;
                mTarBufferSize = mWidth * mHeight * pxlfmt2bpp(mTargetFormat) / 8;
                mInputFile = input.c_str();
                mOutputFile = output.c_str();
                ALOGD("mInputFile =%s", mInputFile);
                ALOGD("mOutputFile =%s", mOutputFile);
                return OK;
            }
        }
        return UNKNOWN_ERROR;
    }
    status_t setParams(){
        status_t ret = OK;
        G2D_PARAM src_param;
        G2D_PARAM tar_param;

        src_param.format = mSourceFormat;
        src_param.width = mWidth;
        src_param.height = mHeight;
        if(mSourceFormat == HAL_PIXEL_FORMAT_P010_TILED)
            src_param.stride = mWidth + ((mWidth + 3) >> 2);
        else
            src_param.stride = mWidth;
        src_param.cropWidth = mCropWidth;
        src_param.cropHeight = mCropHeight;
        src_param.interlace = mInterlace > 0 ? true:false;

        ret = mDev->setSrcParam(&src_param);
        if(ret)
            return ret;

        tar_param.format = mTargetFormat;
        tar_param.width = mWidth;
        tar_param.height = mHeight;
        tar_param.stride = mWidth;
        tar_param.cropWidth = mCropWidth;
        tar_param.cropHeight = mCropHeight;
        tar_param.interlace = false;

        ret = mDev->setDstParam(&tar_param);

        return ret;
    }

    status_t run(uint32_t num){
        status_t ret = OK;
        c2_status_t err = C2_OK;

        FILE * pfile = NULL;
        pfile = fopen(mInputFile,"rb");
        if(!pfile){
            return UNKNOWN_ERROR;
         }

        FILE * pfile2 = nullptr;
        pfile2 = fopen(mOutputFile,"wb");
        if(!pfile2){
            ALOGE("create %s failed",mOutputFile);
            return UNKNOWN_ERROR;
        }

        C2MemoryUsage src_usage(C2AndroidMemoryUsage::HW_TEXTURE_READ | C2MemoryUsage::CPU_WRITE);
        C2MemoryUsage tar_usage(C2AndroidMemoryUsage::HW_TEXTURE_READ);

        do{

            std::shared_ptr<C2GraphicBlock> src_block;
            err = mGraphicPool->fetchGraphicBlock(mWidth, mHeight, mSourceFormat,
                                                 src_usage,
                                                 &src_block);

            if (err != C2_OK) {
                ALOGE("fetchGraphicBlock src_block failed");
                break;
            }

            int src_fd = src_block->handle()->data[0];

            uint64_t src_addr;
            ret = IMXGetBufferAddr(src_fd, mSrcBufferSize, src_addr, false);
            if(ret)
                break;


            std::shared_ptr<C2GraphicBlock> tar_block;
            err = mGraphicPool->fetchGraphicBlock(mWidth, mHeight, mTargetFormat,
                                                 tar_usage,
                                                 &tar_block);
            if (err != C2_OK) {
                ALOGE("fetchGraphicBlock tar_block failed");
                break;
            }

            C2GraphicView src_view = src_block->map().get();
            uint8_t *src_ptr = src_view.data()[C2PlanarLayout::PLANE_Y];
            if(!src_ptr)
                ALOGE("unknown src_ptr");
            ret = fread (src_ptr,1,mSrcBufferSize,pfile);
            ALOGV("read %d bytes",ret);

            if(ret != mSrcBufferSize){
                if(mOutCnt > num){
                    ret = OK;
                    break;
                }else{
                    fseek(pfile, 0, SEEK_SET);
                    continue;
                }
            }

           int tar_fd = tar_block->handle()->data[0];

           uint64_t tar_addr;
           ret = IMXGetBufferAddr(tar_fd, mTarBufferSize, tar_addr, false);
           if(ret)
               break;

           ret = mDev->blit(src_addr, tar_addr);
           if(ret)
               break;

            mOutCnt ++;

            if(mOutCnt == 10){
                C2GraphicView dst_view = tar_block->map().get();
                uint8_t *dst_ptr = dst_view.data()[C2PlanarLayout::PLANE_Y];
                fwrite(dst_ptr,1, mTarBufferSize, pfile2);
                ALOGV("write %d to %s", mTarBufferSize, mOutputFile);
            }
            ALOGV("run mOutCnt=%d",mOutCnt);
        }while(mOutCnt <= num);

        if(pfile)
            fclose(pfile);
        if(pfile2)
            fclose(pfile2);
        ALOGV("run ret=%d",ret);

        return ret;
    }

    status_t testPerformance(uint32_t index){
        status_t ret = OK;
        uint32_t ts = 0;
        bool pass = true;
        setup();
        getParams(index);

        ret = setParams();
        if(ret)
            return ret;

        ret = run(mNum);
        if(ret)
            return ret;


        ts = mDev->getRunningTimeAndFlush();
        ALOGD("testPerformance index=%d, average ts=%d us", index, ts);

        if(ts > mCheckTS[index] + tolarance){
            pass = false;
            ALOGE("testPerformance %d fail expected ts=%d, actual=%d", index, mCheckTS[index], ts);
        }

        if(pass)
            return OK;
        else
            return UNKNOWN_ERROR;
    }

protected:


};


TEST_F(G2dDevTest, NV12Test){
    if (mDisableTest) GTEST_SKIP() << "Test is disabled";
    ALOGD("run NV12Test\n");
    ASSERT_EQ(testPerformance(0), OK);
}
TEST_F(G2dDevTest, YUYVTest){
    if (mDisableTest) GTEST_SKIP() << "Test is disabled";
    ALOGD("run YUYVTest");
    ASSERT_EQ(testPerformance(1), OK);
}
TEST_F(G2dDevTest, InterlaceNV12Test){
    if (mDisableTest) GTEST_SKIP() << "Test is disabled";
    ALOGD("run InterlaceNV12Test");
    ASSERT_EQ(testPerformance(2), OK);
}
TEST_F(G2dDevTest, InterlaceYUYVTest){
    if (mDisableTest) GTEST_SKIP() << "Test is disabled";
    ALOGD("run InterlaceYUYVTest");
    ASSERT_EQ(testPerformance(3), OK);
}

TEST_F(G2dDevTest, 10bitTest){
    if (mDisableTest) GTEST_SKIP() << "Test is disabled";
    ALOGD("run 10bitTest");
    ASSERT_EQ(testPerformance(4), OK);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int status = RUN_ALL_TESTS();
    ALOGD("Test result = %d\n", status);
    return status;
}
