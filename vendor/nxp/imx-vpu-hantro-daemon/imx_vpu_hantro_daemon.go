// Copyright 2021 NXP
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package imx_vpu_hantro_daemon

import (
        "android/soong/android"
        "android/soong/cc"
        "strings"
        "github.com/google/blueprint/proptools"
)

func init() {
    android.RegisterModuleType("IMX_VPU_HANTRO_DAEMON_DEFAULTS", IMX_VPU_HANTRO_DAEMON_DefaultsFactory)
}

func IMX_VPU_HANTRO_DAEMON_DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, Hantro_Daemon_Defaults)
    return module
}

func Hantro_Daemon_Defaults(ctx android.LoadHookContext) {
    var Cflags []string
    var Srcs []string
    var Shared_libs []string
    var Include_dirs []string
    type props struct {
        Target struct {
                Android struct {
                        Enabled *bool
                        Cflags []string
                        Srcs []string
                        Shared_libs []string
                        Include_dirs []string
                }
        }
    }
    p := &props{}
    var vpu_type string = ctx.Config().VendorConfig("IMXPLUGIN").String("BOARD_VPU_TYPE")
    if strings.Contains(vpu_type, "hantro") {
        p.Target.Android.Enabled = proptools.BoolPtr(true)
    } else {
        p.Target.Android.Enabled = proptools.BoolPtr(false)
    }

    var board string = ctx.Config().VendorConfig("IMXPLUGIN").String("BOARD_SOC_TYPE")
    if strings.Contains(board, "IMX8MP") {
        Cflags = append(Cflags, "-DNXP")
        Cflags = append(Cflags, "-DHAS_VSI_ENC")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_hevc.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_h264.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vp9.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vp8.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_enc_img_h2.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_enc.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_enc_video_h2.c")
        Shared_libs = append(Shared_libs, "libhantro_vc8000e")
        Include_dirs = append(Include_dirs, "vendor/nxp/fsl-codec/ghdr/hantro_VC8000E_enc")
    } else if strings.Contains(board, "IMX8MM") {
        Cflags = append(Cflags, "-DUSE_H1")
        Cflags = append(Cflags, "-DNXP")
        Cflags = append(Cflags, "-DHAS_VSI_ENC")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_hevc.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_h264.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vp8.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vp9.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_enc_img_h1.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_enc.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_enc_video_h1.c")
        Shared_libs = append(Shared_libs, "libhantro_h1")
        Include_dirs = append(Include_dirs, "vendor/nxp/imx-vpu-hantro/h1_encoder/software/inc")
    } else if strings.Contains(board, "IMX8MQ") {
        Cflags = append(Cflags, "-DHAS_FULL_DECFMT")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_hevc.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_h264.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_mpeg2.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vp8.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vp9.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_mpeg4.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_vc1.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_jpeg.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_rv.c")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_dec_avs.c")
    }

    // enable/disable nxp ts manager, here disable it by default
    if false {
        Cflags = append(Cflags, "-DNXP_TIMESTAMP_MANAGER")
        Srcs = append(Srcs, "v4l2_vsi_daemon/src/dec_ts.c")
    }

    //android debug log in adb
    if false {
        Cflags = append(Cflags, "-DANDROID_DEBUG")
	    Shared_libs = append(Shared_libs, "liblog")
	    Shared_libs = append(Shared_libs, "libcutils")
	    Include_dirs = append(Include_dirs, "system/core/include")
	    Srcs = append(Srcs, "v4l2_vsi_daemon/src/vsi_daemon_debug.c")
    }

    p.Target.Android.Cflags = Cflags
    p.Target.Android.Srcs = Srcs
    p.Target.Android.Shared_libs = Shared_libs
    p.Target.Android.Include_dirs = Include_dirs
    ctx.AppendProperties(p)
}



