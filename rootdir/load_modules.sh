#!/system/bin/sh
#This scipt loads some modules, which are not loaded automatically (for some reason)
DEVICE=$(cat /sys/firmware/devicetree/base/model) #TODO: for later use

#Graphics
modprobe panel_tpo_td028ttec1
modprobe omapdrm

#Backlight
modprobe pwm-omap-dmtimer
modprobe pwm_bl

#Touchscreen
modprobe tsc2007

#Battery/Charger
modprobe phy-twl4030-usb
modprobe twl4030_charger
modprobe twl4030_madc_battery #TODO bq27xxx

#Power Button
modprobe twl4030-pwrbutton

#USB FunctionFS (ADB)
modprobe libcomposite
modprobe usb_f_fs
#modprobe g_ffs idVendor=0x18d1 idProduct=0x4e26 #FIXME: Busybox modprobe does not correctly pass the module parameters
insmod /system/lib/modules/g_ffs.ko idVendor=0x18d1 idProduct=0x4e26
#Temoporary ADB fix:
mount -o uid=2000,gid=2000 -t functionfs adb /dev/usb-ffs/adb
restart adbd
