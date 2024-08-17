
#ifdef ANDROID_DEBUG

//#define LOG_NDEBUG 0
#define LOG_TAG "vsi_daemon"
#include <log/log.h>
#include <android/log.h>
#include "vsi_daemon_debug.h"

#define LOG_BUF_SIZE 1024

void LogOutput(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];
    android_LogPriority android_level = ANDROID_LOG_VERBOSE;

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    switch(level){
        case HANTRO_LEVEL_ERROR:
            android_level = ANDROID_LOG_ERROR;
            break;
        case HANTRO_LEVEL_WARNING:
        case HANTRO_LEVEL_FIXME:
            android_level = ANDROID_LOG_WARN;
            break;
        case HANTRO_LEVEL_INFO:
            android_level = ANDROID_LOG_INFO;
            break;
        case HANTRO_LEVEL_DEBUG:
            android_level = ANDROID_LOG_DEBUG;
            break;
        case HANTRO_LEVEL_LOG:
            android_level = ANDROID_LOG_VERBOSE;
        default:
            break;
    };
    __android_log_write(android_level, LOG_TAG, buf);

    return;
}
#endif


