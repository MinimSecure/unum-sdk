#!/bin/sh

macaddr=`hexdump -x /dev/mtd0 | tail -n 10 | head -n 1 | \
	awk '{printf("%s:%s:%s:%s:%s:%s\n", substr($2,1,2), \
	substr($2,3,2), substr($3, 1,2), substr($3,3,2), \
	substr($4,1,2), substr($4,3,2));}'`

# At boot time, it looks like, the interfaces are not up by the time
# we reach here. So wait all the interfaces are up
# This script at boot time called just before unum start

ifout=`/sbin/ifconfig eth0.1`
while [ "$ifout" == "" ];
do
	sleep 5
	ifout=`/sbin/ifconfig eth0.1`
done
/sbin/ifconfig eth0.1 down
/sbin/ifconfig eth0.1 hw ether $macaddr
/sbin/ifconfig eth0.1 up

