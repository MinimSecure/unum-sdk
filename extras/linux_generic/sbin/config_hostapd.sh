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

if [[ "$1" == "--no-interactive" ]]; then
    interactively=0
fi

if (( disabled_wlan )); then
    echo "no wireless interface configured, not configuring hostapd"
    exit 0
fi

phydev=$phyname_wlan
hostapd_conf="$UNUM_ETC_DIR/hostapd-$phydev.conf"

mkdir -p $(dirname "$hostapd_conf")

cp "$UNUM_INSTALL_ROOT/extras/etc/hostapd.conf.base" "$hostapd_conf"

echo "interface=$ifname_wlan" >> "$hostapd_conf"

if [[ ! -z "$ifname_bridge" ]]; then
    echo "bridge=$ifname_bridge" >> "$hostapd_conf"
fi

prompt_require "Specify wireless SSID" "${ssid:-MinimSecure}" prompt_validator_ssid
ssid="$prompt_val"
echo "ssid=$ssid" >> "$hostapd_conf"

prompt_require "Specify wireless passphrase" "$passphrase" prompt_validator_passphrase
passphrase="$prompt_val"
echo "wpa_passphrase=$passphrase" >> "$hostapd_conf"

# Update saved ssid in extras.conf.sh
if grep -e '^ssid=' "$UNUM_ETC_DIR/extras.conf.sh" > /dev/null 2>&1; then
    sed -i -E 's:^ssid=.*:ssid="'"$ssid"'":' "$UNUM_ETC_DIR/extras.conf.sh"
else
    echo "ssid=\"$ssid\"" >> "$UNUM_ETC_DIR/extras.conf.sh"
fi
# Update saved passphrase in extras.conf.sh
if grep -e '^passphrase=' "$UNUM_ETC_DIR/extras.conf.sh" > /dev/null 2>&1; then
    sed -i -E 's:^passphrase=.*:passphrase="'"$passphrase"'":' "$UNUM_ETC_DIR/extras.conf.sh"
else
    echo "passphrase=\"$passphrase\"" >> "$UNUM_ETC_DIR/extras.conf.sh"
fi

# iwinfo looks here for the hostapd conf
ln -sf "$hostapd_conf" "/var/run/hostapd-$phydev.conf"
