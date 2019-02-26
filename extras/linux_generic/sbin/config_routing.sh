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

# Configures the system to behave as a router

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

ifname_wan="$ifname_wan"
ifname_bridge="$ifname_bridge"

echo "Enabling ipv4 forwarding"
echo 1 > /proc/sys/net/ipv4/ip_forward

echo "Clearing all existing iptables rules..."
iptables -F

echo "Adding iptables rules..."
# Enable NAT for the WAN interface
iptables -t nat -A POSTROUTING -s "192.168.$subnet_simple.0/24" -o "$ifname_wan" -j MASQUERADE

# Accept forwarded packets on the WAN interface
iptables -A FORWARD -s "192.168.$subnet_simple.0/24" -o "$ifname_wan" -j ACCEPT
iptables -A FORWARD -i "$ifname_wan" -d "192.168.$subnet_simple.0/24" -m state --state ESTABLISHED,RELATED -j ACCEPT

# Accept all inbound packets on the LAN interface
iptables -A INPUT -i "$ifname_bridge" -j ACCEPT
# Only allow established or related inbound packets on the WAN interface
iptables -A INPUT -i "$ifname_wan" -m state --state ESTABLISHED,RELATED -j ACCEPT

# Accept all output packets from anywhere
iptables -A OUTPUT -j ACCEPT
