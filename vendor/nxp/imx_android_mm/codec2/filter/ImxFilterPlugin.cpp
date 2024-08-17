/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */
 
//#define LOG_NDEBUG 0
#define LOG_TAG "ImxFilterPlugin"
#include <utils/Log.h>


#include <chrono>
#include <thread>

#include <codec2/hidl/plugin/FilterPlugin.h>

#include <C2AllocatorGralloc.h>
#include <C2Config.h>
#include <C2PlatformSupport.h>
#include <Codec2Mapper.h>
#include <util/C2InterfaceHelper.h>
#include "C2_imx.h"

#ifdef IMX8Q
#include "G2dFilterInterface.h"
#include "IsiFilterInterface.h"
#endif
#ifdef IMX8MM
#include "G2dFilterInterface.h"
#endif


namespace android {

class ImxFilterPlugin : public FilterPlugin_V1 {
public:
    ImxFilterPlugin() : mStore(GetImxC2Store()) {}
    ~ImxFilterPlugin() override = default;

    std::shared_ptr<C2ComponentStore> getComponentStore() override {
        return mStore;
    }

    bool describe(C2String name, Descriptor *desc) override {
        #ifdef IMX8Q
        if (name == G2dFilterInterface::NAME) {
            *desc = G2dFilterInterface::DESCRIPTOR;
            return true;
        }else if(name == IsiFilterInterface::NAME) {
            *desc = IsiFilterInterface::DESCRIPTOR;
            return true;
        }
        #endif
        #ifdef IMX8MM
        if (name == G2dFilterInterface::NAME) {
            *desc = G2dFilterInterface::DESCRIPTOR;
            return true;
        }
        #endif

        (void)name;
        (void)desc;
        return false;
    }

    bool isFilteringEnabled(const std::shared_ptr<C2ComponentInterface> &intf) override {
        #ifdef IMX8Q
        if (intf->getName() == G2dFilterInterface::NAME) {
            return G2dFilterInterface::IsFilteringEnabled(intf);
        }else if (intf->getName() == IsiFilterInterface::NAME) {
            return IsiFilterInterface::IsFilteringEnabled(intf);
        }
        #endif
        #ifdef IMX8MM
        if (intf->getName() == G2dFilterInterface::NAME) {
            return G2dFilterInterface::IsFilteringEnabled(intf);
        }
        #endif

        (void)intf;
        return false;
    }

    c2_status_t queryParamsForPreviousComponent(
            const std::shared_ptr<C2ComponentInterface> &intf,
            std::vector<std::unique_ptr<C2Param>> *params) override {
        #ifdef IMX8Q
        if (intf->getName() == G2dFilterInterface::NAME) {
            return G2dFilterInterface::QueryParamsForPreviousComponent(
                    intf, params);
        }else if (intf->getName() == IsiFilterInterface::NAME) {
            return IsiFilterInterface::QueryParamsForPreviousComponent(
                    intf, params);
        }
        #endif
        #ifdef IMX8MM
        if (intf->getName() == G2dFilterInterface::NAME) {
            return G2dFilterInterface::QueryParamsForPreviousComponent(
                    intf, params);
        }
        #endif
        (void)intf;
        (void)params;
        return C2_BAD_VALUE;
    }

private:
    std::shared_ptr<C2ComponentStore> mStore;
};

}

extern "C" {

int32_t GetFilterPluginVersion() {
    return ::android::ImxFilterPlugin::VERSION;
}

void *CreateFilterPlugin() {
    return new ::android::ImxFilterPlugin;
}

void DestroyFilterPlugin(void *plugin) {
    delete (::android::ImxFilterPlugin *)plugin;
}

}  // extern "C"

