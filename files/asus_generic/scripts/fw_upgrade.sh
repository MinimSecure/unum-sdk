#!/bin/sh

mv -f $1 /tmp/linux.trx

# This is how Asus does firmware check
nvram set firmware_check=0
/sbin/firmware_check /tmp/linux.trx
sleep 1
firmware_check_ret=`nvram get firmware_check`
if [ "$firmware_check_ret" != "1" ]; then
    exit 1
fi

# Force pull cloud config after the upgrade
/bin/nvram unset restore_from_cloud

# This is how Asus does the upgrade
/sbin/ejusb -1 0
rc rc_service restart_upgrade
