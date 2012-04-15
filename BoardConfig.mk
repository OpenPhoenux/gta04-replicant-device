# Copyright (C) 2012 Paul Kocialkowski <contact@paulk.fr>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# BoardConfig.mk
#
# Product-specific compile-time definitions.
#

# CPU specs
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH_VARIANT := armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER := true
TARGET_GLOBAL_CFLAGS += -mtune=cortex-a8
TARGET_GLOBAL_CPPFLAGS += -mtune=cortex-a8

# Images specs
TARGET_NO_BOOTLOADER := false
TARGET_NO_KERNEL := false
TARGET_NO_RADIOIMAGE := true
BOARD_USES_UBOOT := true
BOARD_CUSTOM_BOOTIMG_MK := device/goldelico/gta04/bootimage.mk

# Board specs
COMMON_GLOBAL_CFLAGS += -DTARGET_OMAP3
TARGET_OMAP3 := true
TARGET_BOARD_PLATFORM := omap3
TARGET_BOOTLOADER_BOARD_NAME := gta04

# Hardware
BOARD_HAVE_FM_RADIO := false
# skip device specific audio libraries
BOARD_USES_GENERIC_AUDIO := true
USE_CAMERA_STUB := true

# Init
# uncomment the following lines for sdcard init.rc
#TARGET_PROVIDES_INIT := true
