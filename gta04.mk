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
	device/goldelico/gta04/rootdir/init.rc:root/init.rc \
	device/goldelico/gta04/rootdir/ueventd.rc:root/ueventd.rc \
	device/goldelico/gta04/rootdir/init.gta04.rc:root/init.gta04.rc \
	device/goldelico/gta04/rootdir/init.gta04.usb.rc:root/init.gta04.usb.rc \
	device/goldelico/gta04/rootdir/ueventd.gta04.rc:root/ueventd.gta04.rc

# Don't install kernel boot splash (initlogo.rle), as it is not universal for all Display sizes (Letux 2804/3704/7004)
#PRODUCT_COPY_FILES += \
#	device/goldelico/gta04/rootdir/initlogo.rle:root/initlogo.rle

# Battery
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/rootdir/battery-init.sh:system/battery-init.sh

# USB
PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mtp

PRODUCT_COPY_FILES += \
	device/goldelico/gta04/usr/idc/TSC2007_Touchscreen.idc:system/usr/idc/TSC2007_Touchscreen.idc \
	device/goldelico/gta04/usr/keylayout/twl4030_pwrbutton.kl:system/usr/keylayout/twl4030_pwrbutton.kl \
	device/goldelico/gta04/usr/keylayout/Phone_button.kl:system/usr/keylayout/Phone_button.kl \
	device/goldelico/gta04/usr/keylayout/gta04_Headset_Jack.kl:system/usr/keylayout/gta04_Headset_Jack.kl \
	device/goldelico/gta04/usr/keylayout/3G_Wakeup.kl:system/usr/keylayout/3G_Wakeup.kl

# Bluetooth configuration files
PRODUCT_COPY_FILES += \
	system/bluetooth/data/main.le.conf:system/etc/bluetooth/main.conf \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml

# Wi-Fi
PRODUCT_COPY_FILES += \
	device/goldelico/gta04/configs/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf

PRODUCT_PROPERTY_OVERRIDES += \
	wifi.interface=wlan0

# FIXME: Do we want to deploy nonfree firmware?
PRODUCT_COPY_FILES += \
    device/goldelico/gta04/nonfree/sd8686_v8.bin:system/vendor/firmware/sd8686.bin \
    device/goldelico/gta04/nonfree/sd8686_v8_helper.bin:system/vendor/firmware/sd8686_helper.bin

# Graphics
PRODUCT_LOCALES := hdpi

# Audio
PRODUCT_PACKAGES += \
	audio.primary.omap3

PRODUCT_COPY_FILES += \
	device/goldelico/gta04/configs/tinyalsa-audio.xml:system/etc/tinyalsa-audio.xml \
	device/goldelico/gta04/configs/audio_policy.conf:system/etc/audio_policy.conf

# RIL
PRODUCT_PACKAGES += \
	libhayes-ril

# Lights
PRODUCT_PACKAGES += \
	lights.gta04

# Sensors
PRODUCT_PACKAGES += \
	sensors.gta04

# GPS
PRODUCT_PACKAGES += \
	gps.gta04

# Permissions
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.touchscreen.xml:system/etc/permissions/android.hardware.touchscreen.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/native/data/etc/android.hardware.location.xml:system/etc/permissions/android.hardware.location.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	frameworks/native/data/etc/android.software.sip.xml:system/etc/permissions/android.software.sip.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.barometer.xml:system/etc/permissions/android.hardware.barometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.compass.xml \
	frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.gyroscope.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.light.xml

	#Light sensor TEPT4400 is only installed on Letux 3704

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
