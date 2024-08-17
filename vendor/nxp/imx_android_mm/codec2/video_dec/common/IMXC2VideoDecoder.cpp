/**
 *  Copyright 2019-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "IMXC2VideoDecoder"
#define ATRACE_TAG  ATRACE_TAG_VIDEO
#include <utils/Log.h>
#include <utils/Trace.h>

#include <atomic>
#include <media/stagefright/MediaDefs.h>
#include <string.h>
#include <sys/mman.h>
#include <Codec2Mapper.h>

#include "IMXC2VideoDecoder.h"
#include "C2_imx.h"
#include "C2Config_imx.h"
#include "IMXUtils.h"
#include "graphics_ext.h"
#include "Memory.h"
#include <cutils/properties.h>
#include "Allocator.h"


namespace android {

#define CHECK_AND_RETURN_C2_ERR(err) if((err) != OK) {ALOGE("%s, line %d", __FUNCTION__, __LINE__); return (((err) == OK) ? C2_OK : C2_CORRUPTED);}
#define C2ERR(err) ((err) == OK ? C2_OK : C2_CORRUPTED)

#define DEFAULT_ACTUAL_OUTPUT_DELAY_VALUE 16
#define DEFAULT_OUTPUT_BUFFER_CNT_IN_POST_PROCESS 6

#ifdef IMX_VIDEO_DEC_API_TRACE
#define IMX_VIDEO_DEC_API_TRACE ALOGD
#else
#define IMX_VIDEO_DEC_API_TRACE
#endif

constexpr size_t kMinInputBufferSize = 2 * 1024 * 1024;
#if(MAX_RESOLUTION == 4096)
constexpr uint32_t kMaxWidth = 4096;
constexpr uint32_t kMaxHeight = 2560;
#else
constexpr uint32_t kMaxWidth = 1920;
constexpr uint32_t kMaxHeight = 1088;
#endif
constexpr uint32_t kDimensionTolerance = 50;
constexpr size_t kSmoothnessFactor = 4;

static std::atomic<std::int32_t> gDecoderInstance = 0;

class IMXC2VideoDecoder::IntfImpl : public IMXInterface<void>::BaseParams {
public:
    explicit IntfImpl(const std::shared_ptr<C2ReflectorHelper> &helper, C2String componentName)
        : IMXInterface<void>::BaseParams(
                helper,
                componentName,
                C2Component::KIND_DECODER,
                C2Component::DOMAIN_VIDEO,
                Name2MimeType(componentName.c_str())),
                mComponentName(componentName) {

        C2String mimeType(Name2MimeType(mComponentName.c_str()));
        char socId[20];
        bool enable_10bit = false;
        if (0 == GetSocId(socId, sizeof(socId))) {
            if (!strncmp(socId, "i.MX8MQ", 7)) {
                enable_10bit = true;
            }
        }

        noPrivateBuffers(); // TODO: account for our buffers here
        noInputReferences();
        noOutputReferences();
        noTimeStretch();

        addParameter(
                DefineParam(mActualInputDelay, C2_PARAMKEY_INPUT_DELAY)
                .withDefault(new C2PortActualDelayTuning::input(0u))
                .withFields({C2F(mActualInputDelay, value).inRange(0, 32)})
                .withSetter(Setter<decltype(*mActualInputDelay)>::StrictValueWithNoDeps)
                .build());

        addParameter(
                DefineParam(mActualOutputDelay, C2_PARAMKEY_OUTPUT_DELAY)
                .withDefault(new C2PortActualDelayTuning::output(DEFAULT_ACTUAL_OUTPUT_DELAY_VALUE))
                .withFields({C2F(mActualOutputDelay, value).inRange(0, 32)})
                .withSetter(Setter<decltype(*mActualOutputDelay)>::StrictValueWithNoDeps)
                .build());

        // TODO: output latency and reordering

        addParameter(
                DefineParam(mAttrib, C2_PARAMKEY_COMPONENT_ATTRIBUTES)
                .withConstValue(new C2ComponentAttributesSetting(C2Component::ATTRIB_IS_TEMPORAL))
                .build());

        // coded and output picture size is the same for this codec
        addParameter(
                DefineParam(mSize, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(0u, 320, 240))
                .withFields({
                    C2F(mSize, width).inRange(2, kMaxWidth, 2),
                    C2F(mSize, height).inRange(2, kMaxHeight, 2),
                })
                .withSetter(SizeSetter)
                .build());

        // coded and output picture size is the same for this codec
        addParameter(
                DefineParam(mCrop, C2_PARAMKEY_CROP_RECT)
                .withDefault(new C2StreamCropRectInfo::output(0u, C2Rect(320, 240)))
                .withFields({
                    C2F(mCrop, width).inRange(2, kMaxWidth, 2),
                    C2F(mCrop, height).inRange(2, kMaxHeight, 2),
                })
                .withSetter(CropSetter)
                .build());

        addParameter(
                DefineParam(mMaxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(0u, 320, 240))
                .withFields({
                    C2F(mMaxSize, width).inRange(2, kMaxWidth, 2),
                    C2F(mMaxSize, height).inRange(2, kMaxHeight, 2),
                })
                .withSetter(MaxPictureSizeSetter, mSize)
                .build());

        if (mimeType == MEDIA_MIMETYPE_VIDEO_AVC) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_AVC_CONSTRAINED_BASELINE, C2Config::LEVEL_AVC_5_2))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_AVC_CONSTRAINED_BASELINE,
                                C2Config::PROFILE_AVC_BASELINE,
                                C2Config::PROFILE_AVC_MAIN,
                                C2Config::PROFILE_AVC_EXTENDED,
                                C2Config::PROFILE_AVC_CONSTRAINED_HIGH,
                                C2Config::PROFILE_AVC_PROGRESSIVE_HIGH,
                                C2Config::PROFILE_AVC_HIGH,
                                C2Config::PROFILE_AVC_HIGH_422,
                                C2Config::PROFILE_AVC_HIGH_444_PREDICTIVE,
                                C2Config::PROFILE_AVC_HIGH_422_INTRA,
                                C2Config::PROFILE_AVC_HIGH_444_INTRA,
                                C2Config::PROFILE_AVC_CAVLC_444_INTRA,
                                C2Config::PROFILE_AVC_SCALABLE_BASELINE,
                                C2Config::PROFILE_AVC_SCALABLE_HIGH,
                                C2Config::PROFILE_AVC_SCALABLE_HIGH_INTRA,
                                C2Config::PROFILE_AVC_MULTIVIEW_HIGH,
                                C2Config::PROFILE_AVC_STEREO_HIGH,
                                C2Config::PROFILE_AVC_MULTIVIEW_DEPTH_HIGH}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_AVC_1, C2Config::LEVEL_AVC_1B, C2Config::LEVEL_AVC_1_1,
                                C2Config::LEVEL_AVC_1_2, C2Config::LEVEL_AVC_1_3,
                                C2Config::LEVEL_AVC_2, C2Config::LEVEL_AVC_2_1, C2Config::LEVEL_AVC_2_2,
                                C2Config::LEVEL_AVC_3, C2Config::LEVEL_AVC_3_1, C2Config::LEVEL_AVC_3_2,
                                C2Config::LEVEL_AVC_4, C2Config::LEVEL_AVC_4_1, C2Config::LEVEL_AVC_4_2,
                                C2Config::LEVEL_AVC_5, C2Config::LEVEL_AVC_5_1, C2Config::LEVEL_AVC_5_2,
                                C2Config::LEVEL_AVC_6, C2Config::LEVEL_AVC_6_1, C2Config::LEVEL_AVC_6_2
                        })
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        }else if (mimeType == MEDIA_MIMETYPE_VIDEO_HEVC) {
            std::vector<uint32_t> hevcProfiles = {
                C2Config::PROFILE_HEVC_MAIN,
                C2Config::PROFILE_HEVC_MAIN_STILL,
            };
            if(enable_10bit)
                hevcProfiles.push_back(C2Config::PROFILE_HEVC_MAIN_10);

            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_HEVC_MAIN, C2Config::LEVEL_HEVC_MAIN_5_1))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf(hevcProfiles),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_HEVC_MAIN_1,
                                C2Config::LEVEL_HEVC_MAIN_2, C2Config::LEVEL_HEVC_MAIN_2_1,
                                C2Config::LEVEL_HEVC_MAIN_3, C2Config::LEVEL_HEVC_MAIN_3_1,
                                C2Config::LEVEL_HEVC_MAIN_4, C2Config::LEVEL_HEVC_MAIN_4_1,
                                C2Config::LEVEL_HEVC_MAIN_5, C2Config::LEVEL_HEVC_MAIN_5_1, C2Config::LEVEL_HEVC_MAIN_5_2,
                                C2Config::LEVEL_HEVC_MAIN_6, C2Config::LEVEL_HEVC_MAIN_6_1, C2Config::LEVEL_HEVC_MAIN_6_2
                        })
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mimeType ==  MEDIA_MIMETYPE_VIDEO_MPEG4) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_MP4V_SIMPLE, C2Config::LEVEL_MP4V_3))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_MP4V_SIMPLE,
                                C2Config::PROFILE_MP4V_SIMPLE_SCALABLE,
                                C2Config::PROFILE_MP4V_CORE,
                                C2Config::PROFILE_MP4V_ACE,
                                C2Config::PROFILE_MP4V_ADVANCED_SIMPLE}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_MP4V_0,
                                C2Config::LEVEL_MP4V_0B,
                                C2Config::LEVEL_MP4V_1,
                                C2Config::LEVEL_MP4V_2,
                                C2Config::LEVEL_MP4V_3,
                                C2Config::LEVEL_MP4V_3B,
                                C2Config::LEVEL_MP4V_4,
                                C2Config::LEVEL_MP4V_5})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mimeType ==  MEDIA_MIMETYPE_VIDEO_MPEG2) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_MP2V_SIMPLE, C2Config::LEVEL_MP2V_HIGH))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_MP2V_SIMPLE,
                                C2Config::PROFILE_MP2V_MAIN,
                                C2Config::PROFILE_MP2V_SNR_SCALABLE,
                                C2Config::PROFILE_MP2V_SPATIALLY_SCALABLE,
                                C2Config::PROFILE_MP2V_HIGH,
                                C2Config::PROFILE_MP2V_MULTIVIEW}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_MP2V_LOW,
                                C2Config::LEVEL_MP2V_MAIN,
                                C2Config::LEVEL_MP2V_HIGH_1440,
                                C2Config::LEVEL_MP2V_HIGH})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());

        } else if (mimeType ==  MEDIA_MIMETYPE_VIDEO_H263) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_H263_BASELINE, C2Config::LEVEL_H263_30))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf({
                                C2Config::PROFILE_H263_BASELINE,
                                C2Config::PROFILE_H263_ISWV2}),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_H263_10,
                                C2Config::LEVEL_H263_20,
                                C2Config::LEVEL_H263_30,
                                C2Config::LEVEL_H263_40,
                                C2Config::LEVEL_H263_45,
                                C2Config::LEVEL_H263_50,
                                C2Config::LEVEL_H263_60,
                                C2Config::LEVEL_H263_70})
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());
        } else if (mimeType ==  MEDIA_MIMETYPE_VIDEO_VP8) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withConstValue(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_UNUSED, C2Config::LEVEL_UNUSED))
                    .build());
        } else if (mimeType ==  MEDIA_MIMETYPE_VIDEO_VP9) {
            // TODO: Add C2Config::PROFILE_VP9_2HDR ??
            std::vector<uint32_t> vp9Profiles = {
                C2Config::PROFILE_VP9_0,
                C2Config::PROFILE_VP9_1,
            };
            if (enable_10bit) {
                vp9Profiles.push_back(C2Config::PROFILE_VP9_2);
                vp9Profiles.push_back(C2Config::PROFILE_VP9_3);
            }
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                    .withDefault(new C2StreamProfileLevelInfo::input(0u,
                            C2Config::PROFILE_VP9_0, C2Config::LEVEL_VP9_5))
                    .withFields({
                        C2F(mProfileLevel, profile).oneOf(vp9Profiles),
                        C2F(mProfileLevel, level).oneOf({
                                C2Config::LEVEL_VP9_1,
                                C2Config::LEVEL_VP9_1_1,
                                C2Config::LEVEL_VP9_2,
                                C2Config::LEVEL_VP9_2_1,
                                C2Config::LEVEL_VP9_3,
                                C2Config::LEVEL_VP9_3_1,
                                C2Config::LEVEL_VP9_4,
                                C2Config::LEVEL_VP9_4_1,
                                C2Config::LEVEL_VP9_5,
                        })
                    })
                    .withSetter(ProfileLevelSetter, mSize)
                    .build());

            mHdr10PlusInfoInput = C2StreamHdr10PlusInfo::input::AllocShared(0);
            addParameter(
                    DefineParam(mHdr10PlusInfoInput, C2_PARAMKEY_INPUT_HDR10_PLUS_INFO)
                    .withDefault(mHdr10PlusInfoInput)
                    .withFields({
                        C2F(mHdr10PlusInfoInput, m.value).any(),
                    })
                    .withSetter(Hdr10PlusInfoInputSetter)
                    .build());

            mHdr10PlusInfoOutput = C2StreamHdr10PlusInfo::output::AllocShared(0);
            addParameter(
                    DefineParam(mHdr10PlusInfoOutput, C2_PARAMKEY_OUTPUT_HDR10_PLUS_INFO)
                    .withDefault(mHdr10PlusInfoOutput)
                    .withFields({
                        C2F(mHdr10PlusInfoOutput, m.value).any(),
                    })
                    .withSetter(Hdr10PlusInfoOutputSetter)
                    .build());
        }

        addParameter(
                DefineParam(mMaxInputSize, C2_PARAMKEY_INPUT_MAX_BUFFER_SIZE)
                .withDefault(new C2StreamMaxBufferSizeInfo::input(0u, kMinInputBufferSize))
                .withFields({
                    C2F(mMaxInputSize, value).any(),
                })
                .calculatedAs(MaxInputSizeSetter, mMaxSize)
                .build());

        if (mimeType == MEDIA_MIMETYPE_VIDEO_HEVC
                || mimeType == MEDIA_MIMETYPE_VIDEO_VP9) {
            helper->addStructDescriptors<C2MasteringDisplayColorVolumeStruct, C2ColorXyStruct>();
            mHdrStaticInfoOutput = std::make_shared<C2StreamHdrStaticInfo::output>();
            addParameter(
                    DefineParam(mHdrStaticInfoOutput, C2_PARAMKEY_HDR_STATIC_INFO)
                    .withDefault(mHdrStaticInfoOutput)
                    .withFields({
                        C2F(mHdrStaticInfoOutput, mastering.red.x).inRange(0,1),
                    })
                    .withSetter(HdrStaticInfoOutputSetter)
                    .build());
        }

        if (mimeType == MEDIA_MIMETYPE_VIDEO_AVC
                || mimeType == MEDIA_MIMETYPE_VIDEO_HEVC
                || mimeType == MEDIA_MIMETYPE_VIDEO_MPEG2) {

            C2ChromaOffsetStruct locations[1] = { C2ChromaOffsetStruct::ITU_YUV_420_0() };
            std::shared_ptr<C2StreamColorInfo::output> defaultColorInfo =
                C2StreamColorInfo::output::AllocShared(
                        1u, 0u, 8u /* bitDepth */, C2Color::YUV_420);
            memcpy(defaultColorInfo->m.locations, locations, sizeof(locations));

            defaultColorInfo =
                C2StreamColorInfo::output::AllocShared(
                        { C2ChromaOffsetStruct::ITU_YUV_420_0() },
                        0u, 8u /* bitDepth */, C2Color::YUV_420);
            helper->addStructDescriptors<C2ChromaOffsetStruct>();

            addParameter(
                    DefineParam(mColorInfo, C2_PARAMKEY_CODED_COLOR_INFO)
                    .withConstValue(defaultColorInfo)
                    .build());

            addParameter(
                    DefineParam(mDefaultColorAspects, C2_PARAMKEY_DEFAULT_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsTuning::output(
                            0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields({
                        C2F(mDefaultColorAspects, range).inRange(
                                    C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                        C2F(mDefaultColorAspects, primaries).inRange(
                                    C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                        C2F(mDefaultColorAspects, transfer).inRange(
                                    C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                        C2F(mDefaultColorAspects, matrix).inRange(
                                    C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                    })
                    .withSetter(DefaultColorAspectsSetter)
                    .build());

            addParameter(
                    DefineParam(mCodedColorAspects, C2_PARAMKEY_VUI_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsInfo::input(
                            0u, C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields({
                        C2F(mCodedColorAspects, range).inRange(
                                    C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                        C2F(mCodedColorAspects, primaries).inRange(
                                    C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                        C2F(mCodedColorAspects, transfer).inRange(
                                    C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                        C2F(mCodedColorAspects, matrix).inRange(
                                    C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                    })
                    .withSetter(CodedColorAspectsSetter)
                    .build());

            addParameter(
                    DefineParam(mColorAspects, C2_PARAMKEY_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsInfo::output(
                            0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields({
                        C2F(mColorAspects, range).inRange(
                                    C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                        C2F(mColorAspects, primaries).inRange(
                                    C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                        C2F(mColorAspects, transfer).inRange(
                                    C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                        C2F(mColorAspects, matrix).inRange(
                                    C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
                    })
                    .withSetter(ColorAspectsSetter, mDefaultColorAspects, mCodedColorAspects)
                    .build());
        }

        std::vector<uint32_t> pixelFormats = {
            HAL_PIXEL_FORMAT_YCBCR_420_888,
            HAL_PIXEL_FORMAT_YCbCr_420_SP,
            HAL_PIXEL_FORMAT_NV12_TILED,
            HAL_PIXEL_FORMAT_YCBCR_422_I,
            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
        };
        //add 10 bit format
        if (mimeType == MEDIA_MIMETYPE_VIDEO_HEVC
                || mimeType == MEDIA_MIMETYPE_VIDEO_VP9) {
            pixelFormats.push_back(HAL_PIXEL_FORMAT_P010_TILED);
            pixelFormats.push_back(HAL_PIXEL_FORMAT_P010);
        }

        addParameter(
                DefineParam(mPixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
                .withDefault(new C2StreamPixelFormatInfo::output(
                            0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
                .withFields({C2F(mPixelFormat, value).oneOf(pixelFormats)})
                .withSetter(Setter<decltype(*mPixelFormat)>::StrictValueWithNoDeps)
                .build());

        addParameter(
                DefineParam(mSurfaceAllocator, C2_PARAMKEY_OUTPUT_SURFACE_ALLOCATOR)
                .withConstValue(new C2PortSurfaceAllocatorTuning::output(C2PlatformAllocatorStore::BUFFERQUEUE))
                .build());

        addParameter(
                DefineParam(mVideoSubFormat, C2_PARAMKEY_VENDOR_SUB_FORMAT)
                .withDefault(new C2StreamVendorSubFormat::output(0u, 0))
                .withFields({C2F(mVideoSubFormat, value).inRange(0, 0x7fffffff)})
                .withSetter(Setter<decltype(*mVideoSubFormat)>::StrictValueWithNoDeps)
                .build());

        addParameter(
                DefineParam(mVendorHalPixelFormat, C2_PARAMKEY_VENDOR_HAL_PIXEL_FORMAT)
                .withDefault(new C2StreamVendorHalPixelFormat::output(0u, HAL_PIXEL_FORMAT_YCbCr_420_SP))
                .withFields({C2F(mVendorHalPixelFormat, value).inRange(0, 0xffffffff)})
                .withSetter(Setter<decltype(*mVendorHalPixelFormat)>::StrictValueWithNoDeps)
                .build());

        addParameter(
                DefineParam(mLowLatency, C2_PARAMKEY_LOW_LATENCY_MODE)
                .withDefault(new C2GlobalLowLatencyModeTuning(0))
                .withFields({C2F(mLowLatency, value).inRange(0, 1)})
                .withSetter(Setter<decltype(*mLowLatency)>::StrictValueWithNoDeps)
                .build());
#if (ANDROID_VERSION >= ANDROID12)
        addParameter(
                DefineParam(mSecureBufferMode, C2_PARAMKEY_SECURE_MODE)
                .withDefault(new C2SecureModeTuning(C2Config::SM_UNPROTECTED))
                .withFields({ C2F(mSecureBufferMode, value).oneOf({
                                C2Config::SM_UNPROTECTED,
                                C2Config::SM_READ_PROTECTED,
                                C2Config::SM_READ_PROTECTED_WITH_ENCRYPTED})
                            })
                .withSetter(Setter<decltype(*mSecureBufferMode)>::StrictValueWithNoDeps)
                .build());

#endif

    addParameter(
            DefineParam(mOutUsage, C2_PARAMKEY_OUTPUT_STREAM_USAGE)
            .withDefault(new C2StreamUsageTuning::output(0u, 0))
            .withFields({C2F(mOutUsage, value).any()})
            .withSetter(Setter<decltype(*mOutUsage)>::StrictValueWithNoDeps)
            .build());

    addParameter(
            DefineParam(mInterlace, C2_PARAMKEY_VENDOR_INTERLACED_FORMAT)
            .withDefault(new C2StreamVendorInterlacedFormat::output(0u, 0))
            .withFields({C2F(mInterlace, value).any()})
            .withSetter(Setter<decltype(*mInterlace)>::StrictValueWithNoDeps)
            .build());

    addParameter(
            DefineParam(mVendorUsage, C2_PARAMKEY_VENDOR_STREAM_USAGE)
            .withDefault(new C2StreamVendorUsageTuning::output(0u, 0))
            .withFields({C2F(mVendorUsage, value).any()})
            .withSetter(Setter<decltype(*mVendorUsage)>::StrictValueWithNoDeps)
            .build());

    addParameter(
            DefineParam(mVendorBlockPool, C2_PARAMKEY_VENDOR_BLOCK_POOLS)
            .withDefault(new C2StreamVendorBlockPoolsTuning::output(0u, 0))
            .withFields({C2F(mVendorBlockPool, value).any()})
            .withSetter(Setter<decltype(*mVendorBlockPool)>::StrictValueWithNoDeps)
            .build());

    addParameter(
            DefineParam(mVendorUVOffset, C2_PARAMKEY_VENDOR_UV_OFFSET)
            .withDefault(new C2StreamVendorUVOffset::output(0u, 0))
            .withFields({C2F(mVendorUVOffset, value).any()})
            .withSetter(Setter<decltype(*mVendorUVOffset)>::StrictValueWithNoDeps)
            .build());

        //TODO: USAGE_CONTINUOUS_BUFFER for dma input buffer
        addParameter(
                DefineParam(mInUsage, C2_PARAMKEY_INPUT_STREAM_USAGE)
                .withDefault(new C2StreamUsageTuning::input(0u, 0))
                .withFields({C2F(mInUsage, value).any()})
                .withSetter(Setter<decltype(*mInUsage)>::StrictValueWithNoDeps)
                .build());

    }

    static C2R SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::output> &oldMe,
                          C2P<C2StreamPictureSizeInfo::output> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
            ALOGI("invalid width = %d", me.v.width);
            if(me.v.width > kMaxWidth + kDimensionTolerance){
                res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
                me.set().width = oldMe.v.width;
            }else{
                me.set().width = ((me.v.width+1)*2/2);
            }
        }

        if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
            ALOGI("invalid height = %d", me.v.height);
            if(me.v.height > kMaxHeight + kDimensionTolerance){
                res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
                me.set().height = oldMe.v.height;
            }else{
                me.set().height = ((me.v.height+1)*2/2);
            }
        }

        return res;
    }

    static C2R CropSetter(bool mayBlock, const C2P<C2StreamCropRectInfo::output> &oldMe,
                          C2P<C2StreamCropRectInfo::output> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
            ALOGI("invalid crop width = %d", me.v.width);
            if(me.v.width > kMaxWidth + kDimensionTolerance){
                res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
                me.set().width = oldMe.v.width;
            }else{
                me.set().width = ((me.v.width+1)*2/2);
            }
        }

        if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
            ALOGI("invalid crop height = %d", me.v.height);
            if(me.v.height > kMaxHeight + kDimensionTolerance){
                res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
                me.set().height = oldMe.v.height;
            }else{
                me.set().height = ((me.v.height+1)*2/2);
            }
        }
        return res;
    }

    static C2R MaxPictureSizeSetter(bool mayBlock, C2P<C2StreamMaxPictureSizeTuning::output> &me,
                                    const C2P<C2StreamPictureSizeInfo::output> &size) {
        (void)mayBlock;
        // TODO: get max width/height from the size's field helpers vs. hardcoding
        me.set().width = c2_min(c2_max(me.v.width, size.v.width), 4080u);
        me.set().height = c2_min(c2_max(me.v.height, size.v.height), 2048u);
        return C2R::Ok();
    }

    static C2R MaxInputSizeSetter(bool mayBlock, C2P<C2StreamMaxBufferSizeInfo::input> &me,
                                  const C2P<C2StreamMaxPictureSizeTuning::output> &maxSize) {
        (void)mayBlock;
        // assume compression ratio of 2
        me.set().value = c2_max((((maxSize.v.width + 15) / 16)
                * ((maxSize.v.height + 15) / 16) * 192), kMinInputBufferSize);
        return C2R::Ok();
    }

    static C2R ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::input> &me,
                                  const C2P<C2StreamPictureSizeInfo::output> &size) {
        (void)mayBlock;
        (void)size;
        (void)me;  // TODO: validate
        return C2R::Ok();
    }

    static C2R DefaultColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsTuning::output> &me) {
        (void)mayBlock;
        if (me.v.range > C2Color::RANGE_OTHER) {
                me.set().range = C2Color::RANGE_OTHER;
        }
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
                me.set().primaries = C2Color::PRIMARIES_OTHER;
        }
        if (me.v.transfer > C2Color::TRANSFER_OTHER) {
                me.set().transfer = C2Color::TRANSFER_OTHER;
        }
        if (me.v.matrix > C2Color::MATRIX_OTHER) {
                me.set().matrix = C2Color::MATRIX_OTHER;
        }
        return C2R::Ok();
    }

    static C2R CodedColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::input> &me) {
        (void)mayBlock;
        if (me.v.range > C2Color::RANGE_OTHER) {
                me.set().range = C2Color::RANGE_OTHER;
        }
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
                me.set().primaries = C2Color::PRIMARIES_OTHER;
        }
        if (me.v.transfer > C2Color::TRANSFER_OTHER) {
                me.set().transfer = C2Color::TRANSFER_OTHER;
        }
        if (me.v.matrix > C2Color::MATRIX_OTHER) {
                me.set().matrix = C2Color::MATRIX_OTHER;
        }
        return C2R::Ok();
    }

    static C2R ColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::output> &me,
                                  const C2P<C2StreamColorAspectsTuning::output> &def,
                                  const C2P<C2StreamColorAspectsInfo::input> &coded) {
        (void)mayBlock;
        // take default values for all unspecified fields, and coded values for specified ones
        me.set().range = coded.v.range == RANGE_UNSPECIFIED ? def.v.range : coded.v.range;
        me.set().primaries = coded.v.primaries == PRIMARIES_UNSPECIFIED
                ? def.v.primaries : coded.v.primaries;
        me.set().transfer = coded.v.transfer == TRANSFER_UNSPECIFIED
                ? def.v.transfer : coded.v.transfer;
        me.set().matrix = coded.v.matrix == MATRIX_UNSPECIFIED ? def.v.matrix : coded.v.matrix;
        return C2R::Ok();
    }

    std::shared_ptr<C2StreamColorAspectsInfo::output> getColorAspects_l() {
        return mColorAspects;
    }

    std::shared_ptr<C2StreamColorAspectsTuning::output> getDefaultColorAspects_l() {
        return mDefaultColorAspects;
    }


    uint32_t getVenderHalFormat() const { return mVendorHalPixelFormat->value; }

    uint32_t getRawPixelFormat() const { return mPixelFormat->value; }

    static C2R Hdr10PlusInfoInputSetter(bool mayBlock, C2P<C2StreamHdr10PlusInfo::input> &me) {
        (void)mayBlock;
        (void)me;  // TODO: validate
        return C2R::Ok();
    }

    static C2R Hdr10PlusInfoOutputSetter(bool mayBlock, C2P<C2StreamHdr10PlusInfo::output> &me) {
        (void)mayBlock;
        (void)me;  // TODO: validate
        return C2R::Ok();
    }

    static C2R HdrStaticInfoOutputSetter(bool mayBlock, C2P<C2StreamHdrStaticInfo::output> &me) {
        (void)mayBlock;
        (void)me;  // TODO: validate
        return C2R::Ok();
    }

    C2Config::secure_mode_t getSecureBufferMode() const { return mSecureBufferMode->value; }
    
    uint32_t getInterlaceFormat() const { return mInterlace->value; }

    uint64_t getVenderUsage() const { return mVendorUsage->value; }

    uint32_t getVenderBlockPool() const { return mVendorBlockPool->value; }

private:
    C2String mComponentName;

    std::shared_ptr<C2StreamProfileLevelInfo::input> mProfileLevel;
    std::shared_ptr<C2StreamPictureSizeInfo::output> mSize;
    std::shared_ptr<C2StreamCropRectInfo::output> mCrop;
    std::shared_ptr<C2StreamMaxPictureSizeTuning::output> mMaxSize;
    std::shared_ptr<C2StreamMaxBufferSizeInfo::input> mMaxInputSize;
    std::shared_ptr<C2StreamColorInfo::output> mColorInfo;
    std::shared_ptr<C2StreamColorAspectsInfo::input> mCodedColorAspects;
    std::shared_ptr<C2StreamColorAspectsTuning::output> mDefaultColorAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::output> mColorAspects;
    std::shared_ptr<C2StreamPixelFormatInfo::output> mPixelFormat;
    std::shared_ptr<C2PortSurfaceAllocatorTuning::output> mSurfaceAllocator;
    std::shared_ptr<C2StreamHdr10PlusInfo::input> mHdr10PlusInfoInput;
    std::shared_ptr<C2StreamHdr10PlusInfo::output> mHdr10PlusInfoOutput;
    std::shared_ptr<C2StreamHdrStaticInfo::output> mHdrStaticInfoOutput;
    std::shared_ptr<C2StreamVendorSubFormat::output> mVideoSubFormat;
    std::shared_ptr<C2StreamVendorHalPixelFormat::output> mVendorHalPixelFormat;
    std::shared_ptr<C2GlobalLowLatencyModeTuning> mLowLatency;
    std::shared_ptr<C2SecureModeTuning> mSecureBufferMode;
    std::shared_ptr<C2StreamUsageTuning::output> mOutUsage;
    std::shared_ptr<C2StreamVendorInterlacedFormat::output> mInterlace;
    std::shared_ptr<C2StreamVendorUsageTuning::output> mVendorUsage;
    std::shared_ptr<C2StreamVendorBlockPoolsTuning::output> mVendorBlockPool;
    std::shared_ptr<C2StreamVendorUVOffset::output> mVendorUVOffset;
    std::shared_ptr<C2StreamUsageTuning::input> mInUsage;
};


IMXC2VideoDecoder::IMXC2VideoDecoder(const char* name, c2_node_id_t id, const std::shared_ptr<IntfImpl> &intfImpl)
    : IMXC2ComponentBase(std::make_shared<IMXInterface<IntfImpl>>(name, id, intfImpl)),
      mIntf(intfImpl),
      mWidth(320),
      mHeight(240),
      mCropWidth(320),
      mCropHeight(240),
      nOutBufferNum(8),
      mName(name),
      bPendingFmtChanged(false),
      bGetGraphicBlockPool(false),
      bSignalOutputEos(false),
      bSignalledError(false),
      bFlushDone(false),
      bSupportColorAspects(false),
      bSecure(false),
      bFirstAfterStart(true),
      bFirstAfterFlush(true),
      bDmaInput(false),
      bImx8m(false),
      bImx8mSecure(false){
    gDecoderInstance++;
}

IMXC2VideoDecoder::~IMXC2VideoDecoder() {
    if(gDecoderInstance > 0)
        gDecoderInstance--;
}

// From IMXC2ComponentBase
c2_status_t IMXC2VideoDecoder::onInit() {
    ATRACE_CALL();
    status_t err;

    int32_t max_count = 32;
    char socId[20];
    if (0 == GetSocId(socId, sizeof(socId))) {
        if(!strncmp(socId, "i.MX8QM", 7) || !strncmp(socId, "i.MX8QXP", 8)){
            max_count = 8;
        }else if(!strncmp(socId, "i.MX8M", 6)){
            bImx8m = true;
        }
    }

    if(gDecoderInstance > max_count)
        return C2_NO_MEMORY;

    const char* mime = Name2MimeType((const char*)mName.c_str());
    if (mime == nullptr) {
        ALOGE("Unsupported component name: %s", mName.c_str());
        return C2_BAD_VALUE;
    }
    ALOGV("onInit mime=%s",mime);

    if (!strcmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)
            || !strcmp(mime, MEDIA_MIMETYPE_VIDEO_HEVC)
            || !strcmp(mime, MEDIA_MIMETYPE_VIDEO_MPEG2))
        bSupportColorAspects = true;

    mDecoder = CreateVideoDecoderInstance(mime);
    if (!mDecoder) {
        ALOGE("CreateVideoDecoderInstance for mime(%s) failed \n", mime);
        return C2_CORRUPTED;
    }

    err = mDecoder->init((VideoDecoderBase::Client*)this);
    if (err) {
        goto RELEASE_DECODER;
    }

    err = initInternalParam();
    if (err)
        goto RELEASE_DECODER;

    return C2_OK;

RELEASE_DECODER:
    ALOGE("RELEASE_DECODER");
    // release decoder if init failed, in case of upper layer don't call release
    releaseDecoder();
    return C2_NO_MEMORY;
}
void IMXC2VideoDecoder::onStart(){
    ALOGV("onStart");
    //ccodec call start first, then call mChannel->start() to set usage config value.
    //so do nothing here and set decoder parameter in processWork();
    bFirstAfterStart = true;
}

c2_status_t IMXC2VideoDecoder::onStop() {
    ATRACE_CALL();
    status_t err;

    ALOGV("onStop");
    err = mDecoder->stop();
    if (err != OK)
        ALOGE("decoder stop return err %d", err);

    bSignalledError = false;
    bSignalOutputEos = false;
    bRecieveOutputEos = false;
    bFirstAfterFlush = true;

    return C2ERR(err);
}

c2_status_t IMXC2VideoDecoder::onFlush_sm() {
    ATRACE_CALL();
    status_t err = OK;
    ALOGV("onFlush_sm");


    err = mDecoder->flush();
    CHECK_AND_RETURN_C2_ERR(err);

    bPendingFmtChanged = false;
    bSignalledError = false;
    bSignalOutputEos = false;
    bRecieveOutputEos = false;
    bFlushDone = true;
    bFirstAfterFlush = true;

    nCurFrameIndex = 0;
    nUsedFrameIndex = 0;
    ALOGV("onFlush_sm end");
    return C2ERR(err);
}

void IMXC2VideoDecoder::onReset() {
    ALOGV("onReset");
    (void) releaseDecoder();
}

void IMXC2VideoDecoder::onRelease() {
    ALOGV("onRelease");
    (void) releaseDecoder();
}

static void fillEmptyWork(const std::unique_ptr<C2Work> &work) {
    uint32_t flags = 0;
    if (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) {
        flags |= C2FrameData::FLAG_END_OF_STREAM;
        ALOGV("signalling eos");
    }
    work->worklets.front()->output.flags = (C2FrameData::flags_t)flags;
    work->worklets.front()->output.buffers.clear();
    work->worklets.front()->output.ordinal = work->input.ordinal;
    work->workletsProcessed = 1u;
}
c2_status_t IMXC2VideoDecoder::canProcessWork(){
    if(!mDecoder)
        return C2_BAD_VALUE;

    if(mDecoder->canProcessWork())
        return C2_OK;

    return C2_BAD_VALUE;
}

void IMXC2VideoDecoder::processWork(const std::unique_ptr<C2Work> &work) {
    ATRACE_CALL();
    int32_t fd, inputId;
    uint8_t* inputBuffer = nullptr;
    uint64_t timestamp;
    status_t ret = OK;

    if (!bGetGraphicBlockPool) {
        status_t err = mDecoder->setGraphicBlockPool(mOutputBlockPool);
        if (err != OK)
            return;
        bGetGraphicBlockPool = true;
    }

    work->result = C2_OK;
    work->workletsProcessed = 0u;
    work->worklets.front()->output.configUpdate.clear();
    work->worklets.front()->output.flags = work->input.flags;

    if (bSignalledError | bSignalOutputEos) {
        work->result = C2_BAD_VALUE;
        return fillEmptyWork(work);
    }

    C2ReadView view = mDummyReadView;
    uint32_t size = 0;
    uint32_t flags = work->input.flags;
    bool eos = ((work->input.flags & C2FrameData::FLAG_END_OF_STREAM) != 0);
    bool csd = ((work->input.flags & C2FrameData::FLAG_CODEC_CONFIG) != 0);
    if(eos)
        flags &= ~(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);

    if (!work->input.buffers.empty()) {
        C2ConstLinearBlock linearBlock =
            work->input.buffers[0]->data().linearBlocks().front();
        size = linearBlock.size();

        if(bFirstAfterStart && size > 0){
            fd = linearBlock.handle()->data[0];
            int32_t input_buffer_type = IMXGetBufferType(fd);
            mDecoder->setConfig(DEC_CONFIG_INPUT_BUFFER_TYPE, &input_buffer_type);
            if(input_buffer_type <= fsl::DMA_HEAP_SYSTEM_UNCACHED)
                bDmaInput = false;
            else
                bDmaInput = true;

            ALOGD("set bDmaInput = %d input_buffer_type=%d", bDmaInput,input_buffer_type);

            // enable secure mode conditions: secure decoder && secure memory
            if (!bSecure) {
                if (mName.find("secure") != std::string::npos && input_buffer_type == fsl::DMA_HEAP_SECURE) {
                    bSecure = true;
                    int secureMode = 1;
                    ALOGD("set secureMode 1");
                    mDecoder->setConfig(DEC_CONFIG_SECURE_MODE, &secureMode);
                    if(bImx8m){
                        //imx8m secure decoder use mmap with 2 buffers
                        input_buffer_type = fsl::DMA_HEAP_SYSTEM_UNCACHED;
                        mDecoder->setConfig(DEC_CONFIG_INPUT_BUFFER_TYPE, &input_buffer_type);
                        bDmaInput = false;
                        bImx8mSecure = true;
                        ALOGD("set bImx8m secureMode");
                    }
                }
            }

        }

        if(!bDmaInput || csd || bImx8mSecure){
            view = linearBlock.map().get();
            //size = view.capacity();

            if (view.capacity() && view.error()) {
                ALOGE("read view map failed %d", view.error());
                work->result = view.error();
                return;
            }
        }
    }

    if(bFirstAfterStart){

        status_t err = OK;

        bFirstAfterStart = false;

        err = setInternalParam();
        if (err)
            ALOGE("setInternalParam ERROR");

        err = mDecoder->start();
        if (err){
            ALOGE("mDecoder start ERROR");
            work->result = C2_BAD_VALUE;
            return;
        }
    }

    if (bFirstAfterFlush) {
        if (size > 0)
            bFirstAfterFlush = false;
        if (eos && 0 == size) {
            ALOGI("only eos, return immediately");
            bSignalOutputEos = true;
            fillEmptyWork(work);
            return;
        }
    }

    if (size > 0) {

        const C2ConstLinearBlock block = work->input.buffers[0]->data().linearBlocks().front();

        // use fd2 to get clear input data in secure mode
        if (bImx8mSecure) {
            #if (ANDROID_VERSION >= ANDROID12)
            if(C2Config::SM_READ_PROTECTED_WITH_ENCRYPTED == mIntf->getSecureBufferMode()){
                if (work->input.infoBuffers.size() > 0)
                    view = work->input.infoBuffers[0].data().linearBlocks().front().map().get();
            }
            #else
            if(work->input.buffers.size() == 2)
                view = work->input.buffers[1]->data().linearBlocks().front().map().get();
            #endif
            if (view.error()) {
                ALOGE("Could not get vitual address");
                work->result = C2_BAD_VALUE;
                return fillEmptyWork(work);
            }
        }

        if(!bDmaInput || csd || bImx8mSecure)
            inputBuffer = const_cast<uint8_t *>(view.data());

        fd = block.handle()->data[0];
        timestamp = work->input.ordinal.timestamp.peeku();
        inputId = static_cast<int32_t>(work->input.ordinal.frameIndex.peeku() & 0x3FFFFFFF);

        ALOGV("in buffer fd %d addr %p size %d timestamp %d frameindex %d, flags %x bDmaInput=%d,bSecure=%d",
              fd, inputBuffer, (int)size, (int)timestamp,
              (int)work->input.ordinal.frameIndex.peeku(), flags, bDmaInput, bSecure);

        ret = mDecoder->queueInput(inputBuffer, size, timestamp, flags, fd, inputId);

        // codec data won't have a picture out, mark c2work as processed directly
        if ((csd && !bImx8mSecure) || ret != OK) {
            work->input.buffers.clear();
            work->workletsProcessed = 1u;
            work->result = C2_OK;
        }
    }

    if (eos) {
        ALOGI("input eos, size=%d,ts=%d",size,(int)work->input.ordinal.timestamp.peeku());
        drainInternal(DRAIN_COMPONENT_WITH_EOS);
        bSignalOutputEos = true;
        return;
    } else if (work->input.buffers.empty()) {
        fillEmptyWork(work);
        return;
    }
}

c2_status_t IMXC2VideoDecoder::drainInternal(uint32_t drainMode) {
    // trigger decoding to drain internel input buffers
    if (drainMode == NO_DRAIN) {
        ALOGW("drain with NO_DRAIN: no-op");
        return C2_OK;
    }
    if (drainMode == DRAIN_CHAIN) {
        ALOGW("DRAIN_CHAIN not supported");
        return C2_OMITTED;

    }
    // DRAIN_COMPONENT_WITH_EOS
    mDecoder->queueInput(nullptr, 0, 0, C2FrameData::FLAG_END_OF_STREAM, -1, -1);
    return C2_OK;
}

status_t IMXC2VideoDecoder::initInternalParam() {

    c2_status_t err = C2_OK;
    ALOGV("initInternalParam");

    C2StreamVendorSubFormat::output subFormat(0);
    err = intf()->query_vb({&subFormat,}, {}, C2_DONT_BLOCK, nullptr);
    if (err == C2_OK && subFormat.value != 0) {
        int32_t vc1SubFormat = subFormat.value;
        ALOGV("SET DEC_CONFIG_VC1_SUB_FORMAT vc1SubFormat=%x",vc1SubFormat);
        mDecoder->setConfig(DEC_CONFIG_VC1_SUB_FORMAT, &vc1SubFormat);
    }

    int inputDelayValue;
    if (OK == mDecoder->getConfig(DEC_CONFIG_INPUT_DELAY, &inputDelayValue)) {

        if(inputDelayValue >= kSmoothnessFactor)
            inputDelayValue -= kSmoothnessFactor;
        else
            inputDelayValue = 0;
        C2PortDelayTuning::input inputDelay(inputDelayValue);
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        ALOGV("inputDelayValue =%d",inputDelayValue);
        (void)mIntf->config({&inputDelay}, C2_MAY_BLOCK, &failures);
    }

    int outputDelayValue;
    if (OK == mDecoder->getConfig(DEC_CONFIG_OUTPUT_DELAY, &outputDelayValue)) {
        C2PortActualDelayTuning::output outputDelay(outputDelayValue);
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        (void)mIntf->config({&outputDelay}, C2_MAY_BLOCK, &failures);
    }

    C2GlobalLowLatencyModeTuning low_latency(0);
    err = intf()->query_vb({&low_latency,}, {}, C2_DONT_BLOCK, nullptr);
    if (err == C2_OK) {
        int32_t enable = (int32_t)low_latency.value;
        ALOGV("SET C2StreamVendorLowLatency enable=%d",enable);
        (void)mDecoder->setConfig(DEC_CONFIG_LOW_LATENCY, &enable);
    }
    int32_t target_fmt;

    #if (ANDROID_VERSION >= ANDROID12)
        target_fmt = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    #else
        target_fmt = HAL_PIXEL_FORMAT_YV12;
    #endif
    if (target_fmt == mIntf->getRawPixelFormat()) {
        // user force output pixel format to be YV12, while VPU don't support YV12, just use NV12
        uint32_t force_fmt = HAL_PIXEL_FORMAT_YCbCr_420_SP;
        (void)mDecoder->setConfig(DEC_CONFIG_FORCE_PIXEL_FORMAT, &force_fmt);
    }

    //only query the config when security decoder component created.
    if(mName.find("secure") != std::string::npos){
        C2Config::secure_mode_t secure_buffer_mode = C2Config::SM_UNPROTECTED;
        if (OK == mDecoder->getConfig(DEC_CONFIG_SECURE_BUFFER_MODE, &secure_buffer_mode)) {
            ALOGI("query DEC_CONFIG_SECURE_BUFFER_MODE =%d",(int)secure_buffer_mode);
            C2SecureModeTuning secureMode(secure_buffer_mode);
            std::vector<std::unique_ptr<C2SettingResult>> failures;
            (void)mIntf->config({&secureMode}, C2_MAY_BLOCK, &failures);
        }
    }

    C2StreamMaxPictureSizeTuning::output max_size(0u, 320, 240);
    err = intf()->query_vb({&max_size,}, {}, C2_DONT_BLOCK, nullptr);
    if (err == C2_OK) {
        (void)mDecoder->setConfig(DEC_CONFIG_OUTPUT_MAX_WIDTH, &max_size.width);
        (void)mDecoder->setConfig(DEC_CONFIG_OUTPUT_MAX_HEIGHT, &max_size.height);
    }

    return OK;
}
status_t IMXC2VideoDecoder::setInternalParam() {
    c2_status_t err = C2_OK;
    ALOGV("start setInternalParam");

    C2StreamPictureSizeInfo::output size(0u, mWidth, mHeight);
    C2StreamMaxBufferSizeInfo::input max_size(0u, 0u);
    err = intf()->query_vb({&size,&max_size}, {}, C2_DONT_BLOCK, nullptr);
    if (err == C2_OK) {
        mWidth = size.width;
        mHeight = size.height;

        VideoFormat vFormat;
        mDecoder->getConfig(DEC_CONFIG_INPUT_FORMAT, &vFormat);
        vFormat.width = mWidth;
        vFormat.height = mHeight;
        vFormat.bufferSize = max_size.value;
        mDecoder->setConfig(DEC_CONFIG_INPUT_FORMAT, &vFormat);

        memset(&vFormat, 0, sizeof(VideoFormat));
        mDecoder->getConfig(DEC_CONFIG_OUTPUT_FORMAT, &vFormat);
        vFormat.width = mWidth;
        vFormat.height = mHeight;
        mDecoder->setConfig(DEC_CONFIG_OUTPUT_FORMAT, &vFormat);
    }

    C2StreamUsageTuning::output usage(0u, 0);
    err = intf()->query_vb({&usage,}, {}, C2_DONT_BLOCK, nullptr);
    if (err == C2_OK) {
        uint64_t decoder_usage = usage.value;
        ALOGV("set DEC_CONFIG_OUTPUT_USAGE =%llx",(long long)decoder_usage);
        (void)mDecoder->setConfig(DEC_CONFIG_OUTPUT_USAGE, &decoder_usage);
    }

    return OK;
}

void IMXC2VideoDecoder::releaseDecoder() {
    ATRACE_CALL();
    if (mDecoder) {
        mDecoder->destroy();
        mDecoder.clear();
    }

}

void IMXC2VideoDecoder::handleOutputPicture(GraphicBlockInfo* info, uint64_t timestamp, uint32_t flag) {
    ATRACE_CALL();

    C2ConstGraphicBlock constBlock = info->mGraphicBlock->share(C2Rect(mCropWidth, mCropHeight), C2Fence());
    std::shared_ptr<C2Buffer> buffer = C2Buffer::CreateGraphicBuffer(std::move(constBlock));

    bool configUpdate = (bPendingFmtChanged || bFlushDone);
    uint32_t outputDelayValue = 0;
    C2PortActualDelayTuning::output outputDelay(0);
    C2StreamPictureSizeInfo::output size(0u, mWidth, mHeight);
    C2StreamPixelFormatInfo::output fmt(0u, HAL_PIXEL_FORMAT_YCBCR_420_888);
    C2StreamCropRectInfo::output crop(0u, C2Rect(mCropWidth, mCropHeight));
    C2StreamVendorInterlacedFormat::output interlace(0u, 0);
    C2StreamVendorUVOffset::output uvOffset(0u, 0);

    if (bSupportColorAspects) {
        IntfImpl::Lock lock = mIntf->lock();
        buffer->setInfo(mIntf->getColorAspects_l());
    }

    if (bPendingFmtChanged) {
        DecStaticHDRInfo hdrInfo;
        if (OK == mDecoder->getConfig(DEC_CONFIG_HDR10_STATIC_INFO, &hdrInfo)) {
            std::shared_ptr<C2StreamHdrStaticInfo::output> hdrStaticInfo =
                std::make_shared<C2StreamHdrStaticInfo::output>();
            hdrStaticInfo->mastering = {
                .red   = { .x = (float)(hdrInfo.mR[0]*0.00002),  .y = (float)(hdrInfo.mR[1]*0.00002) },
                .green = { .x = (float)(hdrInfo.mG[0]*0.00002),  .y = (float)(hdrInfo.mG[1]*0.00002) },
                .blue  = { .x = (float)(hdrInfo.mB[0]*0.00002),  .y = (float)(hdrInfo.mB[1]*0.00002) },
                .white = { .x = (float)(hdrInfo.mW[0]*0.00002),  .y = (float)(hdrInfo.mW[1]*0.00002) },
                .maxLuminance = (float)(hdrInfo.mMaxDisplayLuminance*1.0),
                .minLuminance = (float)(hdrInfo.mMinDisplayLuminance*0.0001),
            };
            hdrStaticInfo->maxCll = (float)(hdrInfo.mMaxContentLightLevel*1.0);
            hdrStaticInfo->maxFall = (float)(hdrInfo.mMaxFrameAverageLightLevel*1.0);
            buffer->setInfo(hdrStaticInfo);

            ALOGI("HdrStaticInfo: red(%0.5f, %0.5f) green(%0.5f, %0.5f) blue(%0.5f, %0.5f) white(%0.5f, %0.5f)",
                hdrStaticInfo->mastering.red.x, hdrStaticInfo->mastering.red.y,
                hdrStaticInfo->mastering.green.x, hdrStaticInfo->mastering.green.y,
                hdrStaticInfo->mastering.blue.x, hdrStaticInfo->mastering.blue.y,
                hdrStaticInfo->mastering.white.x, hdrStaticInfo->mastering.white.y);
            ALOGI("HdrStaticInfo: maxLuminance(%0.1f) minLuminance(%0.4f) maxCll(%0.1f) maxFall(%0.1f)",
                hdrStaticInfo->mastering.maxLuminance, hdrStaticInfo->mastering.minLuminance,
                hdrStaticInfo->maxCll, hdrStaticInfo->maxFall);
        }
    }

    if (configUpdate) {
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        c2_status_t err = intf()->query_vb(
            {
                &fmt,
                &outputDelay,
                &interlace,
                &uvOffset,
            },
            {},
            C2_DONT_BLOCK,
            nullptr);

        if (bPendingFmtChanged) {
            outputDelayValue = nOutBufferNum;

            if (outputDelay.value < outputDelayValue) {
                outputDelay.value = outputDelayValue;
                (void)mIntf->config({&outputDelay}, C2_MAY_BLOCK, &failures);
            }
        }

        ALOGD("configUpdate outputDelay.value=%d, fmt=%x ", outputDelay.value, fmt.value);

        // update config to decoder
        (void)mIntf->config({&fmt, &size, &crop}, C2_MAY_BLOCK, &failures);
    }

    auto fillWork = [buffer, timestamp, configUpdate, crop, size, outputDelay, fmt, interlace, uvOffset, intf = this->mIntf]
                    (const std::unique_ptr<C2Work> &work) mutable {

        uint32_t flags = 0;
        if (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) {
            flags |= C2FrameData::FLAG_END_OF_STREAM;
            ALOGD("signalling output eos");
        }
        if (configUpdate) {
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(crop));
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(size));
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(outputDelay));
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(fmt));
            work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(uvOffset));

            if(interlace.value == 1)
                work->worklets.front()->output.configUpdate.push_back(C2Param::Copy(interlace));
        }

        work->worklets.front()->output.flags = (C2FrameData::flags_t)flags;
        work->worklets.front()->output.buffers.clear();
        work->worklets.front()->output.buffers.push_back(buffer);
        work->worklets.front()->output.ordinal = work->input.ordinal;

        // if original timestamp is -1, use ts manager adjusted timestamp instead,
        // or frame with ts=-1 will be dropped by display, video become influent.
        if (-1 == work->input.ordinal.timestamp.peeku()) {
            work->worklets.front()->output.ordinal.timestamp = timestamp;
        }
        else if((timestamp != (uint64_t)-1) && work->input.ordinal.timestamp.peeku() > timestamp + 5*1000000){
            ALOGV("use ts %lld as output of input %lld", (long long)timestamp, (long long)work->input.ordinal.timestamp.peeku());
            work->worklets.front()->output.ordinal.timestamp = timestamp;
        }

        work->workletsProcessed = 1u;
        for (const std::unique_ptr<C2Param> &param: work->input.configUpdate) {
            if (param) {
                C2StreamHdr10PlusInfo::input *hdr10PlusInfo =
                        C2StreamHdr10PlusInfo::input::From(param.get());
                if (hdr10PlusInfo != nullptr) {
                    std::vector<std::unique_ptr<C2SettingResult>> failures;
                    std::unique_ptr<C2Param> outParam = C2Param::CopyAsStream(
                            *param.get(), true /*output*/, param->stream());
                    c2_status_t err = intf->config(
                            { outParam.get() }, C2_MAY_BLOCK, &failures);
                    if (err == C2_OK) {
                        work->worklets.front()->output.configUpdate.push_back(
                                C2Param::Copy(*outParam.get()));
                    } else {
                        ALOGW("fillWork: Config update size failed");
                    }
                    break;
                }
            }
        }
    };

    c2_status_t err = finish(timestamp, fillWork);

    info->mState = GraphicBlockInfo::State::OWNED_BY_CLIENT;
    bPendingFmtChanged = false;
    bFlushDone = false;

    if (C2_NOT_FOUND == err) {
        // no need to return buffer to post processor because its reference is clear.
        mDecoder->returnOutputBufferToDecoder(info->mBlockId);
    } else if (C2PlatformAllocatorStore::BUFFERQUEUE != mOutputBlockPool->getAllocatorId()) {
        info->mGraphicBlock.reset();
    }
}


// callbacks for VideoDecoderBase

// update intf configure
void IMXC2VideoDecoder::notifyVideoInfo(VideoFormat *pFormat) {
    ATRACE_CALL();

    status_t err = OK;
    C2StreamPixelFormatInfo::output pixel_fmt(0u, pFormat->pixelFormat);
    C2StreamVendorInterlacedFormat::output interlace(0u, pFormat->interlaced ? 1: 0);
    C2StreamVendorUVOffset::output uvOffset(0u, 0);

    nOutBufferNum = pFormat->bufferNum;

    mWidth = pFormat->width;
    mHeight = pFormat->height;
    mCropWidth = pFormat->rect.right - pFormat->rect.left;
    mCropHeight = pFormat->rect.bottom- pFormat->rect.top;
    (void)mDecoder->getConfig(DEC_CONFIG_OUTPUT_UV_OFFSET, &uvOffset.value);

    ALOGV("%s mWidth=%d,mHeight=%d,mCropWidth=%d,mCropHeight=%d,nOutBufferNum=%d,pixel_fmt=%x, uvoffset=%d",
        __func__, mWidth,mHeight,mCropWidth, mCropHeight,nOutBufferNum, pixel_fmt.value, uvOffset.value);

    std::vector<std::unique_ptr<C2SettingResult>> failures;
    (void)mIntf->config({&pixel_fmt, &interlace, &uvOffset}, C2_MAY_BLOCK, &failures);

    DecIsoColorAspects colorAspects;
    if (OK == mDecoder->getConfig(DEC_CONFIG_COLOR_ASPECTS, &colorAspects)) {
        ColorAspects aspects;
        C2StreamColorAspectsInfo::input codedAspects = { 0u };
        uint32_t primaries = colorAspects.colourPrimaries;
        uint32_t transfer = colorAspects.transferCharacteristics;
        uint32_t matrix = colorAspects.matrixCoeffs;
        bool range = colorAspects.fullRange;
        memset(&aspects, 0, sizeof(aspects));
        ColorUtils::convertIsoColorAspectsToCodecAspects(primaries, transfer, matrix, range, aspects);
        if (!C2Mapper::map(aspects.mPrimaries, &codedAspects.primaries)) {
            codedAspects.primaries = C2Color::PRIMARIES_UNSPECIFIED;
        }
        if (!C2Mapper::map(aspects.mTransfer, &codedAspects.transfer)) {
            codedAspects.transfer = C2Color::TRANSFER_UNSPECIFIED;
        }
        if (!C2Mapper::map(aspects.mMatrixCoeffs, &codedAspects.matrix)) {
            codedAspects.matrix = C2Color::MATRIX_UNSPECIFIED;
        }
        if (!C2Mapper::map(aspects.mRange, &codedAspects.range)) {
            codedAspects.range = C2Color::RANGE_UNSPECIFIED;
        }
        ALOGD("C2 color aspect p %d r %d m %d t %d",
            codedAspects.primaries, codedAspects.range,
            codedAspects.matrix, codedAspects.transfer);
        (void)mIntf->config({&codedAspects}, C2_MAY_BLOCK, &failures);
    }

    //filter run in pass through mode, use filter pool id for decoder output pool
    if(mIntf->getVenderHalFormat() == HAL_PIXEL_FORMAT_NV12_TILED && pFormat->pixelFormat == HAL_PIXEL_FORMAT_NV12_TILED){
        uint64_t decoder_usage = mIntf->getVenderUsage();
        (void)mDecoder->setConfig(DEC_CONFIG_OUTPUT_USAGE, &decoder_usage);

        std::shared_ptr<C2BlockPool> filterPool;
        C2BlockPool::local_id_t poolId = mIntf->getVenderBlockPool();
        if(OK == GetCodec2BlockPool(poolId, shared_from_this(), &filterPool)){
            mOutputBlockPool.reset();
            mOutputBlockPool.swap(filterPool);
            mDecoder->setGraphicBlockPool(mOutputBlockPool);
            ALOGD("notifyVideoInfo reset poolId=%llu,usage=%lld",(long long)poolId, (long long)decoder_usage);
        }
    }

    bPendingFmtChanged = true;
}


void IMXC2VideoDecoder::notifyPictureReady(int32_t pictureId, uint64_t timestamp) {
    ALOGV("notifyPictureReady picture id=%d, ts=%lld", (int)pictureId, (long long)timestamp);

    GraphicBlockInfo* info = mDecoder->getGraphicBlockById(pictureId);

    handleOutputPicture(info, timestamp, 0);

}

void IMXC2VideoDecoder::notifyInputBufferUsed(int32_t input_id) {
    nUsedFrameIndex = static_cast<uint64_t>(input_id);
    (void)postClearInputMsg(nUsedFrameIndex);
}

void IMXC2VideoDecoder::notifySkipInputBuffer(int32_t input_id) {
    skipOnePendingWork((uint64_t)input_id);
}

void IMXC2VideoDecoder::notifyFlushDone() {
    bFlushDone = true;
}

void IMXC2VideoDecoder::notifyResetDone() {
}

void IMXC2VideoDecoder::notifyError(status_t err) {
    bSignalledError = true;
    (void)finishWithException(false/*eos*/, false/*force*/);
    ALOGE("video decoder notify with error %d", err);
}

void IMXC2VideoDecoder::notifyEos() {

    (void)finishWithException(true/*eos*/, !bRecieveOutputEos);

    // fill empty work and sigal eos
    bRecieveOutputEos = true;
}

class IMXC2VideoDecoderFactory : public C2ComponentFactory {
public:
    IMXC2VideoDecoderFactory(C2String name)
        : mHelper(std::static_pointer_cast<C2ReflectorHelper>(GetImxC2Store()->getParamReflector())),
          mComponentName(name) {
    }

    virtual c2_status_t createComponent(
            c2_node_id_t id,
            std::shared_ptr<C2Component>* const component,
            std::function<void(C2Component*)> deleter) override {
        *component = std::shared_ptr<C2Component>(
                new IMXC2VideoDecoder(mComponentName.c_str(),
                                 id,
                                 std::make_shared<IMXC2VideoDecoder::IntfImpl>(mHelper, mComponentName.c_str())),
                deleter);
        return C2_OK;
    }

    virtual c2_status_t createInterface(
            c2_node_id_t id,
            std::shared_ptr<C2ComponentInterface>* const interface,
            std::function<void(C2ComponentInterface*)> deleter) override {
        *interface = std::shared_ptr<C2ComponentInterface>(
                new IMXInterface<IMXC2VideoDecoder::IntfImpl>(
                        mComponentName, id, std::make_shared<IMXC2VideoDecoder::IntfImpl>(mHelper, mComponentName)),
                deleter);
        return C2_OK;
    }

    //typedef ::C2ComponentFactory* (*IMXCreateCodec2FactoryFunc)(C2String name);

    virtual ~IMXC2VideoDecoderFactory() override = default;

private:
    std::shared_ptr<C2ReflectorHelper> mHelper;
    C2String mComponentName;
};

extern "C" ::C2ComponentFactory* IMXCreateCodec2Factory(C2String name) {
    ALOGV("in %s", __func__);
    return new ::android::IMXC2VideoDecoderFactory(name);
}

extern "C" void IMXDestroyCodec2Factory(::C2ComponentFactory* factory) {
    ALOGV("in %s", __func__);
    delete factory;
}


} // namespcae android

/* end of file */
