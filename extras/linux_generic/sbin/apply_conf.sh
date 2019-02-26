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

extras_conf_sh="$UNUM_ETC_DIR/extras.conf.sh"
extras_conf_sh_tmp="$extras_conf_sh.tmp"

# Truncate or create the temp file.
echo > "$extras_conf_sh_tmp"
# Read from standard input, writing each line to the temp file.
while read ln; do
    # Handle ifname_lan specially; linux extras sends up $ifname_bridge
    # as the LAN interface name, so apply received configuration accordingly.
    if [[ "$ln" =~ ^ifname_lan= ]]; then
        # Keep existing value in $ifname_lan
        echo "ifname_lan=\"$ifname_lan\"" >> "$extras_conf_sh_tmp"
        # Set $ifname_bridge instead of $ifname_lan with the value received
        echo "${ln/ifname_lan/ifname_bridge}" >> "$extras_conf_sh_tmp"
        continue
    fi

    echo "$ln" >> "$extras_conf_sh_tmp"
    # Ensure that 'ssid' and 'passphrase' are still defined in the new config.
    if [[ "$ln" =~ ^ssid_wlan= ]]; then
        echo "ssid=\"\$ssid_wlan\"" >> "$extras_conf_sh_tmp"
    elif [[ "$ln" =~ ^passphrase_wlan= ]]; then
        echo "passphrase=\"\$passphrase_wlan\"" >> "$extras_conf_sh_tmp"
    fi
done

# Create a copy of the old configuration file.
cp -f "$extras_conf_sh" "$extras_conf_sh.old"
# Move the newly created file, replacing the old configuration file.
mv -f "$extras_conf_sh_tmp" "$extras_conf_sh"

# Restart everything to apply the new configuration.
minim-config --no-interactive
