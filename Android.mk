LOCAL_PATH := $(call my-dir)
multirom_local_path := $(LOCAL_PATH)

# Kernel CmdLine
include $(multirom_local_path)/kernel_cmdline/Android.mk

# libmultirom
include $(multirom_local_path)/lib/Android.mk

