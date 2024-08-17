/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */
#ifndef G2D_FILTER_INTERFACE_H
#define G2D_FILTER_INTERFACE_H

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


class G2dFilterInterface : public IMXInterface<void>::BaseParams {
public:
    G2dFilterInterface(C2String name, c2_node_id_t id, const std::shared_ptr<C2ReflectorHelper>& helper);
    static const std::string NAME;
    static const FilterPlugin_V1::Descriptor DESCRIPTOR;

    c2_node_id_t getId() { return mId; }

    static bool IsFilteringEnabled(const std::shared_ptr<C2ComponentInterface> &intf);

    static c2_status_t QueryParamsForPreviousComponent(
                const std::shared_ptr<C2ComponentInterface> &intf,
                std::vector<std::unique_ptr<C2Param>> *params);

    uint32_t getVenderHalFormat() const { return mVendorHalPixelFormat->value; }

    uint32_t getRawPixelFormat() const { return mPixelFormat->value; }
    uint64_t getOutputStreamUsage() const { return mOutUsage->value; }

private:

    std::shared_ptr<C2StreamPixelFormatInfo::output> mPixelFormat;
    std::shared_ptr<C2StreamVendorHalPixelFormat::output> mVendorHalPixelFormat;
    std::shared_ptr<C2StreamUsageTuning::output> mOutUsage;

    const std::string mComponentName;
    const c2_node_id_t mId;

};

}// namespace android
#endif//G2D_FILTER_INTERFACE_H
