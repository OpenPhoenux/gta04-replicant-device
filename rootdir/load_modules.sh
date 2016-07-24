#!/system/bin/sh
#Debian/4.5 uses this extra modules, which are unused in Replicant, up to now:
#autofs4                25916  1 
#usb_f_ecm               7039  1 
#g_ether                 4993  0 
#usb_f_rndis            16962  2 g_ether
#u_ether                13270  3 usb_f_ecm,usb_f_rndis,g_ether
#ipv6                  410330  20 
#hso                    30144  0 #(this is built-in the Replicant kernel)
#encoder_opa362          3378  1 
#twl4030_madc_hwmon      3361  0 
#connector_analog_tv     3566  1 
#twl4030_madc_battery     3998  0 
#twl4030_madc            9490  1 twl4030_madc_hwmon


#This scipt loads some modules, which are not loaded automatically (for some reason)
DEVICE=$(cat /sys/firmware/devicetree/base/model) #TODO: for later use

#Graphics
modprobe panel_tpo_td028ttec1
modprobe omapdrm

#Audio
modprobe snd-soc-twl4030
modprobe snd-soc-simple-card
modprobe snd-soc-omap-twl4030
modprobe snd-soc-omap-mcbsp

#Backlight
modprobe pwm-omap-dmtimer
modprobe pwm_bl
chown system.system /sys/class/backlight/backlight/brightness
chown system.system /sys/class/backlight/backlight/max_brightness

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
modprobe omap2430 #used for MUSB/UDC
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
modprobe ehci-omap
modprobe wwan-on-off
chmod 777 /dev/rfkill

#Sensors
modprobe bmp085-i2c
modprobe itg3200
modprobe hmc5843_i2c
modprobe lis3lv02d_i2c
modprobe bma150
chmod 666 /dev/input/*
chmod 666 /sys/class/input/*/poll
chmod 666 /dev/iio:device*
#chmod 666 /sys/bus/iio/devices/iio:device*/*
chmod 666 /sys/bus/iio/devices/iio:device*/scan_elements/*
chmod 666 /sys/bus/iio/devices/iio:device*/trigger/*
chmod 666 /sys/bus/iio/devices/iio:device*/buffer/enable
chmod 666 /sys/bus/iio/devices/iio:device*/sampling_frequency

#GPS
modprobe w2sg0004
modprobe extcon-gpio

#Misc
modprobe gpio_twl4030
modprobe rtc_twl
modprobe at24 #I2C-EEPROM
