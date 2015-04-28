#!/system/bin/sh
MODEM_GPIO="/sys/class/gpio/gpio186/value" #available on GTA04a4++
if [ ! -e $MODEM_GPIO ]
then
    # Software Voice Routing
    echo "LOADING tinyalsa-audio_swrouting.xml"
    rm /system/etc/tinyalsa-audio.xml
    cp /system/etc/tinyalsa-audio_swrouting.xml /system/etc/tinyalsa-audio.xml
else
    # Hardware Voice Routing
    echo "LOADING tinyalsa-audio_hwrouting.xml"
    rm /system/etc/tinyalsa-audio.xml
    cp /system/etc/tinyalsa-audio_hwrouting.xml /system/etc/tinyalsa-audio.xml
fi
chown 502.50 /system/etc/tinyalsa-audio.xml
chmod 644 /system/etc/tinyalsa-audio.xml

