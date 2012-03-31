LOCAL_PATH := $(call my-dir)

INSTALLED_BOOTIMAGE_TARGET := $(PRODUCT_OUT)/boot.img
INSTALLED_RECOVERYIMAGE_TARGET := $(PRODUCT_OUT)/recovery.img

.NOTPARALLEL: $(INSTALLED_BOOTIMAGE_TARGET)
.NOTPARALLEL: $(INSTALLED_RECOVERYIMAGE_TARGET)

$(INSTALLED_BOOTIMAGE_TARGET): $(INSTALLED_RAMDISK_TARGET) $(INSTALLED_KERNEL_TARGET)
	$(call pretty,"Boot image: $@")
	$(ACP) $(PRODUCT_OUT)/kernel $@

$(INSTALLED_RECOVERYIMAGE_TARGET): $(recovery_ramdisk) $(recovery_kernel) $(PRODUCT_OUT)/utilities/busybox $(PRODUCT_OUT)/utilities/mke2fs
	$(call pretty,"Recovery image: $@")
	$(ACP) $(PRODUCT_OUT)/kernel-recovery $@
