function handle_switch_config ()
{
	mode=$1
	WAN_VLANS=`/sbin/uci show network | sed -e "s/network\.\(.*\)\.vlan='2'/\1/;t;d"`
	for SECTION in $WAN_VLANS; do /sbin/uci delete network.$SECTION; done
	if [ "$mode" == "ap" ]; then
		/sbin/uci set network.@switch_vlan[0].ports="0t 1 2 3 4 5"
	elif [ "$mode" == "gw" ]; then
		/sbin/uci set network.@switch_vlan[0].ports="0t 2 3 4 5"
		/sbin/uci set network.wanvlan=switch_vlan
		/sbin/uci set network.wanvlan.device="switch0"
		/sbin/uci set network.wanvlan.vlan="2"
		/sbin/uci set network.wanvlan.ports="0t 1"
	fi
	/sbin/uci commit network
}

function get_wan_if_name ()
{
	echo "eth0.2"
}
