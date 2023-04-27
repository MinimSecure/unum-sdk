#!/bin/sh
bname=`cat /tmp/sysinfo/board_name`
case $bname in
motorola,r14)
	wanspeed=`cat /sys/class/net/eth1/speed`
	if [ ! -f /tmp/fixed_phy.txt ] && [ 0$wanspeed -eq 100 ]; then
		# We are fixing this once. Not any more
		touch /tmp/fixed_phy.txt
		ifconfig eth1 down; ifconfig eth1 up
	fi
	;;
esac
