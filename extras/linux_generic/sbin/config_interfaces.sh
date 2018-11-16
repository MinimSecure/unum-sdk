#!/usr/bin/env bash
# (c) 2018 minim.co
# Network interface configuration helper

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

echo "This script will remove all existing interfaces defined"
echo "in /etc/network/interfaces.d/*."

if ! confirm "Continue?" "yes"; then
    echo "Exiting without making any changes."
    exit 0
fi
rm -fv /etc/network/interfaces.d/*

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

prompt "Enter MAC address for the LAN interface"
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

if [[ ! -z "$phyname_wlan" ]]; then
    # Configure a wireless interface
    if [[ "$ifname_wlan" != "$ifname_lan" ]]; then
        # Create the secondary lan ethernet interface
        echo "auto $ifname_lan
iface $ifname_lan inet manual
pre-up ifconfig \$IFACE up
post-down ifconfig \$IFACE down
metric 0
" > "/etc/network/interfaces.d/$ifname_lan"

        # Create the bridge containing the lan and wlan interfaces
        echo "auto $ifname_bridge
iface $ifname_bridge inet static
address 192.168.$subnet_simple.1
netmask 255.255.255.0
gateway 192.168.$subnet_simple.1
bridge_ports $ifname_lan $ifname_wlan
up /usr/sbin/brctl stp $ifname_bridge on
" > "/etc/network/interfaces.d/$ifname_bridge"

    fi

    # Create the wireless lan interface
    echo "allow-hotplug $ifname_wlan
iface $ifname_wlan inet manual
hwaddress $hwaddr_lan
hostapd /etc/hostapd/hostapd-$phyname_wlan.conf
" > "/etc/network/interfaces.d/$ifname_wlan"
else

    # lan ethernet interface
    echo "auto $ifname_lan
iface $ifname_lan inet static
address 192.168.$subnet_simple.1
netmask 255.255.255.0
gateway 192.168.$subnet_simple.1
hwaddress $hwaddr_lan
" > "/etc/network/interfaces.d/$ifname_lan"
fi

# wan ethernet interface
echo "auto $ifname_wan
iface $ifname_wan inet dhcp
" > "/etc/network/interfaces.d/$ifname_wan"

service networking restart
