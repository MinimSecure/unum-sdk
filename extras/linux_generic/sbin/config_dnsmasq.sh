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

# Dnsmasq management script

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

dnsmasq_conf="$UNUM_ETC_DIR/dnsmasq.conf"

if [[ ! -f "$dnsmasq_conf" ]]; then
    cp "$UNUM_INSTALL_ROOT/extras/etc/dnsmasq.conf.base" "$dnsmasq_conf"

    echo "dhcp-range=192.168.$subnet_simple.100,192.168.$subnet_simple.150,12h" >> "$dnsmasq_conf"
    echo "interface=lo,$ifname_bridge" >> "$dnsmasq_conf"
fi
