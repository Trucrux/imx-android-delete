
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

// build libs

cc_defaults {
  name: "isp_defaults",
  compile_multilib: "64",

  shared_libs: [
      "liblog",
      "libcutils",
      "libutils",
  ],

  local_include_dirs: [
      "appshell/build-android/generated/release/include",
      "dewarp/hal",
      "dewarp/parser",
      "mediacontrol/buffer",
      "mediacontrol/fpga",
      "mediacontrol/include",
      "mediacontrol/include_api",
      "units/cam_device/include",
      "units/cam_device/utils",
      "utils3rd/include",
      "utils3rd/3rd/jsoncpp/include",
      "utils3rd/3rd",
      "vvcam/common",
      "vvcam/dwe",
      "vvcam/isp/",
  ],

  cflags: [
      "-DLINUX",
      "-DANDROID",
      "-DHAL_ALTERA",
      "-DISP_VERSION=ISP8000NANO_V1802",
      "-DISP8000NANO_BASE",
      "-DISP8000NANO_V1802",
      "-DISP_COMPAND",
      "-DISP_DVP_PINMAPPING",
      "-DISP_FILTER",
      "-DISP_HDR_STITCH",
      "-DISP_MIV1",
      "-DISP_MI_BUF_DUMP",
      "-DISP_MP_34BIT",
      "-DISP_MULTI_NANO",
      "-DISP_SINGLE_DOM_OVER_MP",
      "-DISP_TPG",
      "-DISP_WDR_V3",
      "-DLIVE_SENSOR",
      "-DMARVIN_HW_ALTERA",
      "-DSUPPORT_PASS_JSON",
      "-DPLATFORM=ARM64 -DAPPMODE=V4L2",
      "-DQTLESS=1 -DFULL_SRC_COMPILE=1",
      "-DWITH_DWE=1",
      "-DSERVER_LESS=1",
      "-DSUBDEV_V4L2=1",
      "-DENABLE_IRQ=1",
      "-DUSE_V4L2",
      "-DMIPI_USE_CAMERIC",
      "-DAPPMODE_V4L2",
      "-DUSE_SENSOR",
      "-DARM64",
      "-DCAM_ENGINE_DRAW_DOM_ONLY",
  ],

  cppflags: [
      "-fexceptions",
  ],

  rtti: true,
  vendor: true,
  allow_undefined_symbols: true,
}

cc_library_shared {
  name: "libdewarp_parser",
  export_include_dirs: [
        "dewarp/parser",
  ],
  local_include_dirs: [
      "dewarp/parser",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "dewarp/parser/*.cpp",
  ],
}

cc_library_shared {
  name: "libjsoncpp-1.8.4",
  export_include_dirs: [
        "utils3rd/3rd/jsoncpp/include",
  ],
  local_include_dirs: [
      "utils3rd/3rd/jsoncpp/include",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "utils3rd/3rd/jsoncpp/src/lib_json/json_reader.cpp",
    "utils3rd/3rd/jsoncpp/src/lib_json/json_writer.cpp",
    "utils3rd/3rd/jsoncpp/src/lib_json/json_value.cpp",
  ],
}

cc_library_shared {
  name: "libmedia_buffer",
  export_include_dirs: [
        "mediacontrol/buffer",
  ],
  local_include_dirs: [
      "mediacontrol/buffer",
      "mediacontrol/fpga",
      "vvcam/common",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
      "libviv_fpga_driver",
  ],

  srcs: [
    "mediacontrol/buffer/*.cpp",
  ],
}

cc_library_shared {
  name: "libviv_fpga_driver",
  export_include_dirs: [
        "mediacontrol/fpga",
  ],
  local_include_dirs: [
      "mediacontrol/fpga",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "mediacontrol/fpga/FpgaDriver.cpp",
  ],
}

cc_library_shared {
  name: "libyuv2rgb",
  export_include_dirs: [
        "appshell/yuv",
  ],
  local_include_dirs: [
      "appshell/yuv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "appshell/yuv/yuv2rgb.cpp",
  ],
}

