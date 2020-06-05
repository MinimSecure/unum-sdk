#!/bin/sh

mkdir -p /var/tmp/

macaddr=`hexdump -x -n 6 /dev/mtd6 | head -n 1 | \
		awk '{printf("%s:%s:%s:%s:%s:%s\n", substr($2,1,2), \
		substr($2,3,2), substr($3, 1,2), substr($3,3,2), \
		substr($4,1,2), substr($4,3,2));}'`
if [ "$macaddr" == "" ]; then
	# MAC is empty
	# Get it from backup file
	cat /etc/deviceid.txt > /var/tmp/deviceid.txt
	# What if the backup file does n't exist?
	# If it does n't, we are left with no other option, but reboot
else
	# Save this MAC to a backup file for future use
	echo $macaddr > /etc/deviceid.txt
	echo $macaddr > /var/tmp/deviceid.txt
fi
