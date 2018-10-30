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

# Configures the network interfaces

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

valid_config || exit 1

wanifname=$(uci get network.wan.ifname)
lanifname=$(uci get network.lan.ifname)
landevice=$(uci get network.lan.device || :)
if [ -z "$landevice" ]; then
    echo "Warning: defaulting network.lan.device to $lanifname"
    landevice="$lanifname"
    uci set network.lan.device=$lanifname
fi

if ! ifconfig "$lanifname"; then
    brctl addbr "$lanifname"
    if [[ "$landevice" != "$lanifname" ]]; then
        brctl addif "$lanifname" "$landevice"
    fi
    ifconfig "$lanifname" up
fi

if [[ $(uci get network.wan.proto) == "dhcp" ]]; then
    wanip=$(awk '/^DHCPACK/ { print $3 }' <(dhclient -v 2>&1 || :))
    if [[ "$wanip" != "" ]]; then
        echo "WAN IP address: $wanip"
        ifconfig "$wanifname" "$wanip"
    else
        echo "Warning: dhclient failed to assign a WAN IP address, continuing anyway"
    fi
fi

#todo: configure loopback

lanip=$(uci get network.lan.ipaddr)
lannetmask=$(uci get network.lan.netmask)

echo "LAN IP address: $lanip"
ifconfig "$lanifname" "$lanip" netmask "$lannetmask"

uci set network.lan.up=1
uci set network.wan.up=1
uci set network.loopback.up=1
uci commit network
