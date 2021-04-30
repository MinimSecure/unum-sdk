
function get_ipq40xx_mac_suffix ()
{
	mac_suffix=`hexdump -x -n 6 /dev/mtd7 | head -n 1 | \
		awk '{printf("%s%s\n", \
		substr($4,4,1), substr($4,1,2));}'`	
	echo $mac_suffix	
}

function get_ipq40xx_device_mac ()
{
	mac=`hexdump -x -n 6 /dev/mtd7 | head -n 1  | \
		awk '{printf("%s:%s:%s:%s:%s:%s\n", substr($2,3,2), \
		substr($2,1,2), substr($3,3,2), substr($3,1,2), \
		substr($4,3,2), substr($4,1,2));}'`
	echo $mac
}

function get_b1300_serial_num ()
{
	suff=`dd if=/dev/mtd7 bs=1 skip=$((0x30)) count=16 2>/dev/null`
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
	mac=$(get_ipq40xx_device_mac)
	ssid="minim-"$mac
	echo $ssid
}

function set_mesh_led_state()
{
	echo $1 > /sys/class/leds/gl-b1300\:green\:mesh/brightness
}

function get_b1300_country_code()
{
	prefix=`dd if=/dev/mtd7 bs=1 skip=$((0x80)) count=8 2>/dev/null`

	if [ "$prefix" = "COUNTRY:" ] ; then
		offset=$((0x88))
	else
		offset=$((0x80))
	fi

	cc=`dd if=/dev/mtd7 bs=1 skip=$offset count=2 2>/dev/null`
	expr match "$cc" "[A-Z][A-Z]$" > /dev/null
	if [ $? -eq 0 ] ; then
		echo $cc
	else
		echo US
	fi
}
