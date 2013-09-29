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

# Overlay
DEVICE_PACKAGE_OVERLAYS := device/goldelico/gta04/overlay

# Install script
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/replicant_gta04_install.sh:replicant_gta04_install.sh

# Ramdisk
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/init.gta04.rc:root/init.gta04.rc \
	device/goldelico/gta04/init.gta04.usb.rc:root/init.gta04.usb.rc \
	device/goldelico/gta04/ueventd.gta04.rc:root/ueventd.gta04.rc

# Don't install kernel boot splash (initlogo.rle), as it is not universal for all Display sizes (Letux 2804/3704/7004)
#PRODUCT_COPY_FILES += \
#	device/goldelico/gta04/initlogo.rle:root/initlogo.rle

# Battery
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/battery-init.sh:system/battery-init.sh

# USB
PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mass_storage,adb

# Input
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/tsc2007.idc:system/usr/idc/tsc2007.idc \
	device/goldelico/gta04/twl4030_pwrbutton.kl:system/usr/keylayout/twl4030_pwrbutton.kl \
	device/goldelico/gta04/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl

# Graphics
PRODUCT_LOCALES := hdpi

# Audio
PRODUCT_PACKAGES += \
	audio.primary.omap3

PRODUCT_COPY_FILES += \
	device/goldelico/gta04/configs/tinyalsa-audio.xml:system/etc/tinyalsa-audio.xml

# Lights
PRODUCT_PACKAGES += \
	lights.gta04 \

# APNS
PRODUCT_COPY_FILES += \
	vendor/replicant/prebuilt/common/etc/apns-conf.xml:system/etc/apns-conf.xml

# Dalvik
PRODUCT_PROPERTY_OVERRIDES += \
	dalvik.vm.lockprof.threshold=500 \
	dalvik.vm.dexopt-data-only=1 \
	dalvik.vm.checkjni=false \
	ro.kernel.android.checkjni=0

PRODUCT_TAGS += dalvik.gc.type-precise

include frameworks/base/build/phone-hdpi-512-dalvik-heap.mk
