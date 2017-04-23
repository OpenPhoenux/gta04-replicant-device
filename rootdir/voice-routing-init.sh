#!/system/bin/sh
case "$(cat /proc/device-tree/model)" in
	*GTA04A[1-3]* ) 
    # Software Voice Routing
    echo "LOADING tinyalsa-audio_swrouting.xml"
    rm /system/etc/tinyalsa-audio.xml
    cp /system/etc/tinyalsa-audio_swrouting.xml /system/etc/tinyalsa-audio.xml
    ;;
*GTA04A[4-9]* )
    # Hardware Voice Routing
    echo "LOADING tinyalsa-audio_hwrouting.xml"
    rm /system/etc/tinyalsa-audio.xml
    cp /system/etc/tinyalsa-audio_hwrouting.xml /system/etc/tinyalsa-audio.xml
    ;;
esac

chown 502.50 /system/etc/tinyalsa-audio.xml
chmod 644 /system/etc/tinyalsa-audio.xml

