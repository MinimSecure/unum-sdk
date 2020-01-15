#!/bin/sh
# (c) 2019 minim.co

# Apply settings like timezone, hostname, logs etc
/etc/init.d/system restart
# Network settings
/etc/init.d/network restart
# This seems to take some time even after command completion.
# Will remove sleep if not needed
sleep 5
#DHCP and DNS server settings
/etc/init.d/dnsmasq restart
# Firewall settings
/etc/init.d/firewall restart
