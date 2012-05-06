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

# Hardware
PRODUCT_COPY_FILES := \
	device/goldelico/gta04/init.gta04.rc:root/init.gta04.rc \
	device/goldelico/gta04/vold.fstab:system/etc/vold.fstab \
	device/goldelico/gta04/twl4030_pwrbutton.kl:system/usr/keylayout/twl4030_pwrbutton.kl \
	device/goldelico/gta04/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl

# Audio
PRODUCT_PACKAGES += \
	libaudio \
	libaudiopolicy

PRODUCT_COPY_FILES += \
	device/goldelico/gta04/tinyalsa-audio.xml:system/etc/tinyalsa-audio.xml

# APNS
PRODUCT_COPY_FILES += \
	vendor/replicant/prebuilt/common/etc/apns-conf.xml:system/etc/apns-conf.xml

# Dalvik
PRODUCT_PROPERTY_OVERRIDES += \
	dalvik.vm.heapsize=32m \
	dalvik.vm.checkjni=false \
	ro.kernel.android.checkjni=0

PRODUCT_TAGS += dalvik.gc.type-precise

# Graphics
PRODUCT_LOCALES := hdpi
PRODUCT_PACKAGES += \
	gralloc.omap3

# Overlay
DEVICE_PACKAGE_OVERLAYS := device/goldelico/gta04/overlay
