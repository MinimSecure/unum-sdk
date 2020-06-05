#!/bin/sh

source m_functions.sh
function usage()
{
	echo "This script is used to configure the UCI settings to"
	echo "switch between AP and Router Modes"
	echo "/sbin/switch_mode.sh <ap|gw|mesh_11s_ap|mesh_11s_gw>"
	echo "                     [--no-lan-dhcp-changes] [--no-wan-changes]"
	echo "                     [--no-switch-config]"
}

lan_dhcp_changes=1
wan_changes=1
mode=""
switch_config=1

#Parse the arguments
while [ "$1" != "" ]; do
	name=`echo $1`
	case $name in
	ap)
		mode="ap"
	;;
	gw)
		mode="gw"
	;;
	mesh_11s_ap)
		mode="mesh_11s_ap"
	;;
	mesh_11s_gw)
		mode="mesh_11s_gw"
	;;
	--no-lan-dhcp-changes)
		lan_dhcp_changes=0
	;;
	--no-wan-changes)
		wan_changes=0
	;;
	--no-switch-config)
		switch_config=0
	;;
	*)
		usage
		exit 1
	;;
	esac
    	shift
done

if [ "$mode" == "" ]; then
	usage
	exit 1
fi

if [ "$mode" == "ap" ] || [ "$mode" == "mesh_11s_ap" ]; then

	if [ $lan_dhcp_changes -eq 1 ]; then
		# Change DHCP settings
		/sbin/uci set network.lan.proto="dhcp"
		/sbin/uci set dhcp.lan.ignore=1
	fi

	if [ $wan_changes -eq 1 ]; then
		# WAN Interface should n't be there for Repeater.
		# Everything is under LAN
		/sbin/uci delete network.wan
	fi

	# Commit the changes
	/sbin/uci commit 
	sleep 1

	if [ $switch_config -eq 1 ]; then
		handle_switch_config "ap"
	fi

	# No need to restart any of the services
	# All restarts are handled in the calling script
elif [ "$mode" == "gw" ] || [ "$mode" == "mesh_11s_gw" ]; then

	# LAN Settings
	if [ $lan_dhcp_changes -eq 1 ]; then
		/sbin/uci set network.lan.proto="static"
		/sbin/uci set dhcp.lan.ignore=0
	fi
	if [ $wan_changes -eq 1 ]; then
		# WAN Settings
		/sbin/uci set network.wan=interface
		wan_if_name=$(get_wan_if_name)
		/sbin/uci set network.wan.ifname=$wan_if_name
		/sbin/uci set network.wan.proto=dhcp
	fi

	# Commit the changes
	/sbin/uci commit 
	sleep 1

	if [ $switch_config -eq 1 ]; then
		handle_switch_config "gw"
	fi
	# No need to restart any of the services
	# All restarts are handled in the calling script
fi
