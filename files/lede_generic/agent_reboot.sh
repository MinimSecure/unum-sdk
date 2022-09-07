#!/bin/sh

# Issue led sequence for reboot
[ -x /sbin/leds.sh ] && /sbin/leds.sh "led_reboot"

/sbin/reboot
