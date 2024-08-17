/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */


//#define LOG_NDEBUG 0
#define LOG_TAG "G2dFilterInterface"

#include "G2dFilterInterface.h"
#include <media/stagefright/foundation/MediaDefs.h>
#include "graphics_ext.h"
#include "C2_imx.h"

namespace android {


// static
const std::string G2dFilterInterface::NAME = "c2.imx.g2d.filter";
// static
const FilterPlugin_V1::Descriptor G2dFilterInterface::DESCRIPTOR = {
    // controlParams
    { 
        C2StreamVendorHalPixelFormat::output::PARAM_TYPE,
        C2StreamUsageTuning::output::PARAM_TYPE,
    },
    // affectedParams
    {
        C2StreamPixelFormatInfo::output::PARAM_TYPE,
        C2StreamVendorHalPixelFormat::output::PARAM_TYPE,
    },
};

G2dFilterInterface::G2dFilterInterface(C2String name, c2_node_id_t id, const std::shared_ptr<C2ReflectorHelper>& helper)
        : IMXInterface<void>::BaseParams(
                helper,
                name,
                C2Component::KIND_OTHER,
                C2Component::DOMAIN_VIDEO,
                MEDIA_MIMETYPE_VIDEO_RAW),
                mComponentName(name),
                mId(id){

    ALOGV("%s(%s)", __func__, NAME.c_str());

    noPrivateBuffers(); // TODO: account for our buffers here
    noInputReferences();
    noOutputReferences();
    noTimeStretch();
    noInputLatency();
    //noOutputLatency();
    noPipelineLatency();

    mName = C2ComponentNameSetting::AllocShared(NAME.size() + 1);
    strncpy(mName->m.value, NAME.c_str(), NAME.size() + 1);

    addParameter(
            DefineParam(mPixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
            .withDefault(new C2StreamPixelFormatInfo::output(
                        0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
            .withFields({C2F(mPixelFormat, value).inRange(0, 0xffffffff)})
            .withSetter(Setter<decltype(*mPixelFormat)>::StrictValueWithNoDeps)
            .build());

    addParameter(
        DefineParam(mVendorHalPixelFormat, C2_PARAMKEY_VENDOR_HAL_PIXEL_FORMAT)
        .withDefault(new C2StreamVendorHalPixelFormat::output(0u, HAL_PIXEL_FORMAT_YCbCr_420_SP))
        .withFields({C2F(mVendorHalPixelFormat, value).oneOf({
                    HAL_PIXEL_FORMAT_YCbCr_420_SP,
                    HAL_PIXEL_FORMAT_NV12_TILED,
                    HAL_PIXEL_FORMAT_YCBCR_422_I,
                })})
        .withSetter(Setter<decltype(*mVendorHalPixelFormat)>::StrictValueWithNoDeps)
        .build());

    addParameter(
            DefineParam(mOutUsage, C2_PARAMKEY_OUTPUT_STREAM_USAGE)
            .withDefault(new C2StreamUsageTuning::output(0u,0))
            .withFields({C2F(mOutUsage, value).any()})
            .withSetter(Setter<decltype(*mOutUsage)>::StrictValueWithNoDeps)
            .build());

} 

bool G2dFilterInterface::IsFilteringEnabled([[maybe_unused]]
    const std::shared_ptr<C2ComponentInterface> &intf){
    return true;
}

c2_status_t G2dFilterInterface::QueryParamsForPreviousComponent(
                const std::shared_ptr<C2ComponentInterface> &intf,
                std::vector<std::unique_ptr<C2Param>> *params){

    c2_status_t err = C2_OK;

    C2StreamUsageTuning::output output_usage(0, 0);
    C2StreamVendorHalPixelFormat::output output_fmt(0, HAL_PIXEL_FORMAT_YCbCr_420_SP);

    std::vector<std::unique_ptr<C2Param>> query_params;
    err = intf->query_vb(
            {&output_fmt, &output_usage},
            { C2PortBlockPoolsTuning::output::PARAM_TYPE },
            C2_DONT_BLOCK,
            &query_params);

    if (err != C2_OK && err != C2_BAD_INDEX) {
        ALOGD("query err = %d", err);
        return err;
    }
    C2BlockPool::local_id_t poolId = C2BlockPool::BASIC_GRAPHIC;

    if (query_params.size()) {
        C2PortBlockPoolsTuning::output *outputPools =
            C2PortBlockPoolsTuning::output::From(query_params[0].get());
        if (outputPools && outputPools->flexCount() >= 1) {
            poolId = outputPools->m.values[0];
        }
    }

    ALOGV("QueryParamsForPreviousComponent output_fmt=%x,vendor_usage=%x,poolId=%lld",
        output_fmt.value, (int)output_usage.value, (long long)poolId);
    params->emplace_back(new C2StreamVendorUsageTuning::output(
            0u, output_usage.value));
    params->emplace_back(new C2StreamVendorHalPixelFormat::output(
        0u, output_fmt.value));
    params->emplace_back(new C2StreamVendorBlockPoolsTuning::output(
        0u, (uint32_t)(poolId & 0xFFFFFFFF)));

    params->emplace_back(new C2StreamUsageTuning::output(
            0u, C2AndroidMemoryUsage::HW_TEXTURE_READ));

    return C2_OK;
}

}//namespace android
