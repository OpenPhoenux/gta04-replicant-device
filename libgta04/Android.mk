# Copyright (C) 2016 Lukas Märdian <lukas@goldelico.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(TARGET_DEVICE),gta04)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := gta04.c

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)

common_COPY_HEADERS_TO := libgta04
common_COPY_HEADERS := gta04.h

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libgta04
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
