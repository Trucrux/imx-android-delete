/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef G2D_FILTER_FACTORY_H
#define G2D_FILTER_FACTORY_H

#include <util/C2InterfaceHelper.h>
#include <C2ComponentFactory.h>


namespace android {

class G2dFilterFactory : public C2ComponentFactory {
public:
    G2dFilterFactory(C2String name);
    ~G2dFilterFactory() override = default;

    // Implementation of C2ComponentFactory.
    c2_status_t createComponent(
            c2_node_id_t id, std::shared_ptr<C2Component>* const component,
            ComponentDeleter deleter = std::default_delete<C2Component>());
    c2_status_t createInterface(
            c2_node_id_t id, std::shared_ptr<C2ComponentInterface>* const interface,
            InterfaceDeleter deleter = std::default_delete<C2ComponentInterface>());

private:
    const std::string mComponentName;
    std::shared_ptr<C2ReflectorHelper> mReflector;
};


}
#endif//G2D_FILTER_FACTORY_H

