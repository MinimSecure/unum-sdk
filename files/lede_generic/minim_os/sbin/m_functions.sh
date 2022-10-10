
function get_mac ()
{
	mac=`fw_printenv -n mfg_base_mac | tr 'A-F' 'a-f'`
	echo $mac
}

function get_serial_num ()
{
	suff=`fw_printenv -n mfg_sn | tr 'A-Z' 'a-z'`
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
		/sbin/uci add_list network.@device[0].ports="eth1"
	elif [ "$mode" == "gw" ]; then
		/sbin/uci del_list network.@device[0].ports="eth1"
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
	true
}

