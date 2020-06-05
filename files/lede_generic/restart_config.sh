#!/bin/sh
# (c) 2019 minim.co

if [ "$1" == "" ]; then
	restart_inits=1
else
	# No need to restart the init scripts
	# A parameter of 0 is passed from boot sequence
	# Init scripts (network, dhcp etc) are anyhow executed later
	# as part of boot sequence
	restart_inits=$1
fi

wifi_onboarding_supported=0
if [ -f /sbin/onboard.sh ]; then
	wifi_onboarding_supported=1
fi
if [ $wifi_onboarding_supported -eq 1 ]; then
	source onboard.sh
fi

# Check if there is a need to change the mode
cur_op_mode=`/sbin/uci get minim.@unum[0].cur_opmode`
op_mode=`/sbin/uci get minim.@unum[0].opmode`

if [ "$op_mode" == "" ]; then
	use_op_mode="gw"
elif [ $wifi_onboarding_supported -eq 1 ]; then
	# op_mode has been set. Remove station interface
	create_onboard_sta 0
	# For other processes to know that the Repeater is onboarded
	touch /tmp/rep_boarded.txt
	# set use_op_mode
	use_op_mode=$op_mode
else
	use_op_mode=$op_mode
fi

mode_change=0
sm_args="" #Arguments to switch mode
if [ "$op_mode" == "" ] && [ "$cur_op_mode" == "" ]; then
        # post factory reset we will be in this condition
        # do not force a mode change in case user has customized
        # settings prior to retroelk connectivity
	mode_change = 0
elif [ "$op_mode" == "gw" ] && [ "$cur_op_mode" == "" ]; then
        # if the user has customized their configuration prior
        # to having connectivity to retroelk, when they connect
        # for the first time in gateway mode, we don't want to
        # pave over their settings so don't change the mode
	mode_change=0
elif [ "$op_mode" != "gw" ] && [ "$cur_op_mode" == "" ]; then
        # if the user has configured the device in retroelk to
        # to be a non-gateway, and this is the first time it's
        # connecting, then change the mode, because the default
        # is gateway
	mode_change=1
elif [ "$cur_op_mode" != "$op_mode" ]; then
        # if we have connected prior (and thus have cur_op_mode)
        # and the op_mode is now differnt,
	mode_change=1
fi

if [ $mode_change -eq 0 ]; then
	# No change in the mode
	# No need to explicity adjust WAN or LAN protocols or network config
	sm_args=" --no-wan-changes --no-lan-dhcp-changes --no-switch-config"
fi

#echo $op_mode > /tmp/op_mode
#echo $cur_op_mode > /tmp/cur_op_mode
#echo $use_op_mode > /tmp/use_op_mode
#echo $mode_change > /tmp/mode_change

# Server always pushes config it happens to have stored and
# ends up pushing switch_vlan entries from the time device
# was in the gateway mode. We have to make sure config is
# sound with regard to our changes every time we receive
# anything from the server side.
/sbin/switch_mode.sh $use_op_mode $sm_args
/sbin/uci set minim.@unum[0].cur_opmode=$op_mode
/sbin/uci commit

if [ $restart_inits -eq 1 ]; then
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
	/etc/init.d/rep_conn restart
	# Restart LAN address fixer script
	/etc/init.d/fix_lan_addr restart
	# Update LED state
	/sbin/update_led.sh
fi
if [ $mode_change -eq 1 ]; then
	exit 1
else
	exit 0
fi
