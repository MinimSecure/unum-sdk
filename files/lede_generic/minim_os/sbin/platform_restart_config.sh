#!/bin/sh
bname=`cat /tmp/sysinfo/board_name`
case $bname in
motorola,r14)
	killall wpa_supplicant
	;;
esac
