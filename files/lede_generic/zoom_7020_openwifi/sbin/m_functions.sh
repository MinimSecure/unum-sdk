
function get_mh7020_mac_suffix ()
{
	mac_suffix=`hexdump -x -n 6 /dev/mtd7 | head -n 1 | \
		awk '{printf("%s%s\n", \
		substr($4,4,1), substr($4,1,2));}'`	
	echo $mac_suffix	
}

function get_wan_if_name ()
{
	echo "eth1"
}

