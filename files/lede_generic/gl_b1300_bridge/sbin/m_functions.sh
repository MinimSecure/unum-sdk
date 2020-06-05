
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
	/sbin/uci set network.lan.ifname="eth0 eth1"
	/sbin/uci set network.lan.proto="dhcp"

	/sbin/uci delete network.wan
	/sbin/uci delete network.wwan

	/sbin/uci delete network.@switch[0]
	/sbin/uci delete network.@switch[1]

	/sbin/uci delete network.lan.ipaddr
	/sbin/uci delete network.lan.netmask
	/sbin/uci delete network.lan.ip6assign

	/sbin/uci delete wireless.sta0
	/sbin/uci delete wireless.radio0
	/sbin/uci delete wireless.radio1
	/sbin/uci delete wireless.default_radio0
	/sbin/uci delete wireless.default_radio1

	/sbin/uci delete firewall.@zone[1]
	/sbin/uci delete firewall.@zone[0]

	/sbin/uci delete firewall.@rule[8]
	/sbin/uci delete firewall.@rule[7]
	/sbin/uci delete firewall.@rule[6]
	/sbin/uci delete firewall.@rule[5]
	/sbin/uci delete firewall.@rule[4]
	/sbin/uci delete firewall.@rule[3]
	/sbin/uci delete firewall.@rule[2]
	/sbin/uci delete firewall.@rule[1]
	/sbin/uci delete firewall.@rule[0]

	/sbin/uci delete firewall.@include[0]
	/sbin/uci delete firewall.@defaults[0]
	/sbin/uci delete firewall.@forwarding[0]

	/sbin/uci commit network
}

function get_on_board_ssid ()
{
	mac=$(get_ipq40xx_device_mac)
	ssid="minim-"$mac
	echo $ssid
}
