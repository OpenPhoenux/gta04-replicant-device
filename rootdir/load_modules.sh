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

#This scipt loads some modules, which are not loaded automatically (for some reason)
DEVICE=$(cat /sys/firmware/devicetree/base/model)
echo "Detected model:" $DEVICE

#PANEL
insmod /system/lib/modules/omapdss-base.ko
insmod /system/lib/modules/omapdss.ko
insmod /system/lib/modules/panel-tpo-td028ttec1.ko

#TV-Out - needed by omapdrm unless disabled in DT
insmod /system/lib/modules/encoder-opa362.ko
insmod /system/lib/modules/connector-analog-tv.ko

#FRAMEBUFFER
insmod /system/lib/modules/drm_panel_orientation_quirks.ko
insmod /system/lib/modules/drm.ko
insmod /system/lib/modules/fb_sys_fops.ko
insmod /system/lib/modules/cfbfillrect.ko
insmod /system/lib/modules/syscopyarea.ko
insmod /system/lib/modules/cfbimgblt.ko
insmod /system/lib/modules/sysfillrect.ko
insmod /system/lib/modules/sysimgblt.ko
insmod /system/lib/modules/cfbcopyarea.ko
insmod /system/lib/modules/drm_kms_helper.ko
insmod /system/lib/modules/omapdrm.ko

#BACKLIGHT
insmod /system/lib/modules/pwm-omap-dmtimer.ko
insmod /system/lib/modules/pwm_bl.ko
chown system.system /sys/class/backlight/backlight/brightness
chown system.system /sys/class/backlight/backlight/max_brightness

#USB
insmod /system/lib/modules/ehci-omap.ko
insmod /system/lib/modules/musb_hdrc.ko
insmod /system/lib/modules/omap_hdq.ko
insmod /system/lib/modules/phy-twl4030-usb.ko
insmod /system/lib/modules/industrialio.ko
insmod /system/lib/modules/twl4030-madc.ko
insmod /system/lib/modules/twl4030_charger.ko

#CHARGER
insmod /system/lib/modules/w1_bq27000.ko
insmod /system/lib/modules/bq27xxx_battery.ko
insmod /system/lib/modules/bq27xxx_battery_hdq.ko

#TOUCH
insmod /system/lib/modules/tsc2007.ko

#FUSE/VOLD
insmod /system/lib/modules/fuse.ko

#NETWORKING
insmod /system/lib/modules/x_tables.ko
insmod /system/lib/modules/ip_tables.ko

insmod /system/lib/modules/ipv6.ko
insmod /system/lib/modules/ip6_tables.ko
insmod /system/lib/modules/ip6table_mangle.ko
insmod /system/lib/modules/ip6table_raw.ko
insmod /system/lib/modules/ip6table_filter.ko

insmod /system/lib/modules/nf_conntrack.ko
insmod /system/lib/modules/nf_defrag_ipv4.ko
insmod /system/lib/modules/nf_conntrack_ipv4.ko
insmod /system/lib/modules/nf_nat.ko
insmod /system/lib/modules/nf_nat_ipv4.ko
insmod /system/lib/modules/iptable_nat.ko
insmod /system/lib/modules/iptable_mangle.ko
insmod /system/lib/modules/iptable_filter.ko
insmod /system/lib/modules/iptable_raw.ko

#USB/ADB
insmod /system/lib/modules/omap2430.ko
insmod /system/lib/modules/configfs.ko
insmod /system/lib/modules/libcomposite.ko
insmod /system/lib/modules/usb_f_fs.ko
#modprobe g_ffs idVendor=0x18d1 idProduct=0x4e26 #FIXME: Busybox modprobe does not correctly pass the module parameters
insmod /system/lib/modules/g_ffs.ko idVendor=0x18d1 idProduct=0x4e26
#Temoporary ADB fix:
mount -o uid=2000,gid=2000 -t functionfs adb /dev/usb-ffs/adb
restart adbd

#AUDIO
insmod /system/lib/modules/snd-soc-twl4030.ko
insmod /system/lib/modules/snd-soc-simple-card-utils.ko
insmod /system/lib/modules/snd-soc-simple-card.ko
insmod /system/lib/modules/snd-soc-omap-twl4030.ko
insmod /system/lib/modules/snd-pcm-dmaengine.ko
insmod /system/lib/modules/snd-soc-omap.ko
insmod /system/lib/modules/snd-soc-omap-mcbsp.ko

#VIBRA
insmod /system/lib/modules/twl4030-vibra.ko

#BUTTON
insmod /system/lib/modules/twl4030-pwrbutton.ko

#MISC
insmod /system/lib/modules/gpio-twl4030.ko
insmod /system/lib/modules/rtc-twl.ko

#===============================================================================

#Graphics
modprobe panel_tpo_td028ttec1 #Letux 2804
modprobe panel_dpi #Letux 3704/7004
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
modprobe twl4030_madc
modprobe twl4030_charger allow_usb=1
modprobe omap_hdq
modprobe w1_bq27000
case "$DEVICE" in
    "Goldelico GTA04A3/Letux 2804" | "Goldelico GTA04A4/Letux 2804" | "Goldelico GTA04A5/Letux 2804" )
        #GTA04 a3/a4/a5 (Letux 2804)
        echo "LOADING bq27xxx_battery"
        modprobe bq27xxx_battery
        echo 5 > /sys/module/bq27xxx_battery/parameters/poll_interval
        ;;
    "Goldelico GTA04b2/Letux 3704" | "Goldelico GTA04b3/Letux 7004" )
        #GTA04 b2/b3 (Letux 3704 / Letux 7004)
        echo "LOADING generic_adc_battery"
        modprobe generic_adc_battery
        ;;
    * ) #Fallback
        echo "Could not detect device variant."
        echo "Check your device-tree model (/sys/firmware/devicetree/base/model)."
        echo "Falling back to bq27xxx_battery"
        modprobe bq27xxx_battery
        echo 5 > /sys/module/bq27xxx_battery/parameters/poll_interval
        ;;
esac

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

#FIXME: start some services manually, to make the system boot up
start installd
start netd
start media
start vold

sleep 6
kill `pidof mediaserver`
