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

# Main entrypoint for starting the Unum agent and related services.

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

usage() {
    echo "Usage: $0 [-W <WAN ifname>] [-L <LAN ifname>]"
    echo
    echo "Starts the Unum agent and all of its dependencies."
    echo
    echo "This command configures and starts dnsmasq, hostapd, and iptables. After all"
    echo "the necessary services are running, the Unum agent is started. This command"
    echo "will exit with a successful exit code (0) upon successful startup."
    echo
    echo "Options:"
    echo "  -W <ifname>  Specify the WAN interface name."
    echo "  -L <ifname>  Specify the LAN interface name."
    echo "  -h           Print this help text."
}

# Read interface names from stored UCI configuration.
ifname_wan=$(uci get network.wan.ifname)
ifname_wan_orig="$ifname_wan"
ifname_lan=$(uci get network.lan.ifname)
ifname_lan_orig="$ifname_lan"

while getopts 'W:L:I:P:h' opt; do
    case "$opt" in
        W ) ifname_wan="$OPTARG"
            ;;
        L ) ifname_lan="$OPTARG"
            ;;
        h ) usage
            exit 0
            ;;
        * ) usage
            exit 1
            ;;
    esac
done

if [[ "$ifname_wan" != "$ifname_wan_orig" ]]; then
    echo "Setting WAN interface to $ifname_wan"
    uci set network.wan.ifname=$ifname_wan
fi
if [[ "$ifname_lan" != "$ifname_lan_orig" ]]; then
    echo "Setting LAN interface to $ifname_lan"
    uci set network.lan.ifname=$ifname_lan
fi

# Commit changes, even if we made none.
uci commit network

valid_config || exit 1

stop_all.sh

start_networking.sh
start_hostapd.sh || \
    echo "Warning: failed to start hostapd, continuing without wireless anyway"
start_dnsmasq.sh
start_routing.sh

echo "Starting unum..."
nohup unum -d
