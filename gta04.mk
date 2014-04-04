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

# System Root
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/init.rc:root/init.rc \
	device/goldelico/gta04/ueventd.rc:root/ueventd.rc \
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

PRODUCT_COPY_FILES += \
	device/goldelico/gta04/keys/TSC2007_Touchscreen.idc:system/usr/idc/TSC2007_Touchscreen.idc \
	device/goldelico/gta04/keys/twl4030_pwrbutton.kl:system/usr/keylayout/twl4030_pwrbutton.kl \
	device/goldelico/gta04/keys/Phone_button.kl:system/usr/keylayout/Phone_button.kl \
	device/goldelico/gta04/keys/gta04_Headset_Jack.kl:system/usr/keylayout/gta04_Headset_Jack.kl \
	device/goldelico/gta04/keys/3G_Wakeup.kl:system/usr/keylayout/3G_Wakeup.kl

# Bluetooth configuration files
PRODUCT_COPY_FILES += \
	system/bluetooth/data/main.le.conf:system/etc/bluetooth/main.conf \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml

# Wifi
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml

PRODUCT_COPY_FILES += \
    device/goldelico/gta04/configs/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf

# FIXME: Do we want to deploy nonfree firmware?
PRODUCT_COPY_FILES += \
    device/goldelico/gta04/nonfree/sd8686_v8.bin:system/etc/firmware/sd8686.bin \
    device/goldelico/gta04/nonfree/sd8686_v8_helper.bin:system/etc/firmware/sd8686_helper.bin

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

# Sensors
PRODUCT_PACKAGES += \
	sensors.gta04 \

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

include frameworks/native/build/phone-hdpi-512-dalvik-heap.mk
