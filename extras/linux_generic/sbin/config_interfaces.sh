#!/usr/bin/env bash
# Copyright 2018 Minim Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Network interface configuration helper

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

if [[ "$1" == "--no-interactive" ]]; then
    interactively=0
fi

# If either is blank, default WAN and LAN interface names to what is specified
# in config.json.
declare ifname_wan="${ifname_wan:-$(cat /etc/opt/unum/config.json | grep 'wan-if' | sed -E 's/^\s+?"wan-if":\s+?"([a-z0-9_\-]+)".*$/\1/i')}"
declare ifname_lan="${ifname_lan:-$(cat /etc/opt/unum/config.json | grep 'lan-if' | sed -E 's/^\s+?"lan-if":\s+?"([a-z0-9_\-]+)".*$/\1/i')}"

prompt_require "Specify LAN network interface name" "$ifname_lan"
ifname_lan="$prompt_val"

declare ifname_wlan
declare phyname_wlan
declare disabled_wlan
declare config_wlan_value="yes"
if [[ -z "$ifname_wlan" ]] && [[ -z $(which iw) || -z $(iw phy) ]]; then
    # Default to 'no' when
    # - $ifname_wlan is blank
    # and either:
    # - iw is not installed
    # - no devices appear in `iw phy` output
    config_wlan_value="no"
fi
declare ifname_wlan_guess=$(iw dev | awk 'match($0, /Interface ([a-zA-Z0-9]+?)$/, matches) { print matches[1] }')
if confirm "Configure wireless interface?" "$config_wlan_value"; then
    prompt_require "Specify wireless network interface name" "${ifname_wlan:-"${ifname_wlan_guess:-"wlan0"}"}"
    ifname_wlan="$prompt_val"
    prompt_require "Specify wireless phy device name" "${phyname_wlan:-"phy0"}"
    phyname_wlan="$prompt_val"
    disabled_wlan=
else
    disabled_wlan=1
fi

declare ifname_bridge="${ifname_bridge:-"br-lan"}"
prompt_require "Specify LAN bridge interface" "$ifname_bridge"
ifname_bridge="$prompt_val"

prompt_require "Specify WAN network interface" "$ifname_wan"
ifname_wan="$prompt_val"

hwaddr_lan_orig="$hwaddr_lan"
if [[ -z "$hwaddr_lan_orig" ]]; then
    hwaddr_lan_orig=$(ip addr show "$ifname_lan" | awk '/ether / { print $2 }')
fi
prompt_require "Enter MAC address for the LAN interface" "$hwaddr_lan_orig" prompt_validator_macaddr
hwaddr_lan="$prompt_val"

# Between 0 and 255, used as the third octet in LAN IP addresses
# This assumes a subnet mask of 255.255.255.0 for the LAN network.
subnet_simple="15"

echo "ifname_lan=\"$ifname_lan\""       >  "$UNUM_ETC_DIR/extras.conf.sh"
echo "ifname_wlan=\"$ifname_wlan\""     >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "phyname_wlan=\"$phyname_wlan\""   >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "disabled_wlan=\"$disabled_wlan\"" >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "ifname_bridge=\"$ifname_bridge\"" >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "ifname_wan=\"$ifname_wan\""       >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "hwaddr_lan=\"$hwaddr_lan\""       >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "subnet_simple=\"$subnet_simple\"" >> "$UNUM_ETC_DIR/extras.conf.sh"
# ssid and passphrase are set in config_hostapd.sh -- save their values if
# they exist.
declare ssid passphrase
echo "ssid=\"$ssid\""                   >> "$UNUM_ETC_DIR/extras.conf.sh"
echo "passphrase=\"$passphrase\""       >> "$UNUM_ETC_DIR/extras.conf.sh"

# Source configuration values again-- be sure we have the latest values.
source "$UNUM_ETC_DIR/extras.conf.sh"

# Update the unum config.json file
echo '{
  "lan-if": "'"$ifname_bridge"'",
  "wan-if": "'"$ifname_wan"'"
}' > "$UNUM_ETC_DIR/config.json"

# This script embeds settings in system configuration files, using these values
# as sentinels (or markers) to automatically alter portions or entire configs.
declare minim_start_sentinel='### managed by minim ###'
declare minim_end_sentinel='### end managed by minim ###'

