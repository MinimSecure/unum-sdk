#!/bin/sh

source m_functions.sh

function switch_channel ()
{
	#Skip this channel. This has been tried and 
	# it does n't connect to Internet
	skip=$1

	chan1=0
	chan2=0

	prev_bs=""
	prev_chan=""

	cur_meshid=`/sbin/uci get wireless.mesh.mesh_id`

	if [ "$cur_meshid" ]; then
		#Sometimes uci fails
		cur_meshid=`/sbin/uci get wireless.mesh.mesh_id`
	fi
	# We need to know the BSSID, channel and Mesh ID
	# Grep only those lines containing this information

	# The order is BSS, followed by channel and optionally 
	# MeshID if it is a mesh network

	iw wlan1 scan meshid $cur_meshid | grep 'MESH ID\|^BSS\|primary' |
	while true
	do
		read -r line
		[ -n "$line" ] || break
		bs=`echo "$line" | grep "^BSS"| awk '{print $2}' | \
				awk 'BEGIN{FS="\("} {print $1}'`

		if [ "$bs" != "" ]; then
			#Start of new BSS
			read -r line
			[ -n "$line" ] || break
			channel=`echo "$line" | awk 'BEGIN{FS=":"} {print $2}'`
			[ -n "$channel" ] || break
			prev_bs=$bs
			prev_chan=$channel
		else
			# This can be a line containing MESH ID
			meshid=`echo "$line" | grep "MESH ID"`

			if [ "$meshid" == "" ]; then
				#This should n't happen
				logger "Error while parsing the line. $line"
				continue
			fi
			meshid1=`echo "$line" | grep "MESH ID" | \
					 awk 'BEGIN{FS=":"}{print $2}'`

			#Remove begining spaces
			meshid=`echo $meshid1 | sed 's/^ *//g'`
			if [ "$cur_meshid" != "$meshid" ]; then
				#Not our mesh
				continue
			fi
			if [ "$prev_chan" == "" ]; then
				#No corresponding channel captured. Skp this
				continue
			fi
			if [ "$skip" != "" ] && \
				[ "$prev_chan" == "$skip" ]; then
				#We have already tried this channel. Skip this
				continue
			fi
			channel=`echo $prev_chan | sed 's/^ *//g'`
			echo $channel
			if [ "$channel" != "" ]; then
				#Set the channel and Restart the network
				/sbin/uci set wireless.radio1.channel=$channel
				/sbin/uci commit wireless
				/sbin/wifi up >/dev/null
			fi
		fi
	done
}

#Give some breathing time for the board to come-up
sleep 30

mesh_status=`/sbin/uci get wireless.mesh.disabled`
opmode=`/sbin/uci get minim.@unum[0].opmode`

# turn the mesh led off by default
set_mesh_led_state 0

# continue only if we are an AP
if [ "$opmode" != "mesh_11s_ap" ]; then
	logger "I am not a 11S AP. I dont need to set my channel"
	exit 0
fi
# continue only if mesh is not disabled in the config
if [ $mesh_status -eq 1 ]; then
	exit 0
fi

count=0
prev_selected_chan=""
while true
do
	#Check if the default gateway is reachable
	rootgw=`ip route show | grep default | awk '{print $3}'`
	if [ "$rootgw" == "" ]; then
		count=`expr $count + 1`
		set_mesh_led_state 0
	else 
		root_ping=`ping -q -4 -c 1  $rootgw | \
				grep "1 packets received" | wc | awk '{print $1}'`
		if [ $root_ping -eq 0 ]; then
			count=`expr $count + 1`
			set_mesh_led_state 0
		else
			count=0
			set_mesh_led_state 1
		fi
	fi
	#Could n't reach the default gateway for 5 consecutive attempts
	#Time switch to new channel and try
	if [ $count -ge 5 ]; then
		# Looks like we are not connected to the Root
		logger "Not connected with root. Try switching to new channel"
		prev_selected_chan=$(switch_channel $prev_selected_chan)
		count=0
	fi
	sleep 20
done
