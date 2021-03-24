#!/bin/sh

source m_functions.sh

[ -f /tmp/wireless_exists.txt ] ||
#Special cases
bname=`cat /tmp/sysinfo/board_name`
case $bname in
glinet,gl-b1300)
	cc=$(get_b1300_country_code)
	mac_suffix=$(get_ipq40xx_mac_suffix)
	suff=$(get_b1300_serial_num)
	suff=`echo ${suff: -8}`

	# For GL-Inet's B1300, ssid is GL-B1300- followed by
	# last 3 niblles of MAC Address
	ssid="GL-B1300-"$mac_suffix
	# key last 8 characters of serial Number
	key=$suff

	#Set the default SSID and Key for both the radios
	for devidx in 0 1
	do
		uci -q batch <<-EOF
			set wireless.default_radio${devidx}.ssid=$ssid
			set wireless.default_radio${devidx}.encryption="psk2"
			set wireless.default_radio${devidx}.key=$key
			set wireless.default_radio${devidx}.disabled=0
			set wireless.radio${devidx}.disabled=0
			set wireless.radio${devidx}.country=$cc

EOF
	done
;;
*)
esac
uci -q commit wireless
rm /tmp/wireless_exists.txt
