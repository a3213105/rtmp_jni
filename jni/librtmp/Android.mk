LOCAL_PATH := $(call my-dir)
$(warning TARGET_ARCH_ABI=$(TARGET_ARCH_ABI))
include $(CLEAR_VARS)
LOCAL_MODULE := librtmp
LOCAL_SRC_FILES := $(MY_APP_OUTPUT_PATH)/lib/librtmp.so
include $(PREBUILT_SHARED_LIBRARY)
