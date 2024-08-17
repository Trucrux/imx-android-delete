/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */
#ifndef ISI_FILTER_INTERFACE_H
#define ISI_FILTER_INTERFACE_H

#include <util/C2InterfaceHelper.h>
#include <C2AllocatorGralloc.h>
#include <C2Config.h>
#include <C2PlatformSupport.h>
#include <Codec2Mapper.h>
#include <codec2/hidl/plugin/FilterPlugin.h>
#include <IMXC2Interface.h>
#include <util/C2InterfaceHelper.h>
#include "C2Config_imx.h"

namespace android {


class IsiFilterInterface : public IMXInterface<void>::BaseParams {
public:
    IsiFilterInterface(C2String name, c2_node_id_t id, const std::shared_ptr<C2ReflectorHelper>& helper);
    static const std::string NAME;
    static const FilterPlugin_V1::Descriptor DESCRIPTOR;

    c2_node_id_t getId() { return mId; }

    static bool IsFilteringEnabled(const std::shared_ptr<C2ComponentInterface> &intf);

    static c2_status_t QueryParamsForPreviousComponent(
                const std::shared_ptr<C2ComponentInterface> &intf,
                std::vector<std::unique_ptr<C2Param>> *params);

    std::shared_ptr<C2StreamIntraRefreshTuning::output> getIntraRefresh_l() const { return mIntraRefresh; }
    std::shared_ptr<C2StreamBitrateInfo::output> getBitrate_l() const { return mBitrate; }
    std::shared_ptr<C2StreamRequestSyncFrameTuning::output> getRequestSync_l() const { return mRequestSync; }

    uint32_t getPixelFormat() const { return mPixelFormat->value; }
protected:

    static C2R IntraRefreshSetter(bool mayBlock, C2P<C2StreamIntraRefreshTuning::output> &me);
    static C2R BitrateSetter(bool mayBlock, C2P<C2StreamBitrateInfo::output> &me);
private:

    std::shared_ptr<C2StreamRequestSyncFrameTuning::output> mRequestSync;
    std::shared_ptr<C2StreamIntraRefreshTuning::output> mIntraRefresh;
    std::shared_ptr<C2StreamBitrateInfo::output> mBitrate;
    std::shared_ptr<C2StreamPixelFormatInfo::input> mPixelFormat;

    const std::string mComponentName;
    const c2_node_id_t mId;

};

}// namespace android
#endif//G2D_FILTER_INTERFACE_H
