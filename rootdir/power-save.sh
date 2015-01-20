#!/system/bin/sh
# Modem already uses USB autosuspend:
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.0/tty/ttyHS0/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.1/tty/ttyHS1/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.2/tty/ttyHS2/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.3/tty/ttyHS3/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.4/tty/ttyHS4/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.6/tty/ttyHS5/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.5/net/hso0/power/control:auto

# UARTs
#echo 2000 > /sys/devices/platform/omap_uart.1/power/autosuspend_delay_ms # Might break battery readings
echo 2000 > /sys/devices/platform/omap_uart.1/power/autosuspend_delay_ms
echo 2000 > /sys/devices/platform/omap_uart.2/power/autosuspend_delay_ms

# Sensors
# UNTESTED
#echo 3 > /sys/bus/iio/devices/iio:device0/operating_mode
#echo 0 > /sys/bus/iio/devices/iio:device1/sampling_frequency
#echo 0 > /sys/bus/i2c/devices/2-0077/oversampling

# Check C-States
# grep "" /sys/devices/system/cpu/cpu0/cpuidle/state*/*

