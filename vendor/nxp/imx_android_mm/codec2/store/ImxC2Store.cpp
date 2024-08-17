/**
 *  Copyright 2019-2023 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "ImxC2Store"
#include <log/log.h>

#include <stdio.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <C2ComponentFactory.h>
#include <C2Config.h>
#include <util/C2InterfaceHelper.h>
#include <C2AllocatorGralloc.h>
#include <C2AllocatorIon.h>
#if (ANDROID_VERSION >= ANDROID12)
#include <C2DmaBufAllocator.h>
#endif
#include <dlfcn.h>
#include <C2_imx.h>
#include <ion/ion.h>
#include <linux/version.h>
#include <ion_4.12.h>
#include "RegistryParser.h"
#include "graphics_ext.h"

namespace android {

typedef ::C2ComponentFactory* (*IMXCreateCodec2FactoryFunc)(C2String name);
typedef void (*IMXDestroyCodec2FactoryFunc)(::C2ComponentFactory*);

static int gNonSecureHeaps = 0;
static int gSecureHeaps = 0;
static void initIonHeaps();

class ImxC2Store : public C2ComponentStore {
public:
    ImxC2Store();

    virtual ~ImxC2Store() override = default;

    virtual C2String getName() const override;
    virtual std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override;
    virtual c2_status_t createComponent(
            C2String name, std::shared_ptr<C2Component> *const component) override;
    virtual c2_status_t createInterface(
            C2String name, std::shared_ptr<C2ComponentInterface> *const interface) override;

    virtual c2_status_t copyBuffer(
            std::shared_ptr<C2GraphicBuffer>src,
            std::shared_ptr<C2GraphicBuffer>dst ) override;

    virtual c2_status_t query_sm(
            const std::vector<C2Param*> &stackParams,
            const std::vector<C2Param::Index> &heapParamIndices,
            std::vector<std::unique_ptr<C2Param>> *const heapParams) const override;

    virtual c2_status_t config_sm(
            const std::vector<C2Param*> &params,
            std::vector<std::unique_ptr<C2SettingResult>> *const failures) override;

    virtual std::shared_ptr<C2ParamReflector> getParamReflector() const override;

    virtual c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>> *const params) const override;

    virtual c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery> &fields) const override;

private:
    std::map<C2String, C2String> mRoleMap;

    struct ComponentBox: public C2ComponentFactory, public std::enable_shared_from_this<ComponentBox>
    {
        ComponentBox(std::string alias, std::string libPath);

        c2_status_t init();

        ~ComponentBox() override;

        c2_status_t createComponent(
                c2_node_id_t id, std::shared_ptr<C2Component> *component,
                ComponentDeleter deleter = std::default_delete<C2Component>()) override;

        c2_status_t createInterface(
                c2_node_id_t id, std::shared_ptr<C2ComponentInterface> *interface,
                InterfaceDeleter deleter = std::default_delete<C2ComponentInterface>()) override;

        std::shared_ptr<const C2Component::Traits> getTraits();

    protected:

        std::string mName;
        std::string mLibPath;
        c2_status_t mInit; ///< initialization result
        void *mLibHandle; ///< loaded library handle

        std::shared_ptr<C2Component::Traits> mTraits; ///< cached component traits
        IMXCreateCodec2FactoryFunc createFactory; ///< loaded create function
        C2ComponentFactory::DestroyCodec2FactoryFunc destroyFactory; ///< loaded destroy function
        C2ComponentFactory *mComponentFactory; ///< loaded/created component factory

        std::recursive_mutex mLock; ///< lock protecting mTraits
        void setErrorTraits();
    };

    std::map<C2String, std::shared_ptr<android::ImxC2Store::ComponentBox>> mComponents; ///< map of name -> components
    std::vector<C2String> mComponentsList; ///< list of components

    std::shared_ptr<android::ImxC2Store::ComponentBox> findComponent(C2String name);
    bool isGSI();

    struct Interface : public C2InterfaceHelper {
        std::shared_ptr<C2StoreIonUsageInfo> mIonUsageInfo;
#if (ANDROID_VERSION >= ANDROID12)
        std::shared_ptr<C2StoreDmaBufUsageInfo> mDmaBufUsageInfo;
        std::shared_ptr<C2StoreFlexiblePixelFormatDescriptorsInfo> mPixelFormatDescriptorsInfo;
#endif

        Interface(std::shared_ptr<C2ReflectorHelper> reflector)
            : C2InterfaceHelper(reflector) {
            setDerivedInstance(this);
            struct Setter {
                static C2R setIonUsage(bool /* mayBlock */, C2P<C2StoreIonUsageInfo> &me) {
                    me.set().heapMask = gNonSecureHeaps;
                    me.set().minAlignment = 0;
                    if (me.set().usage & (C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE)) {
                        me.set().allocFlags = 1;    //ION_FLAG_CACHED;
                    } else {
                        me.set().allocFlags = 0;
                    }
                    if (me.set().usage & (C2MemoryUsage::READ_PROTECTED)) {
                        me.set().allocFlags = 0;
                        me.set().heapMask = gSecureHeaps;
                    }
                    return C2R::Ok();
                }
