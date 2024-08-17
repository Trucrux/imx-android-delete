// Copyright 2019 NXP
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

package imx_filter

import (
        "android/soong/android"
        "android/soong/cc"
        "strings"
        "github.com/google/blueprint/proptools"
)

func init() {
    android.RegisterModuleType("libc2filterplugin_defaults", libc2filterpluginDefaultsFactory)
}

func libc2filterpluginDefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, libc2filterpluginDefaults)
    return module
}

func libc2filterpluginDefaults(ctx android.LoadHookContext) {
    type props struct {
        Target struct {
                Android struct {
                        Enabled *bool
						Shared_libs []string
						Cppflags []string
                }
        }
    }
    p := &props{}
    var board string = ctx.Config().VendorConfig("IMXPLUGIN").String("BOARD_SOC_TYPE")
    if strings.Contains(board, "IMX8Q") {
        p.Target.Android.Enabled = proptools.BoolPtr(true)
        p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "lib_imx_c2_g2d_filter")
        p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "lib_imx_c2_isi_filter")
        p.Target.Android.Cppflags = append(p.Target.Android.Cppflags, "-DIMX8Q")
    }else if strings.Contains(board, "IMX8MM") {
        p.Target.Android.Enabled = proptools.BoolPtr(true)
        p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "lib_imx_c2_g2d_pre_filter")
        p.Target.Android.Cppflags = append(p.Target.Android.Cppflags, "-DIMX8MM")
    } else {
        p.Target.Android.Enabled = proptools.BoolPtr(false)
    }
    ctx.AppendProperties(p)
}
