LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(BUILD_MULTI_PREBUILT)

FSL_CODEC_OUT_PATH := $(TARGET_OUT_VENDOR)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_mp3_dec_v2_arm12_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := ./lib64/lib_mp3_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := ./lib/lib_mp3_dec_v2_arm12_elinux.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_mp3_dec_v2_arm12_elinux.so
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          if [ -d lib ]; then \
                              ln -sf ./lib_mp3_dec_v2_arm12_elinux.so lib/lib_mp3_dec.so; \
                          fi; \
                          if [ -d lib64 ]; then \
                              ln -sf ./lib_mp3_dec_v2_arm12_elinux.so lib64/lib_mp3_dec.so; \
                          fi;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_mp3_dec_v2_arm12_elinux.so lib/lib_mp3_dec.so;
endif

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_aac_dec_v2_arm12_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := ./lib64/lib_aac_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := ./lib/lib_aac_dec_v2_arm12_elinux.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_aac_dec_v2_arm12_elinux.so
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          if [ -d lib ]; then \
                              ln -sf ./lib_aac_dec_v2_arm12_elinux.so lib/lib_aac_dec.so; \
                          fi; \
                          if [ -d lib64 ]; then \
                              ln -sf ./lib_aac_dec_v2_arm12_elinux.so lib64/lib_aac_dec.so; \
                          fi;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_aac_dec_v2_arm12_elinux.so lib/lib_aac_dec.so; 
endif

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_wb_amr_dec_arm9_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := lib64/lib_wb_amr_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/lib_wb_amr_dec_arm9_elinux.so
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          if [ -d lib ]; then \
                              ln -sf ./lib_wb_amr_dec_arm9_elinux.so lib/lib_wb_amr_dec.so; \
                          fi; \
                          if [ -d lib64 ]; then \
                              ln -sf ./lib_wb_amr_dec_arm9_elinux.so lib64/lib_wb_amr_dec.so; \
                          fi;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_wb_amr_dec_arm9_elinux.so lib/lib_wb_amr_dec.so;
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_wb_amr_dec_arm9_elinux.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_flac_dec_v2_arm11_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := lib64/lib_flac_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/lib_flac_dec_v2_arm11_elinux.so
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          if [ -d lib ]; then \
                              ln -sf ./lib_flac_dec_v2_arm11_elinux.so lib/lib_flac_dec.so; \
                          fi; \
                          if [ -d lib64 ]; then \
                              ln -sf ./lib_flac_dec_v2_arm11_elinux.so lib64/lib_flac_dec.so; \
                          fi;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_flac_dec_v2_arm11_elinux.so lib/lib_flac_dec.so;
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_flac_dec_v2_arm11_elinux.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libvpu-malone
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := lib64/libvpu-malone.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/libvpu-malone.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/libvpu-malone.so
endif
include $(BUILD_PREBUILT)


