#!/bin/sh
# (C) 2019 minim.co
source onboard.sh

op_mode=`/sbin/uci get minim.@unum[0].opmode`
if [ "$op_mode" != "" ]; then
	# op_mode has been already set. 
	# Meaning the unit has been already onboarded
	# Delete if there is already any on-boarding station interface
	create_onboard_sta 0
	exit 0
fi
create_onboard_sta 1
