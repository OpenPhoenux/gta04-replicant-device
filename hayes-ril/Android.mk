# This file is part of Hayes-RIL.
#
# Copyright (C) 2012-2013 Paul Kocialkowski <contact@paulk.fr>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

hayes_ril_files := \
	hayes-ril.c \
	device.c \
	at.c \
	call.c \
	network.c \
	gprs.c \
	power.c \
	sim.c \
	sms.c \
	util.c

ifeq ($(TARGET_DEVICE),gta04)
	hayes_ril_device_files := device/gta04/gta04.c
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/device/gta04/
endif

ifeq ($(TARGET_DEVICE),passion)
	hayes_ril_device_files := device/passion/passion.c
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/device/passion/
endif

ifeq ($(TARGET_DEVICE),dream_sapphire)
	hayes_ril_device_files := device/dream_sapphire/dream_sapphire.c
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/device/dream_sapphire/
endif

ifneq ($(hayes_ril_device_files),)
	LOCAL_SRC_FILES := $(hayes_ril_files) $(hayes_ril_device_files)
	LOCAL_SHARED_LIBRARIES += libcutils libnetutils libutils libril libgta04
	LOCAL_PRELINK_MODULE := false

	LOCAL_C_INCLUDES += $(KERNEL_HEADERS) $(LOCAL_PATH)
	LOCAL_LDLIBS += -lpthread
	LOCAL_CFLAGS += -DRIL_SHLIB -D_GNU_SOURCE

	LOCAL_MODULE_TAGS := optional
	LOCAL_MODULE := libhayes-ril

	include $(BUILD_SHARED_LIBRARY)
endif