#if (ANDROID_VERSION >= ANDROID12)
                static C2R setDmaBufUsage(bool /* mayBlock */, C2P<C2StoreDmaBufUsageInfo> &me) {
                    long long usage = (long long)me.get().m.usage;
                    bool uncached = C2DmaBufAllocator::system_uncached_supported() &&
                                        !(usage & (C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE));

                    if (usage & USAGE_CONTINUOUS_BUFFER) {
                        if (uncached)
                            strncpy(me.set().m.heapName, "reserved-uncached", me.v.flexCount());
                        else
                            strncpy(me.set().m.heapName, "reserved", me.v.flexCount());
                    } else {
                        if (uncached)
                            strncpy(me.set().m.heapName, "system-uncached", me.v.flexCount());
                        else
                            strncpy(me.set().m.heapName, "system", me.v.flexCount());
                    }

                    if (usage & (C2MemoryUsage::READ_PROTECTED)) {
                        strncpy(me.set().m.heapName, "secure", me.v.flexCount());
                    }
                    // allocFlags not used in int BufferAllocator::
                    // Alloc(const std::string& heap_name, size_t len, unsigned int heap_flags, size_t legacy_align)
                    // me.set().m.allocFlags = 0;
                    return C2R::Ok();
                };

                static C2R setPixelFormatDescriptorsInfo(bool mayBlock, C2P<C2StoreFlexiblePixelFormatDescriptorsInfo> &me) {
                    (void)mayBlock;
                    (void)me;  // TODO: validate
                    ALOGI("setPixelFormatDescriptorsInfo");

                    return C2R::Ok();
                }

