LOCAL_PATH := $(call my-dir)

INSTALLED_BOOTIMAGE_TARGET := $(PRODUCT_OUT)/boot.img
INSTALLED_RECOVERYIMAGE_TARGET := $(PRODUCT_OUT)/recovery.img
INSTALLED_BOOTLOADERIMAGE_TARGET := $(PRODUCT_OUT)/bootloader.img
INSTALLED_BOOTSCR_TARGET := $(PRODUCT_OUT)/boot.scr
GTA04_BOOTSCR_SOURCE := device/goldelico/gta04/u-boot/boot-scr/replicant-boot.txt

.NOTPARALLEL: $(INSTALLED_BOOTIMAGE_TARGET)
.NOTPARALLEL: $(INSTALLED_RECOVERYIMAGE_TARGET)

$(INSTALLED_BOOTIMAGE_TARGET): $(INSTALLED_RAMDISK_TARGET) $(INSTALLED_KERNEL_TARGET)
	$(call pretty,"Boot image: $@")
	$(ACP) $(PRODUCT_OUT)/kernel $@

$(INSTALLED_RECOVERYIMAGE_TARGET): $(recovery_ramdisk) $(recovery_kernel) $(PRODUCT_OUT)/utilities/busybox $(PRODUCT_OUT)/utilities/mke2fs
	$(call pretty,"Recovery image: $@")
	$(ACP) $(PRODUCT_OUT)/kernel-recovery $@

$(INSTALLED_BOOTLOADERIMAGE_TARGET): $(INSTALLED_BOOTLOADER_TARGET) $(MKIMAGE)
	$(hide) $(ACP) -fp $(INSTALLED_BOOTLOADER_TARGET) $@
	$(MKIMAGE) -A arm -T script -C none -n "boot.scr" -d $(GTA04_BOOTSCR_SOURCE) $(INSTALLED_BOOTSCR_TARGET)
	$(call pretty,"Bootloader image: $@")
	$(call pretty,"Boot-scr: $(INSTALLED_BOOTSCR_TARGET)")
