#!/bin/sh
board=`cat /tmp/sysinfo/board_name`

case $board in
gl-ar300m)

	macaddr=`hexdump -x -n 6 /dev/mtd6 | head -n 1 | \
		awk '{printf("%s:%s:%s:%s:%s:%s\n", substr($2,1,2), \
		substr($2,3,2), substr($3, 1,2), substr($3,3,2), \
		substr($4,1,2), substr($4,3,2));}'`

	cur_macaddr=`/sbin/uci get network.lan.macaddr`

	if [ "$macaddr" != "$cur_macaddr" ]; then
		/sbin/uci set network.lan.macaddr=$macaddr
		/sbin/uci commit network
	fi
	ifconfig eth1 down
	ifconfig eth1 hw ether $macaddr
	ifconfig eth1 up
	;;
*)
esac