#endif
            };

            addParameter(
                DefineParam(mIonUsageInfo, "ion-usage")
                .withDefault(new C2StoreIonUsageInfo())
                .withFields({
                    C2F(mIonUsageInfo, usage).flags({C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                    C2F(mIonUsageInfo, capacity).inRange(0, UINT32_MAX, 1024),
                    C2F(mIonUsageInfo, heapMask).any(),
                    C2F(mIonUsageInfo, allocFlags).flags({}),
                    C2F(mIonUsageInfo, minAlignment).equalTo(0)
                })
                .withSetter(Setter::setIonUsage)
                .build());
#if (ANDROID_VERSION >= ANDROID12)
            addParameter(
                DefineParam(mDmaBufUsageInfo, "dmabuf-usage")
                .withDefault(C2StoreDmaBufUsageInfo::AllocShared(0))
                .withFields({
                    C2F(mDmaBufUsageInfo, m.usage).flags({C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                    C2F(mDmaBufUsageInfo, m.capacity).inRange(0, UINT32_MAX, 1024),
                    C2F(mDmaBufUsageInfo, m.allocFlags).flags({}),
                    C2F(mDmaBufUsageInfo, m.heapName).any(),
                })
                .withSetter(Setter::setDmaBufUsage)
                .build());

            C2FlexiblePixelFormatDescriptorStruct sPixelFmtInfo[]= {
                {HAL_PIXEL_FORMAT_YCbCr_420_SP, 8, C2Color::subsampling_t::YUV_420, C2Color::SEMIPLANAR_PACKED},
            };

            addParameter(
                DefineParam(mPixelFormatDescriptorsInfo, "vendor.FlexiblePixelFormatDescriptorsInfo")
                .withDefault(C2StoreFlexiblePixelFormatDescriptorsInfo::AllocShared(
                                                sizeof(sPixelFmtInfo)/sizeof(sPixelFmtInfo[0]) /* flexCount */, sPixelFmtInfo))
                .withFields({
                    C2F(mPixelFormatDescriptorsInfo, m.values[0].pixelFormat).inRange(0, UINT32_MAX, 1024),
                    C2F(mPixelFormatDescriptorsInfo, m.values[0].bitDepth).oneOf( {8, 10 }),
                    C2F(mPixelFormatDescriptorsInfo, m.values[0].subsampling).inRange(0, C2Color::subsampling_t::YUVA_444, 1024),
                    C2F(mPixelFormatDescriptorsInfo, m.values[0].layout).oneOf(
                        {C2Color::plane_layout_t::UNKNOWN_LAYOUT,
                        C2Color::plane_layout_t::PLANAR_PACKED,
                        C2Color::plane_layout_t::SEMIPLANAR_PACKED}),
                })

                .withSetter(Setter::setPixelFormatDescriptorsInfo)
                .build());
#endif
        }
    };
    std::shared_ptr<C2ReflectorHelper> mReflector;
    Interface mInterface;
};


ImxC2Store::ComponentBox::ComponentBox(std::string alias, std::string libPath)
    : mName(alias),
    mLibPath(libPath),
    mInit(C2_NO_INIT),
    mLibHandle(nullptr),
    mTraits(nullptr),
    createFactory(nullptr),
    destroyFactory(nullptr),
    mComponentFactory(nullptr)
{
}
void ImxC2Store::ComponentBox::setErrorTraits(){
    if(mTraits == nullptr){
       std::shared_ptr<C2Component::Traits> traits(new (std::nothrow) C2Component::Traits);
       traits->name = mName;
       traits->rank = 0x400;
       mTraits = traits;
       ALOGE("setErrorTraits name=%s",mName.c_str());
   };
}

c2_status_t ImxC2Store::ComponentBox::init()
{
    if(mInit == C2_OK){
        return C2_OK;
    }

    if(mInit == C2_NO_INIT){
        //std::lock_guard<std::mutex> lock(mMutex);
        std::unique_lock<std::recursive_mutex> lock(mLock);

        mLibHandle = dlopen(mLibPath.c_str(), RTLD_NOW|RTLD_NODELETE);
        ALOGV("ImxC2Store::ComponentBox init mLibHandle=%s mName=%s",mLibPath.c_str(),mName.c_str());
        if (mLibHandle == nullptr) {
            // could be access/symbol or simply not being there
            ALOGE("could not dlopen %s: %s", mLibPath.c_str(), dlerror());
            mInit = C2_CORRUPTED;
        } else {
            createFactory =
                (IMXCreateCodec2FactoryFunc)dlsym(mLibHandle, "IMXCreateCodec2Factory");
            destroyFactory =
                (IMXDestroyCodec2FactoryFunc)dlsym(mLibHandle, "IMXDestroyCodec2Factory");

            //TODO: add parameter in createFactory
            //or to have more CreateCodec2FactoryFuncs
            if(createFactory != nullptr){
                ALOGV("CALL createFactory");
                mComponentFactory = createFactory(mName);
            }

            if (mComponentFactory == nullptr) {
                ALOGE("could not create factory in %s", mLibPath.c_str());
                mInit = C2_NO_MEMORY;
            } else {
                mInit = C2_OK;
            }
        }

        if (mInit != C2_OK) {
            setErrorTraits();
            return mInit;
        }

        std::shared_ptr<C2ComponentInterface> intf;
        c2_status_t res = createInterface(0, &intf);

        if (res != C2_OK) {
            ALOGD("failed to create interface: %d", res);
            setErrorTraits();
            return mInit;
        }

        std::shared_ptr<C2Component::Traits> traits(new (std::nothrow) C2Component::Traits);
        if (traits) {
            if (mName != intf->getName()) {
                ALOGV("%s is alias to %s", mName.c_str(), intf->getName().c_str());
            }
            traits->name = mName;
            ALOGV("ImxC2Store::ComponentBox init mName=%s",mName.c_str());
            // TODO: get this from interface properly.
            bool encoder = (traits->name.find("encoder") != std::string::npos);
            uint32_t mediaTypeIndex = encoder ? C2PortMediaTypeSetting::output::PARAM_TYPE
                    : C2PortMediaTypeSetting::input::PARAM_TYPE;
            std::vector<std::unique_ptr<C2Param>> params;
            res = intf->query_vb({}, { mediaTypeIndex }, C2_MAY_BLOCK, &params);
            if (res != C2_OK) {
                ALOGD("failed to query interface: %d", res);
                return mInit;
            }
            if (params.size() != 1u) {
                ALOGD("failed to query interface: unexpected vector size: %zu", params.size());
                return mInit;
            }
            C2PortMediaTypeSetting *mediaTypeConfig = (C2PortMediaTypeSetting *)(params[0].get());
            if (mediaTypeConfig == nullptr) {
                ALOGD("failed to query media type");
                return mInit;
            }
            traits->mediaType = mediaTypeConfig->m.value;

            // TODO: define these values properly
            bool decoder = (traits->name.find("decoder") != std::string::npos);
            traits->kind =
                    decoder ? C2Component::KIND_DECODER :
                    encoder ? C2Component::KIND_ENCODER :
                    C2Component::KIND_OTHER;
            if (strncmp(traits->mediaType.c_str(), "audio/", 6) == 0) {
                traits->domain = C2Component::DOMAIN_AUDIO;
            } else if (strncmp(traits->mediaType.c_str(), "video/", 6) == 0) {
                traits->domain = C2Component::DOMAIN_VIDEO;
            } else if (strncmp(traits->mediaType.c_str(), "image/", 6) == 0) {
                traits->domain = C2Component::DOMAIN_IMAGE;
            } else {
                traits->domain = C2Component::DOMAIN_OTHER;
            }

            // set codec rank based on this prop, it is 0x111 as default
            int flag = property_get_int32("vendor.media.fsl_codec.flag", 7);
            bool useImxVideo = (flag & 0x2);
            bool useImxAudio = (flag & 0x4);

            if (traits->domain == C2Component::DOMAIN_VIDEO)
                traits->rank = (useImxVideo ? 0x1 : 0x400);
            else if (traits->domain == C2Component::DOMAIN_AUDIO)
                traits->rank = (useImxAudio ? 0x2 : 0x400);
            else
                traits->rank = 0x200;

            // set imx aac decoder's rank to the lowest, as these is only one aac decoder
            // for audio/mp4a-latm, imx aac decoder still is the default aac decoder, but
            // in CTS-on-GSI, google aac decoder has higher rank and it will be the default
            // aac decoder, then all aac CTS can pass.
            if (traits->name.find("c2.imx.aac.decoder") != std::string::npos) {
                traits->rank = 0x400;
            }

            // for CTS-on-GSI, as there is no imx extractors, set imx audio decoder the lowest priority.
            if (traits->domain == C2Component::DOMAIN_AUDIO) {
                if (access("/system/lib"
#ifdef __LP64__
                            "64"
#endif
                            "/extractors/libimxextractor.so", 0) != 0) {
                    traits->rank = 0x400;
                }
            }

            if (traits->domain == C2Component::DOMAIN_AUDIO
                && traits->name.find("decoder.hw") != std::string::npos) {
                    if(traits->rank > 1)
                        traits->rank -= 1;
                    else
                        ALOGE("Can't set audio dsp decoder to high priority!");
            }

            // for secure decoder, rank it a bit lower then normal decoder
            if (traits->name.find("decoder.secure") != std::string::npos) {
                traits->rank += 1;
            }
        }
        mTraits = traits;
    }

    return mInit;
};
ImxC2Store::ComponentBox::~ComponentBox() {
    ALOGV("in %s", __func__);
    if (destroyFactory && mComponentFactory) {
        destroyFactory(mComponentFactory);
    }
    if (mLibHandle) {
        ALOGV("unloading dll");
        dlclose(mLibHandle);
    }
}

c2_status_t ImxC2Store::ComponentBox::createComponent(
        c2_node_id_t id, std::shared_ptr<C2Component> *component,
        std::function<void(::C2Component*)> deleter) {
    component->reset();
    if (mInit != C2_OK) {
        ALOGE("ImxC2Store::ComponentBox::createComponent failed ret=%d",mInit);
        return mInit;
    }
    std::shared_ptr<ComponentBox> module = shared_from_this();
    c2_status_t res = mComponentFactory->createComponent(
            id, component, [module, deleter](C2Component *p) mutable {
                // capture module so that we ensure we still have it while deleting component
                deleter(p); // delete component first
                module.reset(); // remove module ref (not technically needed)
    });
    return res;
}

c2_status_t ImxC2Store::ComponentBox::createInterface(
        c2_node_id_t id, std::shared_ptr<C2ComponentInterface> *interface,
        std::function<void(::C2ComponentInterface*)> deleter) {
    interface->reset();
    if (mInit != C2_OK) {
        ALOGE("ImxC2Store::ComponentBox::createInterface failed ret=%d",mInit);
        return mInit;
    }
    std::shared_ptr<ComponentBox> module = shared_from_this();
    c2_status_t res = mComponentFactory->createInterface(
            id, interface, [module, deleter](C2ComponentInterface *p) mutable {
                // capture module so that we ensure we still have it while deleting interface
                deleter(p); // delete interface first
                module.reset(); // remove module ref (not technically needed)
    });
    return res;
}

std::shared_ptr<const C2Component::Traits> ImxC2Store::ComponentBox::getTraits() {
    std::unique_lock<std::recursive_mutex> lock(mLock);
    return mTraits;
}

ImxC2Store::ImxC2Store()
    : mReflector(std::make_shared<C2ReflectorHelper>()),
      mInterface(mReflector)
{
    ALOGV("ImxC2Store::ImxC2Store BEGIN");
    auto emplace = [this](const char *alias, const char *libPath) {

        // ComponentLoader is neither copiable nor movable, so it must be
        // constructed in-place. Now ComponentLoader takes two arguments in
        // constructor, so we need to use piecewise_construct to achieve this
        // behavior.
        mComponents.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(alias),
                std::forward_as_tuple(std::make_shared<ComponentBox>(alias, libPath)));
        mComponentsList.emplace_back(alias);
    };

    RegistryParser localRegParser;
    std::string register_file = "/vendor/etc/c2_component_register";
    localRegParser.ParseRegList((char*)register_file.c_str());

    int num = localRegParser.GetRegCompNum();
    if (num <= 0) {
        ALOGE("GetRegCompNum failed");
        return;
    }

    int i;
    REG_ENTRY * pReg;
    bool gsi = isGSI();
    for (i = 0; i < num; i++) {
        pReg = localRegParser.QueryRegComp(i);
        if (pReg) {
            ALOGV("emplace component %s lib %s", pReg->compName, pReg->libName);
            //skip prefil component for gsi image
            if(gsi && strstr(pReg->compName, "prefil"))
                continue;
            emplace(pReg->compName, pReg->libName);
        }
    }

    initIonHeaps();

    ALOGV("ImxC2Store::ImxC2Store END");
}

