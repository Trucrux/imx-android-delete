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
const std::string G2dFilterInterface::NAME = "c2.imx.g2d.prefil";
// static
const FilterPlugin_V1::Descriptor G2dFilterInterface::DESCRIPTOR = {
    // controlParams
    { 
        C2StreamIntraRefreshTuning::output::PARAM_TYPE,
        C2StreamBitrateInfo::output::PARAM_TYPE,
        C2StreamRequestSyncFrameTuning::output::PARAM_TYPE,
        C2StreamPixelFormatInfo::input::PARAM_TYPE,
    },
    // affectedParams
    {
        C2StreamIntraRefreshTuning::output::PARAM_TYPE,
        C2StreamBitrateInfo::output::PARAM_TYPE,
        C2StreamRequestSyncFrameTuning::output::PARAM_TYPE,
        C2StreamPixelFormatInfo::input::PARAM_TYPE,
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
            DefineParam(mIntraRefresh, C2_PARAMKEY_INTRA_REFRESH)
            .withDefault(new C2StreamIntraRefreshTuning::output(
                    0u, C2Config::INTRA_REFRESH_DISABLED, 0.))
            .withFields({
                C2F(mIntraRefresh, mode).oneOf({
                    C2Config::INTRA_REFRESH_DISABLED, C2Config::INTRA_REFRESH_ARBITRARY }),
                C2F(mIntraRefresh, period).any()
            })
            .withSetter(IntraRefreshSetter)
            .build());

    addParameter(
            DefineParam(mBitrate, C2_PARAMKEY_BITRATE)
            .withDefault(new C2StreamBitrateInfo::output(0u, 64000))
            .withFields({C2F(mBitrate, value).inRange(4096, 12000000)})
            .withSetter(BitrateSetter)
            .build());

    addParameter(
            DefineParam(mRequestSync, C2_PARAMKEY_REQUEST_SYNC_FRAME)
            .withDefault(new C2StreamRequestSyncFrameTuning::output(0u, C2_FALSE))
            .withFields({C2F(mRequestSync, value).oneOf({ C2_FALSE, C2_TRUE }) })
            .withSetter(Setter<decltype(*mRequestSync)>::NonStrictValueWithNoDeps)
            .build());

    addParameter(
            DefineParam(mPixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
            .withDefault(new C2StreamPixelFormatInfo::input(
                        0u, HAL_PIXEL_FORMAT_YCbCr_420_SP))
            .withFields({C2F(mPixelFormat, value).inRange(0, 0xffffffff)})
            .withSetter(Setter<decltype(*mPixelFormat)>::StrictValueWithNoDeps)
            .build());
}
C2R G2dFilterInterface::IntraRefreshSetter(bool mayBlock, C2P<C2StreamIntraRefreshTuning::output> &me) {
    (void)mayBlock;
    C2R res = C2R::Ok();
    if (me.v.period < 1) {
        me.set().mode = C2Config::INTRA_REFRESH_DISABLED;
        me.set().period = 0;
    } else {
        // only support arbitrary mode (cyclic in our case)
        me.set().mode = C2Config::INTRA_REFRESH_ARBITRARY;
    }
    return res;
}
C2R G2dFilterInterface::BitrateSetter(bool mayBlock, C2P<C2StreamBitrateInfo::output> &me) {
    (void)mayBlock;
    C2R res = C2R::Ok();
    if (me.v.value <= 4096) {
        me.set().value = 4096;
    }
    return res;
}

bool G2dFilterInterface::IsFilteringEnabled([[maybe_unused]]
    const std::shared_ptr<C2ComponentInterface> &intf){
    return true;
}

c2_status_t G2dFilterInterface::QueryParamsForPreviousComponent(
                [[maybe_unused]]const std::shared_ptr<C2ComponentInterface> &intf,
                std::vector<std::unique_ptr<C2Param>> *params){

    params->emplace_back(new C2StreamPixelFormatInfo::input(
        0u, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));

    return C2_OK;
}


}//namespace android
