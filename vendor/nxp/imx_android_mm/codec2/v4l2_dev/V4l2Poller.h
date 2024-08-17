/**
 *  Copyright 2022 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef POLLER_H
#define POLLER_H
#include "V4l2Dev.h"
namespace android {

class V4l2Poller : public AHandler{
public:
    V4l2Poller(V4l2Dev* const device, void* const caller);
    ~V4l2Poller();

    typedef status_t (*Callback)(void *me, status_t poll_ret);

    void initPoller(Callback func);
    void startPolling();
    void stopPolling();
    bool isPolling();

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg);
private:
    enum {
        kWhatStartPolling,
        kWhatPoll,
        kWhatStopPolling,
    };
    typedef enum {
        POLLER_NONE,
        POLLER_INITIALIZED,
        POLLER_POLLING,
        POLLER_STOPPING,
        POLLER_STOPPED,
    } PollerState;
    V4l2Dev* const mV4l2Dev;
    void* const mCaller;
    Callback mCallback;
    sp<ALooper> mLooper;
    PollerState mPollerState;


    void schedulePoll();
};
}  // namespace android
#endif
