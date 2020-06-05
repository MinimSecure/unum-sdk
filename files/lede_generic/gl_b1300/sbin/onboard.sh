source m_functions.sh

function create_onboard_sta ()
{
	create=$1
	ssid=$(get_on_board_ssid) # This is per platform
	key=$(get_b1300_serial_num) # This is per platform

	if [ $create -eq 1 ]; then
		/sbin/uci set network.wwan=interface
		/sbin/uci set network.wwan.ifname="sta0"
		/sbin/uci set network.wwan.proto=dhcp

		/sbin/uci set wireless.radio0.disabled=0
		/sbin/uci set wireless.sta0="wifi-iface"
		/sbin/uci set wireless.sta0.device='radio1'
		/sbin/uci set wireless.sta0.ifname='sta0'
		/sbin/uci set wireless.sta0.network='wwan'
		/sbin/uci set wireless.sta0.encryption='psk2/aes'
		/sbin/uci set wireless.sta0.mode='sta'
		/sbin/uci set wireless.sta0.ssid="$ssid"
		/sbin/uci set wireless.sta0.key="$key"

		# Create firewall rule to allow client access
		/sbin/uci set firewall.wwanzone=zone
		/sbin/uci set firewall.wwanzone.name="wwan"
		/sbin/uci set firewall.wwanzone.network="wwan"
		/sbin/uci set firewall.wwanzone.input="REJECT"
		/sbin/uci set firewall.wwanzone.output="ACCEPT"
		/sbin/uci set firewall.wwanzone.forward="REJECT"
		/sbin/uci set firewall.wwanzone.masq="1"

		/sbin/uci set firewall.wwanforward=forwarding
		/sbin/uci set firewall.wwanforward.src="lan"
		/sbin/uci set firewall.wwanforward.dst="wwan"
	else
		# Remove wwan interface
		# It is a no-op if the interface does n't exist
		/sbin/uci del network.wwan
		/sbin/uci del wireless.sta0
		/sbin/uci del firewall.wwanzone
		/sbin/uci del firewall.wwanforward
	fi
	/sbin/uci commit
}

function create_onboard_ap ()
{
	create=$1
	ssid=$2
	key=$3

	if [ $create -eq 1 ]; then
		/sbin/uci set wireless.radio0.disabled=0
		/sbin/uci set wireless.onboard="wifi-iface"
		/sbin/uci set wireless.onboard.device='radio0'
		/sbin/uci set wireless.onboard.network='lan'
		/sbin/uci set wireless.onboard.encryption='psk2/aes'
		/sbin/uci set wireless.onboard.mode='ap'
		/sbin/uci set wireless.onboard.ssid="$ssid"
		/sbin/uci set wireless.onboard.key="$key"
	else
		/sbin/uci del wireless.onboard
	fi
	/sbin/uci commit
}
