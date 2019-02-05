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

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

if [[ ! -f "$UNUM_ETC_DIR/dnsmasq.conf" ]]; then
    $(dirname "$BASH_SOURCE")/config_dnsmasq.sh
fi

echo "Starting dnsmasq..."
dnsmasq -C "$UNUM_ETC_DIR/dnsmasq.conf" --log-facility="$UNUM_VAR_DIR/dnsmasq.log"
