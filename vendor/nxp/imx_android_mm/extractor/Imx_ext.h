/**
 *  Copyright 2019-2022 NXP
 *  All Rights Reserved.
 */
#ifndef IMX_EXT_H
#define IMX_EXT_H
#include <media/stagefright/MediaDefs.h>

namespace android {

#define AMEDIAFORMAT_KEY_SUB_FORMAT "vendor.sub-format.value"
#define AMEDIAFORMAT_KEY_AUDIO_BLOCK_ALIGN "vendor.audio-block-align.value"
#define AMEDIAFORMAT_KEY_BITS_PER_FRAME "vendor.bits-per-frame.value"
#define AMEDIAFORMAT_KEY_IS_ADIF "is-adif"
#define AMEDIAFORMAT_KEY_LOW_LATENCY "vendor.low-latency.value"

#define MEDIA_MIMETYPE_TEXT_SRT "text/srt"
#define MEDIA_MIMETYPE_TEXT_SSA "text/ssa"
#define MEDIA_MIMETYPE_TEXT_ASS "text/ass"

#define MEDIA_MIMETYPE_CONTAINER_FLV "video/flv"
#define MEDIA_MIMETYPE_CONTAINER_ASF "video/x-ms-wmv"
#define MEDIA_MIMETYPE_CONTAINER_RMVB "video/rmff"
#define MEDIA_MIMETYPE_CONTAINER_DSF "audio/dsf"


#define MEDIA_MIMETYPE_VIDEO_DIVX4 "video/divx4"
#define MEDIA_MIMETYPE_VIDEO_WMV "video/x-wmv"
#define MEDIA_MIMETYPE_VIDEO_VC1 "video/x-vc1"

#define MEDIA_MIMETYPE_VIDEO_JPEG "video/jpeg"

#define MEDIA_MIMETYPE_VIDEO_REAL "video/x-pn-realvideo"
#define MEDIA_MIMETYPE_VIDEO_SORENSON "video/x-flash-video"

#define MEDIA_MIMETYPE_AUDIO_WMA "audio/x-wma"
#define MEDIA_MIMETYPE_AUDIO_ADPCM "audio/adpcm"
#define MEDIA_MIMETYPE_AUDIO_REAL "audio/x-pn-realaudio"
#define MEDIA_MIMETYPE_AUDIO_APE "audio/x-monkeys-audio"
#define MEDIA_MIMETYPE_AUDIO_AAC_NONSTANDARD  "audio/x-mp4a-latm"
#define MEDIA_MIMETYPE_AUDIO_BSAC "audio/x-bsac"
#define MEDIA_MIMETYPE_AUDIO_DSD  "audio/dsd"

#if (ANDROID_VERSION == ANDROID11)
#define MEDIA_MIMETYPE_AUDIO_MPEGH_MHA1 "audio/mha1"
#define MEDIA_MIMETYPE_AUDIO_MPEGH_MHM1 "audio/mhm1"
#define MEDIA_MIMETYPE_IMAGE_AVIF "image/avif"
#endif

#define AMEDIAFORMAT_VENDOR_KEY_BITS_PER_SAMPLE "vendor.bits-per-sample.value"

#define AMEDIAFORMAT_KEY_MPEGH_PROFILE_LEVEL_INDICATION \
  "mpegh-profile-level-indication"
#define AMEDIAFORMAT_KEY_MPEGH_REFERENCE_CHANNEL_LAYOUT \
  "mpegh-reference-channel-layout"
#define AMEDIAFORMAT_KEY_MPEGH_COMPATIBLE_SETS \
  "mpegh-compatible-sets"
#define AMEDIAFORMAT_KEY_SAMPLE_FILE_OFFSET "sample-file-offset"
#define AMEDIAFORMAT_KEY_LAST_SAMPLE_INDEX_IN_CHUNK "last-sample-index-in-chunk"
}
#endif