C2String ImxC2Store::getName() const {
    return "ImxC2Store";
}


std::shared_ptr<android::ImxC2Store::ComponentBox> ImxC2Store::findComponent(C2String name) {

    auto pos = mComponents.find(name);
    if (pos == mComponents.end()) {
        return nullptr;
    }
    return pos->second;
}

c2_status_t ImxC2Store::copyBuffer(
        std::shared_ptr<C2GraphicBuffer> src, std::shared_ptr<C2GraphicBuffer> dst) {
    (void)src;
    (void)dst;
    return C2_OMITTED;
}

c2_status_t ImxC2Store::createComponent(
        C2String name, std::shared_ptr<C2Component> *const component) {

    c2_status_t ret = C2_NOT_FOUND;

    // This method SHALL return within 100ms.
    component->reset();

    std::shared_ptr<ComponentBox> box = findComponent(name);

    if (box) {
        ret = box->init();
        if (ret == C2_OK) {
            // TODO: get a unique node ID
            ret = box->createComponent(0, component);
        }
    }

    ALOGV("ImxC2Store::createComponent name=%s, ret=%d",name.c_str(), ret);
    return ret;
}

c2_status_t ImxC2Store::createInterface(
        C2String name, std::shared_ptr<C2ComponentInterface> *const interface) {

    c2_status_t ret = C2_NOT_FOUND;

    // This method SHALL return within 100ms.
    interface->reset();

    std::shared_ptr<ComponentBox> box = findComponent(name);

    if (box) {
        ret = box->init();
        if (ret == C2_OK) {
            // TODO: get a unique node ID
            ret = box->createInterface(0, interface);
        }
    }

    ALOGV("ImxC2Store::createInterface !!! name=%s, ret=%d",name.c_str(), ret);
    return ret;
}

