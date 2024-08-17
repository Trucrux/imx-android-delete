/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "IsiFilterFactory"
#include <utils/Log.h>
#include "IsiFilterFactory.h"
#include "IsiFilter.h"
#include "IsiFilterInterface.h"
#include "C2_imx.h"
#include <IMXC2Interface.h>

namespace android {

IsiFilterFactory::IsiFilterFactory(C2String name)
    :mComponentName(name),
     mReflector(std::static_pointer_cast<C2ReflectorHelper>(GetImxC2Store()->getParamReflector()))
{
    ALOGV("IsiFilterFactory name=%s",mComponentName.c_str());
}
c2_status_t IsiFilterFactory::createComponent(
            c2_node_id_t id, std::shared_ptr<C2Component>* const component,
            std::function<void(C2Component*)> deleter){

    if(mComponentName == IsiFilterInterface::NAME){

        auto intfImpl = std::make_shared<IsiFilterInterface>(mComponentName, id, mReflector);
        ALOGV("createComponent name=%s",mComponentName.c_str());
        *component = std::shared_ptr<C2Component>(
                new IsiFilter(id, mComponentName, mReflector, intfImpl),deleter);

        return C2_OK;
    }
    return C2_NOT_FOUND;
}
c2_status_t IsiFilterFactory::createInterface(
        c2_node_id_t id, std::shared_ptr<C2ComponentInterface>* const interface,
        std::function<void(C2ComponentInterface*)> deleter){

    if(mComponentName == IsiFilterInterface::NAME){
        ALOGV("createInterface name=%s",mComponentName.c_str());
        *interface = std::shared_ptr<C2ComponentInterface>(
                new IMXInterface<IsiFilterInterface>(mComponentName, id, std::make_shared<IsiFilterInterface>(mComponentName, id, mReflector)),
                deleter);

        return C2_OK;
    }
    return C2_NOT_FOUND;
}

extern "C" ::C2ComponentFactory* IMXCreateCodec2Factory(C2String name) {
    ALOGV("in %s", __func__);
    return new ::android::IsiFilterFactory(name);
}

extern "C" void IMXDestroyCodec2Factory(::C2ComponentFactory* factory) {
    ALOGV("in %s", __func__);
    delete factory;
}
}// namespcae android