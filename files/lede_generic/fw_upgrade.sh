#!/bin/sh

# Issue led sequence for firmware update
[ -x /sbin/leds.sh ] && /sbin/leds.sh "led_firmware_update"

wifi down

# Issue the firmware upgrade command
# MD5 errors and platform checks are taken care by sysupgrade script
/sbin/sysupgrade "$1"

# Clear the led sequence in the case that sysupgrade returns
[ -x /sbin/leds.sh ] && /sbin/leds.sh "led_off"

wifi up

