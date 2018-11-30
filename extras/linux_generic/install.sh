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

[[ $(id -u) != "0" ]] && \
    echo "This command must be run as root! Re-run as root with sudo:" && \
    echo "    sudo $0 $@" && \
    exit 1

declare -i interactively=1
declare -r working_dir=$(dirname "$BASH_SOURCE")
declare -r install_dir=$(readlink -e "$working_dir/..")
declare -r dist_dir="$install_dir/dist"

declare install_etc_dir="/etc/opt/unum"
declare install_var_dir="/var/opt/unum"

declare -i install_systemd_service=0
declare -i install_extras=0
declare -i install_profile=0
declare -i install_aio=0

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

interactive_set_dirs() {
    prompt "Specify configuration directory" $install_etc_dir
    install_etc_dir="$prompt_val"
    prompt "Specify log file directory" $install_var_dir
    install_var_dir="$prompt_val"
}

# Handle command line options

for opt in $@; do
    case "$opt" in
        --no-interactive)
            interactively=0
            ;;
        --extras)
            install_extras=1
            ;;
        --no-extras)
            install_extras=0
            ;;
        --no-init)
            install_systemd_service=0
            ;;
        --init)
            install_systemd_service=1
            ;;
        --no-profile)
            install_profile=0
            ;;
        --profile)
            install_profile=1
            ;;
        --aio)
            install_aio=1
            ;;
        --no-aio)
            install_aio=0
            ;;
        --uninstall)
            uninstall=1
            ;;
    esac
done

# Determine full install configuration first

if [[ "$install_dir" != "/opt/unum" ]]; then
    echo "detected nonstandard install directory '$install_dir'"
    if ! [[ -z "$PS1" ]]; then
        echo "shell is running in non-interactive, carrying on with defaults"
    else
        interactive_set_dirs
    fi
fi

[[ -f "$install_dir/.installed" ]] && source "$install_dir/.installed"

if (( uninstall )); then
    continue_value="no"
    if ! (( interactively )); then
        continue_value="yes"
    fi
    if confirm "are you sure you want to remove the current unum install?" "$continue_value"; then
        if confirm "delete configuration files?" "yes"; then
            rm -rfv "$install_etc_dir" || :
        fi
        rm -rfv "$install_var_dir" || :
        rm -rfv "$install_dir"|| :
        rm -fv /etc/profile.d/unum.sh || :
        rm -fv /etc/systemd/system/unum.service || :
    else
        echo "did not remove unum"
    fi

    exit 0
fi

while true; do
    echo "storing config files in $install_etc_dir"
    echo "log and temp runtime storage in $install_var_dir"

    if confirm "are these correct?" "yes"; then
        break
    fi

    interactive_set_dirs
done

declare systemd_value="no"
if (( install_systemd_service )); then
    systemd_value="yes"
fi
if [[ -d "/etc/systemd" ]] && confirm "detected systemd, install systemd service?" "$systemd_value"; then
    install_systemd_service=1
fi

declare profile_value="no"
if (( install_profile )); then
    profile_value="yes"
fi
if confirm "install profile.d script?" "$profile_value"; then
    install_profile=1

    declare extras_value="no"
    if (( install_extras )); then
        extras_value="yes"
    fi
    if confirm "add optional bash helper scripts to login shells' PATH?" "$extras_value"; then
        install_extras=1
    else
        install_extras=0
    fi
else
    install_profile=0
fi

# Perform the installation according to spec

mkdir -p "$install_etc_dir"
mkdir -p "$install_var_dir"

echo "install_etc_dir=\"$install_etc_dir\"" >  "$install_dir/.installed"
echo "install_var_dir=\"$install_var_dir\"" >> "$install_dir/.installed"

declare unum_sh_path="$dist_dir/etc/profile.d/unum.sh"
if [[ ! -f "$unum_sh_path.orig" ]]; then
    mv "$unum_sh_path" "$unum_sh_path.orig"
fi
sed -e 's:/opt/unum:'"$install_dir"':g' "$unum_sh_path.orig" > "$unum_sh_path"

if (( install_profile )); then
    if (( install_extras )); then
        echo "adding extras to login shells' PATH"
        echo 'export PATH="$PATH:'"$working_dir"'/sbin"' >> "$unum_sh_path"
    fi
    echo "installing profile.d script /etc/profile.d/unum.sh"
    ln -sf "$unum_sh_path" "/etc/profile.d/unum.sh"
fi
echo "unum config file is $install_etc_dir/config.json"
cp -f "$dist_dir/etc/opt/unum/config.json" "$install_etc_dir/config.json"

if (( install_systemd_service )); then
    cp -f "$dist_dir/etc/systemd/system/unum.service" "/etc/systemd/system/unum.service"
    cp -f "$dist_dir/etc/systemd/system/unum-aio.service" "/etc/systemd/system/unum-aio.service"
    if which systemctl > /dev/null 2>&1; then
        # this fails when ran inside a docker container
        systemctl daemon-reload > /dev/null 2>&1 || :
    fi
    echo "installed systemd service: /etc/systemd/system/unum.service"
    echo "installed systemd service: /etc/systemd/system/unum-aio.service"
fi

if (( interactively )); then
    echo "installation complete!"
    if (( install_profile )); then
        echo "restart your terminal or run this command to refresh your current shell's"
        echo "environment:"
        echo "    source /etc/profile.d/unum.sh"
    fi
fi
