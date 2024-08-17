/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "G2dFilterFactory"
#include <utils/Log.h>
#include "G2dFilterFactory.h"
#include "G2dFilter.h"
#include "G2dFilterInterface.h"
#include "C2_imx.h"
#include <IMXC2Interface.h>

namespace android {

G2dFilterFactory::G2dFilterFactory(C2String name)
    :mComponentName(name),
     mReflector(std::static_pointer_cast<C2ReflectorHelper>(GetImxC2Store()->getParamReflector()))
{
    ALOGV("G2dFilterFactory name=%s",mComponentName.c_str());
}
c2_status_t G2dFilterFactory::createComponent(
            c2_node_id_t id, std::shared_ptr<C2Component>* const component,
            std::function<void(C2Component*)> deleter){

    if(mComponentName == G2dFilterInterface::NAME){

        auto intfImpl = std::make_shared<G2dFilterInterface>(mComponentName, id, mReflector);
        ALOGV("createComponent name=%s",mComponentName.c_str());
        *component = std::shared_ptr<C2Component>(
                new G2dFilter(id, mComponentName, mReflector, intfImpl),deleter);

        return C2_OK;
    }
    return C2_NOT_FOUND;
}
c2_status_t G2dFilterFactory::createInterface(
        c2_node_id_t id, std::shared_ptr<C2ComponentInterface>* const interface,
        std::function<void(C2ComponentInterface*)> deleter){

    if(mComponentName == G2dFilterInterface::NAME){
        ALOGV("createInterface name=%s",mComponentName.c_str());
        *interface = std::shared_ptr<C2ComponentInterface>(
                new IMXInterface<G2dFilterInterface>(mComponentName, id, std::make_shared<G2dFilterInterface>(mComponentName, id, mReflector)),
                deleter);

        return C2_OK;
    }
    return C2_NOT_FOUND;
}

extern "C" ::C2ComponentFactory* IMXCreateCodec2Factory(C2String name) {
    ALOGV("in %s", __func__);
    return new ::android::G2dFilterFactory(name);
}

extern "C" void IMXDestroyCodec2Factory(::C2ComponentFactory* factory) {
    ALOGV("in %s", __func__);
    delete factory;
}
}// namespcae android