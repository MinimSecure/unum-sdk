#!/usr/bin/env bash
# Copyright 2019 Minim Inc
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

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

printopt() {
    key="$1"
    val="${2:-"${!key}"}"
    echo "$key=\"$val\""
    return 0
}

cidr_to_mask() {
    local -i cidr_mask=$1
    local -i full_octets=$(( cidr_mask / 8 ))
    local -i remaining_octet=$(( cidr_mask % 8 ))
    local -i iterv
    local -i itern
    for itern in 0 1 2 3; do
        if (( full_octets > 0 )); then
            full_octets=$(( full_octets - 1 ))
            iterv=8
        else
            iterv=$(( remaining_octet ))
            remaining_octet=0
        fi
        printf '%d' $(( 256 - (1 << (8 - iterv)) ))
        if (( itern < 3 )); then
            printf '.'
        else
            printf '\n'
        fi
    done
    return 0
}

ipcidr_for() {
    ip addr show "$1" | awk '/inet6/ { next }; /inet/ { print $2 }'
    return 0
}

regulatory_country() {
    iw reg get | awk 'match($0, /^country ([A-Z]{2})/, matches) { print matches[1] }'
    return 0
}

channel_for() {
    iwinfo "$1" info | awk 'match($0, /Channel: ([0-9]{1,3}) /, matches) { print matches[1] }'
}

hwmode_for() {
    local hwmode=$(iwinfo "$1" info | awk 'match($0, /HW Mode.*?: (.*)$/, matches) { print matches[1] }')
    if [[ "$hwmode" =~ ^802.11[a-z]*g[a-z]*$ ]]; then
        echo "11g"
    else
        echo "11a"
    fi
    return 0
}

ipcidr_lan=$(ipcidr_for "$ifname_bridge")
ipaddr_lan=$(echo "$ipcidr_lan" | cut -d'/' -f1)
subnet_lan=$(cidr_to_mask $(echo "$ipcidr_lan" | cut -d'/' -f2))

# WAN interface fields
printopt "ifname_wan"

# LAN interface fields
# Note that the bridge interface is actually used for the LAN interface name.
printopt "ifname_lan" "$ifname_bridge"
printopt "ipaddr_lan"
printopt "subnet_lan"

# Wireless adapter fields
printopt "ifname_wlan"
printopt "phyname_wlan"
printopt "ssid_wlan" "$ssid"
printopt "passphrase_wlan" "$passphrase"
if (( disabled_wlan )); then
    # Mark the wireless adapter as disabled if it is not configured
    printopt "disabled_wlan" "1"
else
    printopt "channel_wlan" $(channel_for "$ifname_wlan")
    printopt "hwmode_wlan" $(hwmode_for "$ifname_wlan")
    printopt "country_wlan" $(regulatory_country)
fi