# Configure the network interfaces appropriately.
if [[ -f "/etc/dhcpcd.conf" ]]; then
    # System has dhcpcd installed, presume that's how it manages IP assignation
    # and modify accordingly.

    # Check for the sentinel, defined above
    conf_check=$(grep -n "$minim_start_sentinel" "/etc/dhcpcd.conf" | cut -d':' -f1 || :)
    end_check=$(grep -n "$minim_end_sentinel" "/etc/dhcpcd.conf" | cut -d':' -f1 || :)

    # Remove previous configuration embedded in /etc/dhcpcd.conf, if it's there
    if [[ ! -z "$conf_check" ]]; then
        # $conf_check contains the line number with the sentinel
        head "-n$(( conf_check - 1 ))" /etc/dhcpcd.conf > /etc/dhcpcd.conf.tmp
        if [[ ! -z "$end_check" ]]; then
            # $end_check contains line number with end sentinel
            declare -i dhcpcd_lines=$(wc -l /etc/dhcpcd.conf | cut -d' ' -f1)
            tail -n$(( dhcpcd_lines - end_check )) /etc/dhcpcd.conf >> /etc/dhcpcd.conf.tmp
        fi
    else
        # Otherwise just cat the whole file into our temp file
        cat /etc/dhcpcd.conf > /etc/dhcpcd.conf.tmp
    fi

    # Append 'managed by minim' block to temp dhcpcd.conf file
    echo "$minim_start_sentinel
# This section is autogenerated. DO NOT EDIT
interface $ifname_bridge
static ip_address=192.168.$subnet_simple.1/24
static routers=192.168.$subnet_simple.1" >> /etc/dhcpcd.conf.tmp
    echo "
interface $ifname_lan
static ip_address=
destination routers" >> /etc/dhcpcd.conf.tmp
    if (( ! disabled_wlan )) && [[ "$ifname_lan" != "$ifname_wlan" ]]; then
        echo "
interface $ifname_wlan
static ip_address=
destination routers" >> /etc/dhcpcd.conf.tmp
    fi
    echo "$minim_end_sentinel" >> /etc/dhcpcd.conf.tmp

    if [[ ! -f "/etc/dhcpcd.conf.pre-unum" ]]; then
        # Keep a backup of the original dhcpcd.conf
        mv /etc/dhcpcd.conf /etc/dhcpcd.conf.pre-unum
    fi

    # Replace the real /etc/dhcpcd.conf with the newly generated one
    mv /etc/dhcpcd.conf.tmp /etc/dhcpcd.conf

    service dhcpcd restart

elif [[ -f "/etc/network/interfaces" ]]; then
    # Configuring the interfaces with ifupdown and /etc/network/interfaces.

    echo "$minim_start_sentinel
# This file is autogenerated. DO NOT EDIT
auto lo
iface lo inet loopback

auto $ifname_wan
iface $ifname_wan inet dhcp" > /etc/network/interfaces.tmp
    bridge_interfaces=""
    if [[ "$ifname_lan" != "$ifname_wlan" ]]; then
        bridge_interfaces="$ifname_lan"
        echo "
iface $ifname_lan inet manual" >> /etc/network/interfaces.tmp
    fi
    if (( ! disabled_wlan )); then
        bridge_interfaces="$bridge_interfaces $ifname_wlan"
        echo "
allow-hotplug $ifname_wlan
iface $ifname_wlan inet manual" >> /etc/network/interfaces.tmp
    fi
    echo "
iface $ifname_bridge inet static
    address 192.168.$subnet_simple.1
    netmask 255.255.255.0
    network 192.168.$subnet_simple.0
    broadcast 192.168.$subnet_simple.255
    bridge_ports $bridge_interfaces" >> /etc/network/interfaces.tmp
    echo "$minim_end_sentinel" >> /etc/network/interfaces.tmp

    if [[ ! -f "/etc/network/interfaces.pre-unum" ]]; then
        # Keep a backup of the original interfaces file
        mv /etc/network/interfaces /etc/network/interfaces.pre-unum
    fi

    mv /etc/network/interfaces.tmp /etc/network/interfaces

    ifup "$ifname_bridge"

else
    # Otherwise platform is not directly supported.
    echo "warning: this system is unsupported, interfaces must be configured manually."
fi

if [[ "$hwaddr_lan_orig" != "$hwaddr_lan" ]]; then
    # MAC address changed, existing client cert is no longer any good.
    rm -fv /var/opt/unum/unum.key /var/opt/unum/unum.pem || :
fi
