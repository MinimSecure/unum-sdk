#!/bin/sh

source m_functions.sh

op_mode=`/sbin/uci get minim.@unum[0].opmode`
if [ "$op_mode" != "" ]; then
	# op_mode has been already set. 
	# Meaning the unit has been already onboarded
	exit 0
fi

ssid=$(get_on_board_ssid)

INTERFACE=wlan1
DELAY=1

FREQLIST=$(iw phy phy1 channels | grep "MHz \[" | cut -d ' ' -f 2)
while [ 1 ] ; do
	for freq in $FREQLIST ; do
		iw $INTERFACE scan freq $freq flush | grep -q $ssid
		[ $? == 0 ] && {
			uci set wireless.sta0.disabled=0
			uci commit wireless
			wifi
			/sbin/run_on_board_sta.sh &
			exit 0
		}

		# Check if onboarding is completed
		if [ -f /tmp/rep_boarded.txt ]; then
			# Onboarding interface is destroyed (and network restart)
			# by restart_config.sh
			restart_network=0
			logger "Device is onboarded. Exit Provisioning loop"
			exit 0
		fi

		sleep $DELAY
	done
done
