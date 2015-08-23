LOCAL_PATH:= $(call my-dir)

ifeq ($(TARGET_DEVICE),gta04)

include $(CLEAR_VARS)

LOCAL_MODULE := libaudio-ril-interface
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := audio-ril-interface.cpp

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libtinyalsa \
	libaudioutils

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	hardware/tinyalsa-audio/include/ \
	system/media/audio_utils/include

LOCAL_LDLIBS += -lpthread

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

endif
