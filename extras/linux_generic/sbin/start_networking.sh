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

wanifname="$ifname_wan"
lanifname="$ifname_lan"

wanip=$(awk '/^DHCPACK/ { print $3 }' <(dhclient -v 2>&1 || :))
if [[ "$wanip" != "" ]]; then
    echo "WAN IP address: $wanip"
    ifconfig "$wanifname" "$wanip"
else
    echo "Warning: dhclient failed to assign a WAN IP address, continuing anyway"
fi

echo "LAN IP address: 192.168.$subnet_simple.1"
ifconfig "$lanifname" 192.168.$subnet_simple.1 netmask 255.255.255.0
