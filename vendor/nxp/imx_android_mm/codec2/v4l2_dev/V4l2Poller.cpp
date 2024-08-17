/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */
 
//#define LOG_NDEBUG 0
#define LOG_TAG "V4l2Poller"

#include <utils/Log.h>
#include "V4l2Poller.h"
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ADebug.h>


namespace android {

V4l2Poller::V4l2Poller(V4l2Dev* const device, void* const caller):
    mV4l2Dev(device),
    mCaller(caller),
    mCallback(nullptr),
    mLooper(new ALooper()),
    mPollerState(POLLER_NONE)
{
    mLooper->setName("V4l2Poller");
}

V4l2Poller::~V4l2Poller() {
    mLooper->unregisterHandler(id());
    mLooper->stop();
    mPollerState = POLLER_NONE;
}

void V4l2Poller::initPoller(Callback func){
    if (mPollerState != POLLER_NONE)
        return;

    mPollerState = POLLER_INITIALIZED;
    mLooper->start(false, false, ANDROID_PRIORITY_VIDEO);
    mLooper->registerHandler(this);
    mCallback = func;
}

void V4l2Poller::startPolling() {
    if (mPollerState == POLLER_NONE) {
        ALOGE("Poller isn't initialized");
        return;
    }
    if(!isPolling())
        (new AMessage(kWhatStartPolling, this))->post();
}

void V4l2Poller::stopPolling() {

    if(mPollerState != POLLER_POLLING)
        return;

    mPollerState = POLLER_STOPPING;
    mV4l2Dev->SetPollInterrupt();
    (new AMessage(kWhatStopPolling, this))->post();
}

bool V4l2Poller::isPolling() {
    return (mPollerState == POLLER_POLLING);
}

void V4l2Poller::schedulePoll() {
    // post a message to start polling
    (new AMessage(kWhatPoll, this))->post();
}

void V4l2Poller::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatStartPolling:
        {
            if (isPolling())
                break; // do nothing
            ALOGV("kWhatStartPolling");

            mPollerState = POLLER_POLLING;
            schedulePoll();
            break;
        }
        case kWhatPoll:
        {
            if (!isPolling()) {
                ALOGV("quit from polling loop");
                return;
            }

            status_t ret = OK;

            ret = mV4l2Dev->Poll();

            if(OK != mCallback(mCaller, ret)){
                mPollerState = POLLER_STOPPED;
                break;
            }

            schedulePoll();
            break;
        }
        case kWhatStopPolling:
        {
            ALOGV("kWhatStopPolling");
            mPollerState = POLLER_STOPPED;
            mV4l2Dev->ClearPollInterrupt();
            break;
        }
        default:
            ALOGW("Unkown poller message!");
    }
}
};
