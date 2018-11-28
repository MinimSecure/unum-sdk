#!/usr/bin/env bash
# (c) 2018 minim.co
# Network interface configuration helper

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

declare ifname_wan=$(cat /etc/opt/unum/config.json | grep 'wan-if' | sed -E 's/^\s+?"wan-if":\s+?"(\w+)".*$/\1/')
declare ifname_lan=$(cat /etc/opt/unum/config.json | grep 'lan-if' | sed -E 's/^\s+?"lan-if":\s+?"(\w+)".*$/\1/')

prompt "Specify LAN network interface name" "$ifname_lan"
ifname_lan="$prompt_val"

declare ifname_wlan
declare phyname_wlan
if confirm "Configure wireless interface?" "yes"; then
    prompt "Specify wireless network interface name" "wlan0"
    ifname_wlan="$prompt_val"
    prompt "Specify wireless phy device name" "phy0"
    phyname_wlan="$prompt_val"
fi

declare ifname_bridge
if [[ ! -z "$phyname_wlan" ]] && [[ "$ifname_lan" != "$ifname_wlan" ]]; then
    echo "detected multiple LAN interfaces, configuring bridge"
    prompt "Specify bridge interface" "br-lan"
    ifname_bridge="$prompt_val"
fi

prompt "Specify WAN network interface" "$ifname_wan"
ifname_wan="$prompt_val"

prompt_require "Enter MAC address for the LAN interface" "$hwaddr_lan"
hwaddr_lan="$prompt_val"

# Between 0 and 255, used as the third octet in LAN IP addresses
subnet_simple="15"

echo "ifname_lan=\"$ifname_lan\""       >  "$UNUM_ETC_DIR/extras.conf.sh"
echo "ifname_wlan=\"$ifname_wlan\""     >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "phyname_wlan=\"$phyname_wlan\""   >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "ifname_bridge=\"$ifname_bridge\"" >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "ifname_wan=\"$ifname_wan\""       >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "hwaddr_lan=\"$hwaddr_lan\""       >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "subnet_simple=\"$subnet_simple\"" >> "$UNUM_ETC_DIR/extras.conf.sh"

source "$UNUM_ETC_DIR/extras.conf.sh"

minim_start_sentinel='### managed by minim ###'
minim_end_sentinel='### end managed by minim ###'
conf_check=$(grep -n "$minim_start_sentinel" "/etc/dhcpcd.conf" | cut -d':' -f1 || :)
end_check=$(grep -n "$minim_end_sentinel" "/etc/dhcpcd.conf" | cut -d':' -f1 || :)

# Remove previous configuration embedded in /etc/dhcpcd.conf, if needed
if [[ ! -z "$conf_check" ]]; then
    # $conf_check contains the line number with the sentinel
    head "-n$(( conf_check - 1 ))" /etc/dhcpcd.conf > /etc/dhcpcd.conf.tmp
    if [[ ! -z "$end_check" ]]; then
        # $end_check contains line number with end sentinel
        declare -i dhcpcd_lines=$(wc -l /etc/dhcpcd.conf | cut -d' ' -f1)
        tail -n$(( dhcpcd_lines - end_check )) /etc/dhcpcd.conf >> /etc/dhcpcd.conf.tmp
    fi
else
    cat /etc/dhcpcd.conf > /etc/dhcpcd.conf.tmp
fi

echo "### managed by minim ###
# This section is autogenerated. DO NOT EDIT
interface wlan0
static ip_address=192.168.$subnet_simple.1/24
static routers=192.168.$subnet_simple.1
static domain_name_servers=192.168.$subnet_simple.1
### end managed by minim ###" >> /etc/dhcpcd.conf.tmp

if [[ ! -f "/etc/dhcpcd.conf.pre-unum" ]]; then
    mv /etc/dhcpcd.conf /etc/dhcpcd.conf.pre-unum
fi
mv /etc/dhcpcd.conf.tmp /etc/dhcpcd.conf

service dhcpcd restart
