#!/bin/sh

# Restart services
/sbin/rc rc_service stop_dnsmasq
/sbin/rc rc_service start_dnsmasq
/sbin/rc rc_service restart_firewall
/sbin/rc rc_service restart_wireless

# Wait till restart is complete (up to 60 sec)
# This has to wait till server UI can handle it.
# The rc_service should not be droping into background either.
while [ "`nvram get rc_service`" != "" ] && [ 0$count -lt 60 ]; do
  sleep 1
  let "count++"
done

# start httpd
/sbin/rc rc_service start_httpd
