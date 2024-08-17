ifeq ($(PREBUILT_FSL_IMX_ISP),true)
LOCAL_PATH := $(my-dir)

#### libs
include $(CLEAR_VARS)
LOCAL_MODULE := DAA3840_30MC_1080P
include $(LOCAL_PATH)/lib_common.mk
LOCAL_MODULE_SUFFIX :=.drv
LOCAL_SRC_FILES := lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := liba2dnr
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := liba3dnr
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libadpcc
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libadpf
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libaec
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libaee
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libaflt
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libaf
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libahdr
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libappshell_ebase
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libappshell_hal
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libappshell_ibd
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libappshell_oslayer
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libavs
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libawb
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libawdr3
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libbase64
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libbufferpool
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libbufsync_ctrl
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcam_calibdb
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcam_device
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcam_engine
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcameric_drv
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcameric_reg_drv
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcim_ctrl
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libcommon
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libdaA3840_30mc
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libdewarp_hal
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libebase
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libfpga
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libhal
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libi2c_drv
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libibd
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libisi
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libmedia_server
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libmim_ctrl
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libmipi_drv
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libmom_ctrl
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libos08a20
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := liboslayer
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libov2775
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libsom_ctrl
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libversion
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libvom_ctrl
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libvvdisplay_shared
include $(LOCAL_PATH)/lib_common.mk
include $(BUILD_PREBUILT)

#### executables
include $(CLEAR_VARS)
LOCAL_MODULE := isp_media_server
include $(LOCAL_PATH)/exe_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := vvext
include $(LOCAL_PATH)/exe_common.mk
include $(BUILD_PREBUILT)

#### configs for basler
include $(CLEAR_VARS)
LOCAL_MODULE := DAA3840_30MC_1080P-linear.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-basler.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := DAA3840_30MC_1080P-hdr.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-basler.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := DAA3840_30MC_4K-linear.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-basler.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := DAA3840_30MC_4K-hdr.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-basler.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := basler-Sensor0_Entry.cfg
LOCAL_MODULE_STEM := Sensor0_Entry.cfg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/vendor_overlay_sensor/basler/vendor/etc/configs/isp/
LOCAL_SRC_FILES := config_basler/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := basler-Sensor1_Entry.cfg
LOCAL_MODULE_STEM := Sensor1_Entry.cfg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/vendor_overlay_sensor/basler/vendor/etc/configs/isp/
LOCAL_SRC_FILES := config_basler/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := daA3840_30mc_1080P.json
SUB_DIR := dewarp_config
include $(LOCAL_PATH)/config_common-basler.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := daA3840_30mc_4K.json
SUB_DIR := dewarp_config
include $(LOCAL_PATH)/config_common-basler.mk
include $(BUILD_PREBUILT)


#### configs for os08a20
include $(CLEAR_VARS)
LOCAL_MODULE := OS08a20_8M_10_1080p_linear.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-os08a20.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := OS08a20_8M_10_1080p_hdr.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-os08a20.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := OS08a20_8M_10_4k_linear.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-os08a20.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := OS08a20_8M_10_4k_hdr.xml
SUB_DIR :=
include $(LOCAL_PATH)/config_common-os08a20.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := os08a20-Sensor0_Entry.cfg
LOCAL_MODULE_STEM := Sensor0_Entry.cfg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/vendor_overlay_sensor/os08a20/vendor/etc/configs/isp/
LOCAL_SRC_FILES := config_os08a20/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := os08a20-Sensor1_Entry.cfg
LOCAL_MODULE_STEM := Sensor1_Entry.cfg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/vendor_overlay_sensor/os08a20/vendor/etc/configs/isp/
LOCAL_SRC_FILES := config_os08a20/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sensor_dwe_os08a20_1080P_config.json
SUB_DIR := dewarp_config
include $(LOCAL_PATH)/config_common-os08a20.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sensor_dwe_os08a20_4K_config.json
SUB_DIR := dewarp_config
include $(LOCAL_PATH)/config_common-os08a20.mk
include $(BUILD_PREBUILT)

#### make sure /vendor/etc/configs/isp/ is created
include $(CLEAR_VARS)
LOCAL_MODULE := hollow
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/etc/configs/isp/
LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

endif
