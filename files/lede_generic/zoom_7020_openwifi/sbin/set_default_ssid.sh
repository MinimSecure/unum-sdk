#!/bin/sh

source m_functions.sh

[ -f /tmp/wireless_exists.txt ] ||
#Special cases
bname=`cat /tmp/sysinfo/board_name`
case $bname in
motorola,mh7020)
	mac_suffix=$(get_mh7020_mac_suffix)

	# For Motorola MH7020, ssid is MH7020- followed by
	# last 3 niblles of MAC Address
	ssid="MH7020-"$mac_suffix
	key="openwifi"

	#Set the default SSID and Key for both the radios
	for devidx in 0 1 2
	do
		uci -q batch <<-EOF
			set wireless.default_radio${devidx}.ssid=$ssid
			set wireless.default_radio${devidx}.encryption="psk2"
			set wireless.default_radio${devidx}.key=$key
			set wireless.default_radio${devidx}.disabled=0
			set wireless.radio${devidx}.disabled=0

EOF
	done
;;
*)
esac
uci -q commit wireless
rm /tmp/wireless_exists.txt
