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

# Hostapd management script

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

if [[ -z "$phyname_wlan" ]]; then
    echo "no wireless interface configured, not starting hostapd"
    exit 0
fi

phydev=$phyname_wlan
hostapd_conf="$UNUM_VAR_DIR/hostapd-$phydev.conf"

if [[ ! -f "$hostapd_conf" ]]; then
    mkdir -p $(dirname "$hostapd_conf")

    cp "$UNUM_INSTALL_ROOT/extras/etc/hostapd.conf.base" "$hostapd_conf"

    echo "interface=$ifname_wlan" >> "$hostapd_conf"

    prompt "Specify wireless SSID" "MinimSecure"
    echo "ssid=$prompt_val" >> "$hostapd_conf"

    prompt_val=
    while [[ -z "$prompt_val" ]]; do
        prompt "Specify wireless passphrase"
    done
    echo "wpa_passphrase=$prompt_val" >> "$hostapd_conf"
fi

for ifn in $(iw dev | grep Interf | cut -d' ' -f2); do
    if [[ -z "$ifn" ]]; then
        continue
    fi
    echo "Removing existing WLAN interface $ifn"
    iw dev "$ifn" del
    sleep 1
done

rfkill unblock wifi || :
killwait wpa_supplicant || :

echo "Creating WLAN interface $ifname_wlan"
iw phy $phydev interface add $ifname_wlan type monitor

echo "Starting hostapd..."
hostapd -B "$hostapd_conf"
