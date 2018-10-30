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

# Common environment library script

# Source this file in your bash sessions to ease interaction with the
# installed Unum agent and related dependencies:
#     source "/path/to/unum_env.sh"

# Avoid repeated execution of this script; return early if the test
# succeeds (and the type is already defined)
test "$__unum_env_sourced" > /dev/null 2>&1 && return 0
declare __unum_env_sourced="$BASH_SOURCE"

[[ $(id -u) != "0" ]] && echo "This command must be run as root!" && exit 1

# Set up important paths, but only if they are not already set
if [[ -z "$UNUM_INSTALL_ROOT" ]]; then
    # Presume this file exists in '$UNUM_INSTALL_ROOT/extras/sbin'
    #     export UNUM_INSTALL_ROOT="/path/to/this/script/../.."
    export UNUM_INSTALL_ROOT=$(readlink -e "$(dirname "$BASH_SOURCE")/../..")
fi

export UNUM_ETC_DIR="/etc/opt/unum"
export UNUM_VAR_DIR="/var/opt/unum"

# Ensure that Unum and its dependencies have the correct library path.
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$UNUM_INSTALL_ROOT/lib/"
# Ensure the current PATH contains all the necessary binaries.
export PATH="$PATH:$UNUM_INSTALL_ROOT/bin:$UNUM_INSTALL_ROOT/extras/sbin"

uci() {
    $(which uci) -q $@
    return $?
}
export -f uci

valid_config() {
    declare -A required=( \
        ["network.wan.ifname"]="WAN Interface" \
        ["network.lan.ifname"]="LAN Interface" \
        ["network.lan.device"]="LAN Device" )
    declare -a errors
    for key in "${!required[@]}"; do
        if [ -z $(uci get "$key") ]; then
            errors+=("${required[$key]} ($key) is required, but not set")
        fi
    done
    if [[ -z "${errors[@]}" ]]; then
        return 0
    fi
    echo "Invalid configuration! Found these problems:"
    for err_msg in "${errors[@]}"; do
        echo "  $err_msg"
    done
    return 1
}
export -f valid_config

# Kills all processes matching the given input.
# Sends the SIGTERM signal, then SIGKILL after two seconds.
# This function waits until all processes are killed before returning.
killwait() {
    local pids=$(ps -A | grep $1 | awk 'ORS=" " { print $1 }')
    [[ -z "${pids## }" ]] && return 0
    kill $pids 2> /dev/null
    (sleep 3; kill -9 $pids 2> /dev/null || :) &
    wait $pids 2> /dev/null || :
    return 0
}
export -f killwait

# Helper function for iterating over multiple UCI config sections of the same
# underlying type.
#
# Usage: uci_iter <mapcmd> <keypattern>
#
#   uci_iter "echo 'got a value: '" wireless.@wifi-ifaces[#].device
#
# Any "#" character in <keypattern> is replaced with an incrementing value
# starting from 0 until an empty result is returned. For each non-empty record
# found, <map-cmd> is invoked with the value fetched from UCI as the first
# argument and the current iteration number and key as the second and third
# arguments.
#
# A function used as <map-cmd> should accept arguments as:
#   map-cmd <conf-value> <iter-index> <iter-key>
uci_iter() {
    local fn="$1"
    shift
    local i=0
    local val=
    local key=
    local section=
    # limit to 5 entries
    while (( i < 5 )); do
        key=${1/"#"/"$i"}
        section=$(sed -E 's/(\.[\w_]+?)$//' <<< "$key")
        # break if the section does not exist
        uci get "$section" > /dev/null || break
        val=$(uci get "$key" || :)
        $fn "$val" "$i" "$key" || :
        (( i++ ))
    done
}
export -f uci_iter

uci_bool() {
    case "$(tr A-Z a-z <<< "$1")" in
        y|1|on|yes) return 0
                    ;;
        n|0|off|no) return 1
                    ;;
        "")         return 2
                    ;;
        *)          return 3
                    ;;
    esac
}
export -f uci_bool

uci_exclude_true() {
    if ! uci_bool "$1"; then
        # Config value is considered "no"; do not filter this value
        echo "$2"
    fi
    return 0
}
export -f uci_exclude_true

# Determine the currently active agent profile that should be used.
if [[ -z "$UNUM_ACTIVE_PROFILE" ]]; then
    # Presume the first enabled agent profile should be used unless already set.
    first_active_agent=$(uci_iter uci_exclude_true "unum.@agent[#].disabled" | head -n1)
    export UNUM_ACTIVE_PROFILE=$(uci get "unum.@agent[$first_active_agent].profile")
    if [[ -z "$UNUM_ACTIVE_PROFILE" ]]; then
        echo "warning: unable to determine active profile, falling back to profile 'default'"
        export UNUM_ACTIVE_PROFILE="default"
    fi
fi

# mapcmd for determining the correct section for the active profile. Examples:
#     startup_section=$(uci_iter uci_find_active_section "unum.@startup[#]" | head -n1)
#     next_startup_section=$(uci_iter uci_find_active_section "unum.@next_startup[#]" | head -n1)
#     radio_sections=$(uci_iter uci_find_active_section "unum.@radio[#]" | tr '\n' ' ')
uci_find_active_section() {
    local val="$1"
    local idx="$2"
    local key="$3"
    local section=$(sed -E 's/(\.[\w_]+?)$//' <<< "$key")
    if [[ $(uci get "$section.profile") == "$UNUM_ACTIVE_PROFILE" ]]; then
        if ! uci_bool $(uci get "$section.disabled"); then
            echo "$section"
        fi
    fi
    return 0
}
export -f uci_find_active_section
