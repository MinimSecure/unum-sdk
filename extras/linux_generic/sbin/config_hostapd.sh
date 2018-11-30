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
    echo "no wireless interface configured, not configuring hostapd"
    exit 0
fi

phydev=$phyname_wlan
hostapd_conf="$UNUM_ETC_DIR/hostapd-$phydev.conf"

mkdir -p $(dirname "$hostapd_conf")

cp "$UNUM_INSTALL_ROOT/extras/etc/hostapd.conf.base" "$hostapd_conf"

echo "interface=$ifname_wlan" >> "$hostapd_conf"

prompt_require "Specify wireless SSID" "MinimSecure" prompt_validator_ssid
echo "ssid=$prompt_val" >> "$hostapd_conf"

prompt_require "Specify wireless passphrase" "" prompt_validator_passphrase
echo "wpa_passphrase=$prompt_val" >> "$hostapd_conf"

# iwinfo looks here for the hostapd conf
ln -sf "$hostapd_conf" "/var/run/hostapd-$phydev.conf"
