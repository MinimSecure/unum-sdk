#!/bin/sh

mkdir -p /var/tmp/

macaddr=`/usr/sbin/fw_printenv | grep ^ethaddr= |awk 'BEGIN{FS="="} {print $2}'`
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
