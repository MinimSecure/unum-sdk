#!/bin/sh

# Issue led sequence for factory reset
[ -x /sbin/leds.sh ] && /sbin/leds.sh "led_factory_defaults"

/sbin/firstboot -y -r

