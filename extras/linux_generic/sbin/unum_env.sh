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

source "$UNUM_INSTALL_ROOT/.installed"

export UNUM_ETC_DIR="${install_etc_dir:-/etc/opt/unum}"
export UNUM_VAR_DIR="${install_var_dir:-/var/opt/unum}"

if [[ "$LD_LIBRARY_PATH" != *$UNUM_INSTALL_ROOT* ]]; then
    export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$UNUM_INSTALL_ROOT/lib/"
fi
if [[ "$PATH" != *$UNUM_INSTALL_ROOT* ]]; then
    export PATH="$PATH:$UNUM_INSTALL_ROOT/bin:$UNUM_INSTALL_ROOT/extras/sbin"
fi

[[ -f "$UNUM_ETC_DIR/extras.conf.sh" ]] && source "$UNUM_ETC_DIR/extras.conf.sh"

# Kills all processes matching the given input.
# Sends the SIGTERM signal, then SIGKILL after two seconds.
# This function waits until all processes are killed before returning.
killwait() {
    local pids=$(ps axco pid,command | grep $1 | awk 'ORS=" " { print $1 }')
    [[ -z "${pids## }" ]] && return 0
    kill $pids 2> /dev/null
    (sleep 3; kill -9 $pids 2> /dev/null || :) &
    wait $pids 2> /dev/null || :
    return 0
}

declare -i interactively=1
declare prompt_val
prompt() {
    prompt_val="$2"
    if ! (( interactively )); then
        return 0
    fi
    echo -n "---> $1 [$prompt_val]: "
    read prompt_val
    if [[ -z "$prompt_val" ]]; then
        prompt_val="$2"
    fi
    return 0
}
confirm() {
    prompt "$1" "${2:-no}"
    if [[ "$prompt_val" =~ ^[Yy](es)? ]]; then
        return 0
    fi
    return 1
}
prompt_validator_nonempty() {
    if [[ -z "$1" ]]; then
        return 1
    fi
    return 0
}
prompt_validator_macaddr() {
    if [[ "$1" =~ ^[a-f0-9]{2}:[a-f0-9]{2}:[a-f0-9]{2}:[a-f0-9]{2}:[a-f0-9]{2}:[a-f0-9]{2}$ ]]; then
        return 0
    fi
    return 1
}
prompt_validator_ssid() {
    if (( ${#1} > 32 )); then
        echo "ssid cannot be longer than 32 characters"
        return 1
    elif (( ${#1} == 0 )); then
        echo "ssid cannot be empty"
        return 1
    fi
    return 0
}
prompt_validator_passphrase() {
    if (( ${#1} < 8 )); then
        echo "passphrase must be at least 8 characters"
        return 1
    fi
    return 0
}
prompt_require() {
    prompt_val="$2"
    prompt_validator="$3"
    if [[ -z "$prompt_validator" ]]; then
        prompt_validator=prompt_validator_nonempty
    fi
    while true; do
        prompt "$1" "$prompt_val"
        if $prompt_validator "$prompt_val"; then
            return 0
        fi
    done
}
