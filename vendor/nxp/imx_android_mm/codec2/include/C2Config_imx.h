/**
 *  Copyright 2019-2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef C2CONFIG_IMX_H
#define C2CONFIG_IMX_H

#include <C2Config.h>

enum imx_aac_packaging_t : uint32_t {
    VENDOR_START = 10,
    AAC_PACKAGING_ADIF,
};

constexpr imx_aac_packaging_t C2AacStreamFormatAdif = AAC_PACKAGING_ADIF;



#define C2_PARAM_INDEX_VENDOR_START 0x8000 ///< vendor-defined parameters

#define kParamIndexVendorSubFormat C2_PARAM_INDEX_VENDOR_START
#define kParamIndexAudioBlockAlgin (C2_PARAM_INDEX_VENDOR_START + 1)
#define kParamIndexBitsPerFrame (C2_PARAM_INDEX_VENDOR_START + 2)
#define kParamIndexVendorHalPixelFormat (C2_PARAM_INDEX_VENDOR_START + 3)
#define kParamIndexBitsPerSample (C2_PARAM_INDEX_VENDOR_START + 4)
#define kParamIndexVendorLowLatency (C2_PARAM_INDEX_VENDOR_START + 5)
#define kParamIndexVendorInterlacedFormat (C2_PARAM_INDEX_VENDOR_START + 6)
#define kParamIndexVendorStreamUsageTuning (C2_PARAM_INDEX_VENDOR_START + 7)
#define kParamIndexVendorBlockPoolsTuning (C2_PARAM_INDEX_VENDOR_START + 8)
#define kParamIndexVendorUVOffset (C2_PARAM_INDEX_VENDOR_START + 9)


typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexVendorSubFormat> C2StreamVendorSubFormat;
constexpr char C2_PARAMKEY_VENDOR_SUB_FORMAT[] = "sub-format";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexVendorHalPixelFormat> C2StreamVendorHalPixelFormat;
constexpr char C2_PARAMKEY_VENDOR_HAL_PIXEL_FORMAT[] = "hal-pixel-format";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexAudioBlockAlgin> C2StreamAudioBlockAlign;
constexpr char C2_PARAMKEY_AUDIO_BLOCK_ALIGN[] = "audio-block-align";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexBitsPerFrame> C2StreamBitsPerFrame;
constexpr char C2_PARAMKEY_BITS_PER_FRAME[] = "bits-per-frame";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexBitsPerSample> C2StreamBitsPerSample;
constexpr char C2_PARAMKEY_BITS_PER_SAMPLE[] = "bits-per-sample";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexVendorLowLatency> C2StreamVendorLowLatency;
constexpr char C2_PARAMKEY_VENDOR_LOW_LATENCY[] = "low-latency";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexVendorInterlacedFormat> C2StreamVendorInterlacedFormat;
constexpr char C2_PARAMKEY_VENDOR_INTERLACED_FORMAT[] = "interlaced-format";

typedef C2StreamParam<C2Tuning, C2Uint64Value, kParamIndexVendorStreamUsageTuning> C2StreamVendorUsageTuning;
constexpr char C2_PARAMKEY_VENDOR_STREAM_USAGE[] = "vendor-stream-usage";

typedef C2StreamParam<C2Tuning, C2Uint32Value, kParamIndexVendorBlockPoolsTuning> C2StreamVendorBlockPoolsTuning;
constexpr char C2_PARAMKEY_VENDOR_BLOCK_POOLS[] = "vendor-buffers-pool-ids";

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexVendorUVOffset> C2StreamVendorUVOffset;
constexpr char C2_PARAMKEY_VENDOR_UV_OFFSET[] = "vendor-uv-offset";


#endif

