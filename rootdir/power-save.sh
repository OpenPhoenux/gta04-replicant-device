#!/system/bin/sh
# Modem (already uses USB autosuspend)
# grep "" /sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/*/*/power/control
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.0/tty/ttyHS0/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.1/tty/ttyHS1/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.2/tty/ttyHS2/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.3/tty/ttyHS3/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.4/tty/ttyHS4/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.6/tty/ttyHS5/power/control:auto
#/sys/devices/platform/usbhs_omap/ehci-omap.0/usb1/1-2/1-2:1.5/net/hso0/power/control:auto

# UARTs
echo 2000 > /sys/devices/platform/omap_uart.1/power/autosuspend_delay_ms # Bluetooth HCI
echo 2000 > /sys/devices/platform/omap_uart.1/power/autosuspend_delay_ms # GPS module (NMEA)
echo 2000 > /sys/devices/platform/omap_uart.2/power/autosuspend_delay_ms # RS232 console / IrDA

# Sensors
echo 3 > /sys/bus/iio/devices/iio:device0/operating_mode # Compass HMC5883l
echo 0 > /sys/bus/iio/devices/iio:device1/sampling_frequency # Gyroscope ITG3200
echo 0 > /sys/bus/i2c/devices/2-0077/oversampling # Barometer BMP085
echo 0 > /sys/class/input/input1/poll # Accelerometer BMA150
echo 0 > /sys/class/input/input5/poll # Accelerometer LIS302

# Check C-States
# grep "" /sys/devices/system/cpu/cpu0/cpuidle/state*/*

