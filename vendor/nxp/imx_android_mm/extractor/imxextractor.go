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

package imxextractor

import (
        "android/soong/android"
        "android/soong/cc"
        "github.com/google/blueprint/proptools"
)

func init() {
    android.RegisterModuleType("imxextractor_defaults", imxextractorDefaultsFactory)
}

func imxextractorDefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, imxextractorDefaults)
    return module
}

func imxextractorDefaults(ctx android.LoadHookContext) {
    var static_libs []string
    var version string = ctx.AConfig().PlatformVersionName()
    type props struct {
        Target struct {
                Android struct {
                        Enabled *bool
                        Static_libs []string
                        Cppflags []string
                }
        }
    }
    p := &props{}
    p.Target.Android.Enabled = proptools.BoolPtr(true)
    if ctx.Config().VendorConfig("IMXPLUGIN").Bool("BOARD_VPU_ONLY") {
        p.Target.Android.Enabled = proptools.BoolPtr(false)
    }
    if (version == "12" || version == "Tiramisu" || version == "13") {
        static_libs = append(static_libs,"libstagefright_foundation_colorutils_ndk")
    }
    if(version == "11") {
        p.Target.Android.Cppflags = append(p.Target.Android.Cppflags, "-DANDROID_VERSION=1100")
    }
    p.Target.Android.Static_libs = static_libs
    p.Target.Android.Cppflags = append(p.Target.Android.Cppflags, "-DANDROID11=1100")
    ctx.AppendProperties(p)
}
