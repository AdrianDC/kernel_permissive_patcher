LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(multirom_local_path) \
    $(multirom_local_path)/lib \
    system/extras/libbootimg/include \

LOCAL_SRC_FILES:= \
    kernel_cmdline.c \

LOCAL_MODULE := kernel_cmdline
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE_CLASS := UTILITY_EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities

LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_UNSTRIPPED)
LOCAL_STATIC_LIBRARIES := libcutils libc libmultirom_static libbootimg

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_PACK_MODULE_RELOCATIONS := false

include $(BUILD_EXECUTABLE)
