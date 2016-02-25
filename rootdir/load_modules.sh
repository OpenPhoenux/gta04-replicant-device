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
modprobe omap_hdq
modprobe w1_bq27000
modprobe bq27xxx_battery
#modprobe twl4030_madc_battery #optional for L3740/L7004

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

#Vibracall
modprobe twl4030_vibra

#Bluetooth
modprobe w2cbw003-bluetooth
modprobe hci_uart

#WiFi
modprobe leds-tca6507 #needed for MMC reset/power (the WiFi chip is connected via MMC)
modprobe libertas #autoloads the cfg80211 dependency
#libertas_sdio is loaded by the Android framework, once WiFi is enabled

#WWAN
chmod 777 /dev/rfkill
modprobe wwan-on-off
modprobe ehci-omap