std::vector<std::shared_ptr<const C2Component::Traits>> ImxC2Store::listComponents() {
    // This method SHALL return within 500ms.
    std::vector<std::shared_ptr<const C2Component::Traits>> list;
    for (const C2String &alias : mComponentsList) {
        std::shared_ptr<ComponentBox> box = mComponents.at(alias);
        (void)box->init();
        std::shared_ptr<const C2Component::Traits> traits = box->getTraits();
        if (traits) {
            list.push_back(traits);
        }

    }

    return list;
};

c2_status_t ImxC2Store::query_sm(
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<C2Param>> *const heapParams) const {
    return mInterface.query(stackParams, heapParamIndices, C2_MAY_BLOCK, heapParams);
}

c2_status_t ImxC2Store::config_sm(
        const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>> *const failures) {
    return mInterface.config(params, C2_MAY_BLOCK, failures);
}

std::shared_ptr<C2ParamReflector> ImxC2Store::getParamReflector() const {
    return mReflector;
}

c2_status_t ImxC2Store::querySupportedParams_nb(
        std::vector<std::shared_ptr<C2ParamDescriptor>> *const params) const {
    return mInterface.querySupportedParams(params);
}
c2_status_t ImxC2Store::querySupportedValues_sm(
        std::vector<C2FieldSupportedValuesQuery> &fields) const {
    return mInterface.querySupportedValues(fields, C2_MAY_BLOCK);
}
bool ImxC2Store::isGSI(){
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.product.system_ext.name", value, "0");

    if(!strcmp(value, "gsi_arm") ||
        !strcmp(value, "gsi_arm64") ||
        !strcmp(value, "gsi_x86") ||
        !strcmp(value, "gsi_x86_64") ||
        !strcmp(value, "aosp_arm") ||
        !strcmp(value, "aosp_arm64") ||
        !strcmp(value, "aosp_x86") ||
        !strcmp(value, "aosp_x86_64") ||
        !strcmp(value, "aosp_tv_arm") ||
        !strcmp(value, "aosp_tv_arm64")){
        return true;
    }else{
        ALOGD("isGSI false value=%s",value);
        return false;
    }
}

