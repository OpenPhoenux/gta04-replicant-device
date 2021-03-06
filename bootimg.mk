LOCAL_PATH := $(call my-dir)

# Make it possible to build the kernel with a locally provided toolchain
# (LOCAL_TOOLCHAIN), as the Android toolchain isn't able to build newer
# kernels (e.g. 3.12).
# Behave normally if LOCAL_TOOLCHAIN= isn't given.
#
# This is not needed anymore on Replicant 4.2
ifneq ($(LOCAL_TOOLCHAIN),)
    override ARM_CROSS_COMPILE := CROSS_COMPILE="$(LOCAL_TOOLCHAIN)"
endif

INSTALLED_XLOADER_MODULE := $(PRODUCT_OUT)/xloader

INSTALLED_BOOTLOADERIMAGE_TARGET := $(PRODUCT_OUT)/u-boot.bin
INSTALLED_XLOADERIMAGE_TARGET := $(PRODUCT_OUT)/MLO
INSTALLED_BOOTLOADER_SCRIPT_TARGET := $(PRODUCT_OUT)/boot.scr
INSTALLED_BOOTLOADER_SPLASH_TARGET := $(PRODUCT_OUT)/splash.rgb16z

$(INSTALLED_BOOTIMAGE_TARGET): $(MKIMAGE) $(INSTALLED_RAMDISK_TARGET) $(INSTALLED_KERNEL_TARGET)
	$(call pretty,"Boot image: $@")
	$(ACP) $(INSTALLED_KERNEL_TARGET) $@
	@echo -e ${CL_INS}"Made boot image: $@"${CL_RST}

$(INSTALLED_BOOTLOADERIMAGE_TARGET): $(INSTALLED_XLOADER_MODULE) $(INSTALLED_BOOTLOADER_MODULE) $(MKIMAGE)
	$(call pretty,"X-Loader image: $(INSTALLED_XLOADER_MODULE)")
	$(ACP) $(INSTALLED_XLOADER_MODULE) $(INSTALLED_XLOADERIMAGE_TARGET)
	@echo -e ${CL_INS}"Made X-Loader image: $(INSTALLED_XLOADERIMAGE_TARGET)"${CL_RST}
	$(call pretty,"Bootloader splash: $(INSTALLED_BOOTLOADER_SPLASH_TARGET)")
	$(ACP) $(BOOTLOADER_SRC)/../boot-scr/splash.rgb16z $(INSTALLED_BOOTLOADER_SPLASH_TARGET)
	@echo -e ${CL_INS}"Made bootloader splash: $(INSTALLED_BOOTLOADER_SPLASH_TARGET)"${CL_RST}
	$(call pretty,"Bootloader script: $(INSTALLED_BOOTLOADER_SCRIPT_TARGET)")
	$(MKIMAGE) -A arm -T script -C none -n "boot.scr" -d $(BOOTLOADER_SRC)/../boot-scr/boot.txt $(INSTALLED_BOOTLOADER_SCRIPT_TARGET)
	@echo -e ${CL_INS}"Made bootloader script: $(INSTALLED_BOOTLOADER_SCRIPT_TARGET)"${CL_RST}
	$(call pretty,"Bootloader image: $@")
	$(ACP) $(INSTALLED_BOOTLOADER_MODULE) $@
	@echo -e ${CL_INS}"Made bootloader image: $@"${CL_RST}
