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

ifname_wan_orig="$ifname_wan"
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

# Run configuration helper
if [[ ! -f "$UNUM_ETC_DIR/extras.conf.sh" ]]; then
    # Extras configuration file does not exist, presume this is a first-time start
    config_interfaces.sh
    config_hostapd.sh
    config_dnsmasq.sh
fi

if [[ "$ifname_wan" != "$ifname_wan_orig" ]]; then
    echo "Setting WAN interface to $ifname_wan"
    sed -i -e 's/ifname_wan=.*/ifname_wan='"$ifname_wan"'/' "$UNUM_ETC_DIR/extras.conf.sh"
fi
if [[ "$ifname_lan" != "$ifname_lan_orig" ]]; then
    echo "Setting LAN interface to $ifname_lan"
    sed -i -e 's/ifname_lan=.*/ifname_lan='"$ifname_wan"'/' "$UNUM_ETC_DIR/extras.conf.sh"
fi

# Source these again explicitly, in case the values have changed above
source "$UNUM_ETC_DIR/extras.conf.sh"

stop_all.sh

config_routing.sh

rfkill unblock wifi || :
killwait wpa_supplicant || :

# Set MAC address on the LAN interface
ifconfig "$ifname_lan" down
ip link set "$ifname_lan" address "$hwaddr_lan"
ifconfig "$ifname_lan" up

start_hostapd.sh || \
    echo "Warning: failed to start hostapd, continuing without wireless anyway"
start_dnsmasq.sh
start_unum.sh
