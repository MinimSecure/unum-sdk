
function get_mac ()
{
	mac=`fw_printenv -n ethaddr`
	echo $mac
}

function get_serial_num ()
{
	suff=`fw_printenv -n sn`
	echo $suff
}

function get_wan_if_name ()
{
	echo "eth1"
}

function handle_switch_config ()
{
	mode=$1
	if [ "$mode" == "ap" ]; then
		/sbin/uci set network.lan.ifname="eth0 eth1"
	elif [ "$mode" == "gw" ]; then
		/sbin/uci set network.lan.ifname="eth0"
	fi
	/sbin/uci commit network
}

function get_on_board_ssid ()
{
	mac=$(get_mac)
	ssid="minim-"$mac
	echo $ssid
}

function set_mesh_led_state()
{
	# mesh led behavior to be determined
}

