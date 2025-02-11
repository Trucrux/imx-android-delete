/**
 *  Copyright 2019 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#define LOG_TAG "IMXC2Interface"
#include <utils/Log.h>

// use MediaDefs here vs. MediaCodecConstants as this is not MediaCodec specific/dependent
#include <media/stagefright/foundation/MediaDefs.h>

#include <IMXC2Interface.h>

namespace android {

/* IMXInterface */
static C2R SubscribedParamIndicesSetter(
        bool mayBlock, C2InterfaceHelper::C2P<C2SubscribedParamIndicesTuning> &me) {
    (void)mayBlock;
    (void)me;

    return C2R::Ok();
}

IMXInterface<void>::BaseParams::BaseParams(
        const std::shared_ptr<C2ReflectorHelper> &reflector,
        C2String name,
        C2Component::kind_t kind,
        C2Component::domain_t domain,
        C2String mediaType,
        std::vector<C2String> aliases)
    : C2InterfaceHelper(reflector) {
    setDerivedInstance(this);

    addParameter(
            DefineParam(mName, C2_PARAMKEY_COMPONENT_NAME)
            .withConstValue(AllocSharedString<C2ComponentNameSetting>(name.c_str()))
            .build());

    if (aliases.size()) {
        C2String joined;
        for (const C2String &alias : aliases) {
            if (joined.length()) {
                joined += ",";
            }
            joined += alias;
        }
        addParameter(
                DefineParam(mAliases, C2_PARAMKEY_COMPONENT_ALIASES)
                .withConstValue(AllocSharedString<C2ComponentAliasesSetting>(joined.c_str()))
                .build());
    }

    addParameter(
            DefineParam(mKind, C2_PARAMKEY_COMPONENT_KIND)
            .withConstValue(new C2ComponentKindSetting(kind))
            .build());

    addParameter(
            DefineParam(mDomain, C2_PARAMKEY_COMPONENT_DOMAIN)
            .withConstValue(new C2ComponentDomainSetting(domain))
            .build());

    // simple interfaces have single streams
    addParameter(
            DefineParam(mInputStreamCount, C2_PARAMKEY_INPUT_STREAM_COUNT)
            .withConstValue(new C2PortStreamCountTuning::input(1))
            .build());

    addParameter(
            DefineParam(mOutputStreamCount, C2_PARAMKEY_OUTPUT_STREAM_COUNT)
            .withConstValue(new C2PortStreamCountTuning::output(1))
            .build());

    C2String inputStreamType = mediaType;
    C2String outputStreamType = mediaType;
    C2BufferData::type_t inputBufferType = C2BufferData::LINEAR;
    C2BufferData::type_t outputBufferType = C2BufferData::LINEAR;
    C2Allocator::id_t inputAllocator = C2AllocatorStore::DEFAULT_LINEAR;
    C2Allocator::id_t outputAllocator = C2AllocatorStore::DEFAULT_LINEAR;
    C2BlockPool::local_id_t outputPoolId = C2BlockPool::BASIC_LINEAR;
    C2String rawMediaType = MEDIA_MIMETYPE_VIDEO_RAW;

    if(domain == C2Component::DOMAIN_AUDIO){
        rawMediaType = MEDIA_MIMETYPE_AUDIO_RAW;
        if(kind == C2Component::KIND_DECODER){
            inputStreamType = mediaType;
            inputBufferType = C2BufferData::LINEAR;
            outputStreamType = rawMediaType;
            outputBufferType = C2BufferData::LINEAR;
        }
    }
    else if(domain == C2Component::DOMAIN_VIDEO || domain == C2Component::DOMAIN_IMAGE){
        switch(kind){
            case C2Component::KIND_DECODER:
            {
                inputStreamType = mediaType;
                inputBufferType = C2BufferData::LINEAR;
                outputStreamType = rawMediaType;
                outputBufferType = C2BufferData::GRAPHIC;
                inputAllocator = C2AllocatorStore::DEFAULT_LINEAR;
                outputAllocator = C2AllocatorStore::DEFAULT_GRAPHIC;
                outputPoolId = C2BlockPool::BASIC_GRAPHIC;
                break;
            }
            case C2Component::KIND_ENCODER:{
                inputStreamType = rawMediaType;
                inputBufferType = C2BufferData::GRAPHIC;
                outputStreamType = mediaType;
                outputBufferType = C2BufferData::LINEAR;
                inputAllocator = C2AllocatorStore::DEFAULT_GRAPHIC;
                outputAllocator = C2AllocatorStore::DEFAULT_LINEAR;
                outputPoolId = C2BlockPool::BASIC_LINEAR;
                break;
            }
            case C2Component::KIND_OTHER:{
                if(strstr(name.c_str(), "filter")){
                    inputStreamType = rawMediaType;
                    inputBufferType = C2BufferData::GRAPHIC;
                    outputStreamType = rawMediaType;
                    outputBufferType = C2BufferData::GRAPHIC;
                    inputAllocator = C2AllocatorStore::DEFAULT_GRAPHIC;
                    outputAllocator = C2AllocatorStore::DEFAULT_GRAPHIC;
                    outputPoolId = C2BlockPool::BASIC_GRAPHIC;
                }else if(strstr(name.c_str(), "prefil")){
                    inputStreamType = rawMediaType;
                    inputBufferType = C2BufferData::GRAPHIC;
                    outputStreamType = rawMediaType;
                    outputBufferType = C2BufferData::GRAPHIC;
                    inputAllocator = C2AllocatorStore::DEFAULT_GRAPHIC;
                    outputAllocator = C2AllocatorStore::DEFAULT_GRAPHIC;
                    outputPoolId = C2BlockPool::BASIC_GRAPHIC;
                }
                break;
            }
            default:
                break;
        }
    }

    addParameter(
            DefineParam(mInputFormat, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE)
            .withConstValue(new C2StreamBufferTypeSetting::input(
                    0u, inputBufferType))
            .build());

    addParameter(
            DefineParam(mInputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
            .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>(
                    inputStreamType))
            .build());

    addParameter(
            DefineParam(mOutputFormat, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE)
            .withConstValue(new C2StreamBufferTypeSetting::output(
                    0u, outputBufferType))
            .build());

    addParameter(
            DefineParam(mOutputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
            .withConstValue(AllocSharedString<C2PortMediaTypeSetting::output>(
                    outputStreamType))
            .build());

    C2Allocator::id_t inputAllocators[1] = { inputAllocator };
    C2Allocator::id_t outputAllocators[1] = { outputAllocator };
    C2BlockPool::local_id_t outputPoolIds[1] = { outputPoolId };

    addParameter(
            DefineParam(mInputAllocators, C2_PARAMKEY_INPUT_ALLOCATORS)
            .withDefault(C2PortAllocatorsTuning::input::AllocShared(inputAllocators))
            .withFields({ C2F(mInputAllocators, m.values[0]).any(),
                          C2F(mInputAllocators, m.values).inRange(0, 1) })
            .withSetter(Setter<C2PortAllocatorsTuning::input>::NonStrictValuesWithNoDeps)
            .build());

    addParameter(
            DefineParam(mOutputAllocators, C2_PARAMKEY_OUTPUT_ALLOCATORS)
            .withDefault(C2PortAllocatorsTuning::output::AllocShared(outputAllocators))
            .withFields({ C2F(mOutputAllocators, m.values[0]).any(),
                          C2F(mOutputAllocators, m.values).inRange(0, 1) })
            .withSetter(Setter<C2PortAllocatorsTuning::output>::NonStrictValuesWithNoDeps)
            .build());

    addParameter(
            DefineParam(mOutputPoolIds, C2_PARAMKEY_OUTPUT_BLOCK_POOLS)
            .withDefault(C2PortBlockPoolsTuning::output::AllocShared(outputPoolIds))
            .withFields({ C2F(mOutputPoolIds, m.values[0]).any(),
                          C2F(mOutputPoolIds, m.values).inRange(0, 1) })
            .withSetter(Setter<C2PortBlockPoolsTuning::output>::NonStrictValuesWithNoDeps)
            .build());

    // add stateless params
    addParameter(
            DefineParam(mSubscribedParamIndices, C2_PARAMKEY_SUBSCRIBED_PARAM_INDICES)
            .withDefault(C2SubscribedParamIndicesTuning::AllocShared(0u))
            .withFields({ C2F(mSubscribedParamIndices, m.values[0]).any(),
                          C2F(mSubscribedParamIndices, m.values).any() })
            #if (ANDROID_VERSION >= ANDROID13)
            .withSetter(SubscribedParamIndicesSetter)
            #else
            .withSetter(Setter<C2SubscribedParamIndicesTuning>::NonStrictValuesWithNoDeps)
            #endif
            .build());

    /* TODO

    addParameter(
            DefineParam(mCurrentWorkOrdinal, C2_PARAMKEY_CURRENT_WORK)
            .withDefault(new C2CurrentWorkTuning())
            .withFields({ C2F(mCurrentWorkOrdinal, m.timeStamp).any(),
                          C2F(mCurrentWorkOrdinal, m.frameIndex).any(),
                          C2F(mCurrentWorkOrdinal, m.customOrdinal).any() })
            .withSetter(Setter<C2CurrentWorkTuning>::NonStrictValuesWithNoDeps)
            .build());

    addParameter(
            DefineParam(mLastInputQueuedWorkOrdinal, C2_PARAMKEY_LAST_INPUT_QUEUED)
            .withDefault(new C2LastWorkQueuedTuning::input())
            .withFields({ C2F(mLastInputQueuedWorkOrdinal, m.timeStamp).any(),
                          C2F(mLastInputQueuedWorkOrdinal, m.frameIndex).any(),
                          C2F(mLastInputQueuedWorkOrdinal, m.customOrdinal).any() })
            .withSetter(Setter<C2LastWorkQueuedTuning::input>::NonStrictValuesWithNoDeps)
            .build());

    addParameter(
            DefineParam(mLastOutputQueuedWorkOrdinal, C2_PARAMKEY_LAST_OUTPUT_QUEUED)
            .withDefault(new C2LastWorkQueuedTuning::output())
            .withFields({ C2F(mLastOutputQueuedWorkOrdinal, m.timeStamp).any(),
                          C2F(mLastOutputQueuedWorkOrdinal, m.frameIndex).any(),
                          C2F(mLastOutputQueuedWorkOrdinal, m.customOrdinal).any() })
            .withSetter(Setter<C2LastWorkQueuedTuning::output>::NonStrictValuesWithNoDeps)
            .build());

    std::shared_ptr<C2OutOfMemoryTuning> mOutOfMemory;

    std::shared_ptr<C2PortConfigCounterTuning::input> mInputConfigCounter;
    std::shared_ptr<C2PortConfigCounterTuning::output> mOutputConfigCounter;
    std::shared_ptr<C2ConfigCounterTuning> mDirectConfigCounter;

    */
}

void IMXInterface<void>::BaseParams::noInputLatency() {
    addParameter(
            DefineParam(mRequestedInputDelay, C2_PARAMKEY_INPUT_DELAY_REQUEST)
            .withConstValue(new C2PortRequestedDelayTuning::input(0u))
            .build());

    addParameter(
            DefineParam(mActualInputDelay, C2_PARAMKEY_INPUT_DELAY)
            .withConstValue(new C2PortActualDelayTuning::input(0u))
            .build());
}

void IMXInterface<void>::BaseParams::noOutputLatency() {
    addParameter(
            DefineParam(mRequestedOutputDelay, C2_PARAMKEY_OUTPUT_DELAY_REQUEST)
            .withConstValue(new C2PortRequestedDelayTuning::output(0u))
            .build());

    addParameter(
            DefineParam(mActualOutputDelay, C2_PARAMKEY_OUTPUT_DELAY)
            .withConstValue(new C2PortActualDelayTuning::output(0u))
            .build());
}

void IMXInterface<void>::BaseParams::noPipelineLatency() {
    addParameter(
            DefineParam(mRequestedPipelineDelay, C2_PARAMKEY_PIPELINE_DELAY_REQUEST)
            .withConstValue(new C2RequestedPipelineDelayTuning(0u))
            .build());

    addParameter(
            DefineParam(mActualPipelineDelay, C2_PARAMKEY_PIPELINE_DELAY)
            .withConstValue(new C2ActualPipelineDelayTuning(0u))
            .build());
}

void IMXInterface<void>::BaseParams::noPrivateBuffers() {
    addParameter(
            DefineParam(mPrivateAllocators, C2_PARAMKEY_PRIVATE_ALLOCATORS)
            .withConstValue(C2PrivateAllocatorsTuning::AllocShared(0u))
            .build());

    addParameter(
            DefineParam(mMaxPrivateBufferCount, C2_PARAMKEY_MAX_PRIVATE_BUFFER_COUNT)
            .withConstValue(C2MaxPrivateBufferCountTuning::AllocShared(0u))
            .build());

    addParameter(
            DefineParam(mPrivatePoolIds, C2_PARAMKEY_PRIVATE_BLOCK_POOLS)
            .withConstValue(C2PrivateBlockPoolsTuning::AllocShared(0u))
            .build());
}

void IMXInterface<void>::BaseParams::noInputReferences() {
    addParameter(
            DefineParam(mMaxInputReferenceAge, C2_PARAMKEY_INPUT_MAX_REFERENCE_AGE)
            .withConstValue(new C2StreamMaxReferenceAgeTuning::input(0u))
            .build());

    addParameter(
            DefineParam(mMaxInputReferenceCount, C2_PARAMKEY_INPUT_MAX_REFERENCE_COUNT)
            .withConstValue(new C2StreamMaxReferenceCountTuning::input(0u))
            .build());
}

void IMXInterface<void>::BaseParams::noOutputReferences() {
    addParameter(
            DefineParam(mMaxOutputReferenceAge, C2_PARAMKEY_OUTPUT_MAX_REFERENCE_AGE)
            .withConstValue(new C2StreamMaxReferenceAgeTuning::output(0u))
            .build());

    addParameter(
            DefineParam(mMaxOutputReferenceCount, C2_PARAMKEY_OUTPUT_MAX_REFERENCE_COUNT)
            .withConstValue(new C2StreamMaxReferenceCountTuning::output(0u))
            .build());
}

void IMXInterface<void>::BaseParams::noTimeStretch() {
    addParameter(
            DefineParam(mTimeStretch, C2_PARAMKEY_TIME_STRETCH)
            .withConstValue(new C2ComponentTimeStretchTuning(1.f))
            .build());
}

/*
    Clients need to handle the following base params due to custom dependency.

    std::shared_ptr<C2ApiLevelSetting> mApiLevel;
    std::shared_ptr<C2ApiFeaturesSetting> mApiFeatures;
    std::shared_ptr<C2ComponentAttributesSetting> mAttrib;

    std::shared_ptr<C2PortSuggestedBufferCountTuning::input> mSuggestedInputBufferCount;
    std::shared_ptr<C2PortSuggestedBufferCountTuning::output> mSuggestedOutputBufferCount;

    std::shared_ptr<C2TrippedTuning> mTripped;

*/

} // namespace android