std::shared_ptr<C2ComponentStore> GetImxC2Store() {
    static std::mutex mutex;
    static std::weak_ptr<C2ComponentStore> imxC2Store;
    std::lock_guard<std::mutex> lock(mutex);
    std::shared_ptr<C2ComponentStore> store = imxC2Store.lock();
    if (store == nullptr) {
        store = std::make_shared<android::ImxC2Store>();
        imxC2Store = store;
    }
    return store;
}

static void initIonHeaps() {
    // init non secure heap and secure heap
    int ionFd = ion_open();
    if (ionFd < 0) {
        ALOGE("ion open failed!");
        return;
    }
    int heapCnt = 0;
    int ret = ion_query_heap_cnt(ionFd, &heapCnt);
    if (ret != 0 || heapCnt == 0) {
        ALOGE("can't query heap count");
        close(ionFd);
        return;
    }

    struct ion_heap_data ihd[heapCnt];
    memset(&ihd, 0, sizeof(ihd));
    ret = ion_query_get_heaps(ionFd, heapCnt, &ihd);
    if (ret != 0) {
        ALOGE("can't get ion heaps");
        close(ionFd);
        return;
    }

    for (int i = 0; i < heapCnt; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 1)
        if (ihd[i].type == ION_HEAP_TYPE_SYSTEM_CONTIG) {
            gNonSecureHeaps |= 1 << ihd[i].heap_id;
            continue;
        }
        if (ihd[i].type == ION_HEAP_TYPE_DMA ||
                ihd[i].type == ION_HEAP_TYPE_CARVEOUT) {
#else
        if (ihd[i].type == ION_HEAP_TYPE_DMA) {
#endif
            gNonSecureHeaps |=  1 << ihd[i].heap_id;
            continue;
        }
        if (ihd[i].type == ION_HEAP_TYPE_SYSTEM) {
            gNonSecureHeaps |= 1 << ihd[i].heap_id;
            continue;
        }
        if (ihd[i].type == ION_HEAP_TYPE_UNMAPPED) {
            gSecureHeaps = 1 << ihd[i].heap_id;  //we have only one secure heap
            continue;
        }
    }
    close(ionFd);
}


}

