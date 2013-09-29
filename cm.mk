# Inherit some common CM stuff.
$(call inherit-product, vendor/replicant/config/gsm.mk)

# Release name
PRODUCT_RELEASE_NAME := GTA04

# Boot animation
TARGET_BOOTANIMATION_NAME := square-480x480

# Inherit some common CM stuff.
$(call inherit-product, vendor/replicant/config/common_full_phone.mk)

# Inherit device configuration
$(call inherit-product, device/goldelico/gta04/full_gta04.mk)

# Inherit Software GL configuration.
$(call inherit-product, vendor/replicant/config/software_gl.mk)

PRODUCT_NAME := replicant_gta04
PRODUCT_DEVICE := gta04
PRODUCT_BRAND := Goldelico
PRODUCT_MODEL := GTA04
PRODUCT_MANUFACTURER := goldelico