cc_library_shared {
  name: "liboslayer",
  export_include_dirs: [
      "units/oslayer/include",
  ],
  local_include_dirs: [
      "units/oslayer/include",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/oslayer/source/*.c"
  ],
}


cc_library_shared {
  name: "libbase64",
  export_include_dirs: [
      "units/cam_device/utils/base64",
  ],
  local_include_dirs: [
      "units/cam_device/utils/base64",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/cam_device/utils/base64/*.cpp"
  ],
}

cc_library_shared {
  name: "libebase",
  export_include_dirs: [
      "units/ebase/include",
  ],
  local_include_dirs: [
      "units/ebase/include",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/ebase/source/*.c"
  ],
}

cc_library_shared {
  name: "libversion",
  export_include_dirs: [
      "units/version/include",
  ],
  local_include_dirs: [
      "units/version/include",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/version/source/*.c"
  ]
}

cc_library_shared {
  name: "libcommon",
  export_include_dirs: [
      "units/common/include",
  ],
  local_include_dirs: [
      "units/common/include",
      "units/common/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/common/source/*.c"
  ],
}

cc_library_shared {
  name: "libbufferpool",
  export_include_dirs: [
      "units/bufferpool/include",
  ],
  local_include_dirs: [
      "units/bufferpool/include",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
     "liboslayer",
  ],

  srcs: [
    "units/bufferpool/source/*.c"
  ],
}

cc_library_shared {
  name: "libfpga",
  export_include_dirs: [
      "units/fpga/fpga/include",
  ],
  local_include_dirs: [
      "units/fpga/fpga/include",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
     "liboslayer",
  ],

  srcs: [
    "units/fpga/fpga/source/*.c"
  ],
}

cc_library_shared {
  name: "libhal",
  export_include_dirs: [
      "units/hal/include",
  ],
  local_include_dirs: [
      "units/hal/include",
      "units/hal/include_priv",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
      "liboslayer",
      "libfpga",
  ],

  srcs: [
    "units/hal/source/*.c"
  ],
}

cc_library_shared {
  name: "libcameric_drv",
  export_include_dirs: [
      "units/cameric_drv/include",
  ],
  local_include_dirs: [
      "units/cameric_drv/include",
      "units/cameric_drv/include_priv",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
     "libhal",
  ],

  srcs: [
    "units/cameric_drv/source/*.c",
  ],
}

cc_library_shared {
  name: "libcameric_reg_drv",
  export_include_dirs: [
      "units/cameric_reg_drv/include",
  ],
  local_include_dirs: [
      "units/cameric_reg_drv/include",
      "units/cameric_reg_drv/include_priv",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
     "libhal",
  ],

  srcs: [
    "units/cameric_reg_drv/source/*.c",
  ],
}

cc_library_shared {
  name: "liba2dnr",
  export_include_dirs: [
      "units/aaa/a2dnr/include",
  ],
  local_include_dirs: [
      "units/aaa/a2dnr/include",
      "units/aaa/a2dnr/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/a2dnr/source/*.c"
  ],
}

cc_library_shared {
  name: "liba3dnr",
  export_include_dirs: [
      "units/aaa/a3dnr/include",
  ],
  local_include_dirs: [
      "units/aaa/a3dnr/include",
      "units/aaa/a3dnr/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/a3dnr/source/*.c"
  ],
}

cc_library_shared {
  name: "libadpcc",
  export_include_dirs: [
      "units/aaa/adpcc/include",
  ],
  local_include_dirs: [
      "units/aaa/adpcc/include",
      "units/aaa/adpcc/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/adpcc/source/*.c"
  ],
}

cc_library_shared {
  name: "libadpf",
  export_include_dirs: [
      "units/aaa/adpf/include",
  ],
  local_include_dirs: [
      "units/aaa/adpf/include",
      "units/aaa/adpf/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/adpf/source/*.c"
  ],
}

cc_library_shared {
  name: "libaec",
  export_include_dirs: [
      "units/aaa/aec/include",
  ],
  local_include_dirs: [
      "units/aaa/aec/include",
      "units/aaa/aec/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/aec/source/*.c"
  ],
}

cc_library_shared {
  name: "libaee",
  export_include_dirs: [
      "units/aaa/aee/include",
  ],
  local_include_dirs: [
      "units/aaa/aee/include",
      "units/aaa/aee/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/aee/source/*.c"
  ],
}

cc_library_shared {
  name: "libaflt",
  export_include_dirs: [
      "units/aaa/aflt/include",
  ],
  local_include_dirs: [
      "units/aaa/aflt/include",
      "units/aaa/aflt/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/aflt/source/*.c"
  ],
}

cc_library_shared {
  name: "libaf",
  export_include_dirs: [
      "units/aaa/af/include",
  ],
  local_include_dirs: [
      "units/aaa/af/include",
      "units/aaa/af/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/af/source/*.c"
  ],
}

cc_library_shared {
  name: "libahdr",
  export_include_dirs: [
      "units/aaa/ahdr/include",
  ],
  local_include_dirs: [
      "units/aaa/ahdr/include",
      "units/aaa/ahdr/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/ahdr/source/*.c"
  ],
}

cc_library_shared {
  name: "libavs",
  export_include_dirs: [
      "units/aaa/avs/include",
  ],
  local_include_dirs: [
      "units/aaa/avs/include",
      "units/aaa/avs/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/avs/source/*.c"
  ],
}

cc_library_shared {
  name: "libawb",
  export_include_dirs: [
      "units/aaa/awb/include",
  ],
  local_include_dirs: [
      "units/aaa/awb/include",
      "units/aaa/awb/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/awb/source/*.c"
  ],
}

cc_library_shared {
  name: "libawdr3",
  export_include_dirs: [
      "units/aaa/awdr3/include",
  ],
  local_include_dirs: [
      "units/aaa/awdr3/include",
      "units/aaa/awdr3/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/aaa/awdr3/source/*.c"
  ],
}

cc_library_shared {
  name: "libbufsync_ctrl",
  export_include_dirs: [
      "units/bufsync_ctrl/include",
  ],
  local_include_dirs: [
      "units/bufsync_ctrl/include",
      "units/bufsync_ctrl/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/bufsync_ctrl/source/*.c"
  ],
}

cc_library_shared {
  name: "libcam_calibdb",
  export_include_dirs: [
      "units/cam_calibdb/include",
  ],
  local_include_dirs: [
      "units/cam_calibdb/include",
      "units/cam_calibdb/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/cam_calibdb/source/*.c"
  ],
}

cc_library_shared {
  name: "libcam_device",
  export_include_dirs: [
      "units/cam_device/include",
  ],
  local_include_dirs: [
      "units/cam_device/include",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/cam_device/source/calibdb/*.cpp",
    "units/cam_device/source/calibration/*.cpp",
    "units/cam_device/source/camera/*.cpp",
    "units/cam_device/source/common/*.cpp",
    "units/cam_device/source/commitf/*.cpp"
  ],
}

cc_library_shared {
  name: "libcam_engine",
  export_include_dirs: [
      "units/cam_engine/include",
  ],
  local_include_dirs: [
      "units/cam_engine/include",
      "units/cam_engine/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/cam_engine/source/*.c"
  ],
}

cc_library_shared {
  name: "libdewarp_hal",
  export_include_dirs: [
      "dewarp/hal/",
  ],
  local_include_dirs: [
      "dewarp/hal/",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
      "libviv_fpga_driver",
  ],

  srcs: [
    "dewarp/hal/*.cpp"
  ],
}

cc_library_shared {
  name: "libi2c_drv",
  export_include_dirs: [
      "units/i2c_drv/include",
  ],
  local_include_dirs: [
      "units/i2c_drv/include",
      "units/i2c_drv/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/i2c_drv/source/*.c"
  ],
}

cc_library_shared {
  name: "libisi",
  export_include_dirs: [
      "units/isi/include",
  ],
  local_include_dirs: [
      "units/isi/include",
      "units/isi/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/isi/source/*.c"
  ],
}

cc_library_shared {
  name: "libmedia_server",
  export_include_dirs: [
      "mediacontrol/server",
  ],
  local_include_dirs: [
      "mediacontrol/server",
      "mediacontrol/include",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
      "libdewarp_hal",
      "libmedia_buffer",
      "libviv_fpga_driver",
  ],

  srcs: [
    "mediacontrol/server/*.cpp"
  ],
}

cc_library_shared {
  name: "libmim_ctrl",
  export_include_dirs: [
      "units/mim_ctrl/include",
  ],
  local_include_dirs: [
      "units/mim_ctrl/include",
      "units/mim_ctrl/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/mim_ctrl/source/*.c"
  ],
}

cc_library_shared {
  name: "libmipi_drv",
  export_include_dirs: [
      "units/mipi_drv/include",
  ],
  local_include_dirs: [
      "units/mipi_drv/include",
      "units/mipi_drv/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/mipi_drv/source/*.c"
  ],
}

cc_library_shared {
  name: "libmom_ctrl",
  export_include_dirs: [
      "units/mom_ctrl/include",
  ],
  local_include_dirs: [
      "units/mom_ctrl/include",
      "units/mom_ctrl/include_priv",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "units/mom_ctrl/source/*.c"
  ],
}

cc_library_shared {
  name: "libtinyxml2-6.2.0",
  export_include_dirs: [
      "utils3rd/3rd/tinyxml2",
  ],
  local_include_dirs: [
      "utils3rd/3rd/tinyxml2",
  ],
  defaults: ["isp_defaults"],

  srcs: [
    "utils3rd/3rd/tinyxml2/tinyxml2.cpp"
  ],
}

// build binaries

cc_defaults {
    name: "isp_binary_default",
    compile_multilib: "64",

    local_include_dirs: [
        "appshell/hal/include",
        "dewarp/hal",
        "dewarp/parser",
        "mediacontrol/include",
        "mediacontrol/include_api",
        "mediacontrol/buffer",
        "mediacontrol/fpga",
        "mediacontrol/server",
        "utils3rd/3rd/jsoncpp/include",
        "utils3rd/include",
        "units/cam_device/include",
        "vvcam/common",
        "vvcam/dwe",
    ],


    shared_libs: [
        "libi2c_drv",
        "libcam_device",
        "libcam_engine",
        "libcameric_drv",
        "libhal",
        "libbase64",
        "libtinyxml2-6.2.0",
        "libjsoncpp-1.8.4",
        "libbufferpool",
        "libcameric_reg_drv",
        "libisi",
        "libmedia_server",
        "libmipi_drv",
        "libcam_calibdb",
        "libfpga",
        "libebase",
        "liboslayer",
        "libcommon",
        "libaec",
        "libawb",
        "libaf",
        "libadpf",
        "libadpcc",
        "liba2dnr",
        "liba3dnr",
        "libawdr3",
        "libahdr",
        "libaee",
        "libaflt",
        "libavs",
        "libmim_ctrl",
        "libmom_ctrl",
        "libbufsync_ctrl",
        "libdewarp_parser",
        "libmedia_buffer",
        "liblog",
        "libcutils",
        "libutils",
  ],

  cppflags: [
      "-fexceptions",
  ],

  rtti: true,
  vendor: true,
}


cc_binary {
    name: "isp_media_server",
    defaults: ["isp_binary_default"],

    local_include_dirs: [
        "mediacontrol/daemon",
    ],

    srcs: [
        "mediacontrol/daemon/main_v4l2.cpp",
        "mediacontrol/daemon/V4l2Event.cpp",
    ],
}

cc_binary {
    name: "vvext",
    defaults: ["isp_binary_default"],

    srcs: [
        "appshell/vvext/vvext.cpp",
    ],
}

cc_library_shared {
  name: "libvvdisplay_shared",
  local_include_dirs: [
      "appshell/display",
  ],
  defaults: ["isp_defaults"],

  shared_libs: [
      "libyuv2rgb",
  ],

  srcs: [
    "appshell/display/*.cpp"
  ],
}

cc_binary {
    name: "video_test",
    defaults: ["isp_binary_default"],

    local_include_dirs: [
        "appshell/display",
    ],

    shared_libs: [
        "libvvdisplay_shared",
    ],

    srcs: [
        "appshell/v4l_drm_test/video_test.cpp",
    ],
}

bootstrap_go_package {
    name: "soong-isp",
    pkgPath: "android/soong/vendor/nxp-private/verisilicon_sw_isp",
    deps: [
        "blueprint",
        "blueprint-pathtools",
        "soong",
        "soong-android",
        "soong-cc",
        "soong-genrule",
    ],
    srcs: [
        "isp.go",
    ],
    pluginFor: ["soong_build"],
}
