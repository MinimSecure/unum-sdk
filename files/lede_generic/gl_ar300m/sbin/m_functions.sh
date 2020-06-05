function get_wan_if_name ()
{
	echo "eth0"
}

function handle_switch_config ()
{
	mode=$1
	if [ "$mode" == "ap" ]; then
		/sbin/uci set network.lan.ifname="eth0 eth1"
	elif [ "$mode" == "gw" ]; then
		/sbin/uci set network.lan.ifname="eth1"
	fi
	/sbin/uci commit network
}
