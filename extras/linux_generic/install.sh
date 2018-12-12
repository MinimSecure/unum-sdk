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

# Generalized installation script that can be used to automate the installation
# of different parts of the "extras" bundle.

set -eo pipefail

[[ $(id -u) != "0" ]] && \
    echo "This command must be run as root! Re-run as root with sudo:" && \
    echo "    sudo $0 $@" && \
    exit 1

usage() {
    echo "Usage: $0 [--no-interactive|--uninstall|--purge]"
    echo "       $0 [--[no-]aio] [--[no-]profile] [--[no-]extras] [--[no-]init]"
    if [[ "$1" == "-v" ]]; then
        echo
        echo "Automatically or interactively install various features included in the"
        echo "Unum extras."
        echo
        echo "Modes:"
        echo "  --no-interactive   Do not prompt for user input. Use this mode with"
        echo "                     the uninstall mode or with options (listed below)"
        echo "  --uninstall        Uninstall the Unum agent and related installed files"
        echo "  --purge            Uninstall and remove configuration files, too"
        echo
        echo "Options:"
        echo "  --[no-]profile     Install (or do not) a script in /etc/profile.d that"
        echo "                     adds the unum command and extras, if enabled, to login"
        echo "                     paths."
        echo "  --[no-]init        Install (or do not) init scripts for the current"
        echo "                     system, automatically detected."
        echo "  --[no-]extras      Install (or do not) extra scripts that manage various"
        echo "                     aspects of a Linux router."
        echo "  --[no-]aio         Install (or do not) the unum 'all-in-one' service and"
        echo "                     minim-config utility."
    fi
}

declare -i interactively=1
declare -r working_dir=$(dirname "$BASH_SOURCE")
declare -r install_dir=$(readlink -e "$working_dir/..")
declare -r dist_dir="$install_dir/dist"
declare -r extras_dir="$install_dir/extras"

declare install_etc_dir="/etc/opt/unum"
declare install_var_dir="/var/opt/unum"

declare -i install_systemd_service=0
declare -i install_extras=0
declare -i install_profile=0
declare -i install_aio=0

declare -i uninstall=0
declare -i purge=0

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
confirm_install_dirs() {
    while true; do
        echo "storing config files in $install_etc_dir"
        echo "log and temp runtime storage in $install_var_dir"

        if confirm "are these correct?" "yes"; then
            break
        fi

        interactive_set_dirs
    done
    return 0
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
        --purge)
            uninstall=1
            purge=1
            ;;
        --help|-h|-help)
            usage -v
            exit 0
            ;;
        *)
            echo "unsupported option: $opt"
            usage
            exit 1
            ;;
    esac
done

# Handle uninstall
if (( uninstall )); then
    # Set install directories if Unum was not installed with this script.
    if [[ ! -f "$install_dir/.installed" ]]; then
        echo "warning: running --uninstall without previous installation"
        if (( interactively )); then
            confirm_install_dirs
        fi
    fi
    continue_value="no"
    if ! (( interactively )); then
        continue_value="yes"
    fi
    if confirm "are you sure you want to remove the current unum install?" "$continue_value"; then
        declare purge_value="no"
        if (( purge )); then
            purge_value="yes"
        fi
        if confirm "delete configuration files?" "$purge_value"; then
            rm -rfv "$install_etc_dir" || :
        fi
        rm -rfv "$install_var_dir" || :
        rm -rfv "$install_dir"|| :
        rm -fv /etc/profile.d/unum.sh || :
        rm -fv /etc/systemd/system/unum.service || :
        rm -fv /usr/bin/minim-config || :
        mv -fv /etc/default/hostapd.pre-unum /etc/default/hostapd || :
        mv -fv /etc/default/dnsmasq.pre-unum /etc/default/dnsmasq || :
        mv -fv /etc/dhcpcd.conf.pre-unum /etc/dhcpcd.conf || :
    else
        echo "did not uninstall unum, exiting without making any changes"
    fi

    exit 0
fi


# Determine full install configuration first

if [[ "$install_dir" != "/opt/unum" ]]; then
    echo "detected nonstandard install directory '$install_dir'"
    if ! (( interactively )); then
        echo "shell is running in non-interactive, carrying on with defaults"
    else
        interactive_set_dirs
    fi
fi

[[ -f "$install_dir/.installed" ]] && source "$install_dir/.installed"

# Allow the user to change the directories before continuing
confirm_install_dirs

# Confirm installation of unum "all-in-one" and minim-config
declare aio_value="no"
if (( install_aio )); then
    aio_value="yes"
fi
if confirm "install unum 'all-in-one' and minim-config management utility?" "$aio_value"; then
    install_aio=1
fi

# Confirm installation of systemd service for unum
# If $install_aio is enabled, a service for unum-aio will also be installed.
declare systemd_value="no"
if (( install_systemd_service )); then
    systemd_value="yes"
fi
if [[ -d "/etc/systemd" ]] && confirm "detected systemd, install systemd service?" "$systemd_value"; then
    install_systemd_service=1
fi

# Confirm installation of login shell profile script
declare profile_value="no"
if (( install_profile )); then
    profile_value="yes"
fi
if confirm "install profile.d script?" "$profile_value"; then
    install_profile=1

    # Confirm installation of "extras" on login shell PATH.
    # These are the scripts bundled with unum in /opt/unum/extras/sbin.
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

declare unum_sh_path="$extras_dir/etc/profile.d/unum.sh"
if [[ ! -f "$unum_sh_path.orig" ]]; then
    mv "$unum_sh_path" "$unum_sh_path.orig"
fi
sed -e 's:/opt/unum:'"$install_dir"':g' "$unum_sh_path.orig" > "$unum_sh_path"

# Add script in /etc/profile.d that will be sourced in login shells.
# This script enables regular users to use unum directly.
if (( install_profile )); then
    # Add the extras/sbin folder to the profile.d script.
    if (( install_extras )); then
        echo "adding extras to login shells' PATH"
        echo 'export PATH="$PATH:'"$working_dir"'/sbin"' >> "$unum_sh_path"
    fi
    echo "installing profile.d script /etc/profile.d/unum.sh"
    ln -sf "$unum_sh_path" "/etc/profile.d/unum.sh"
fi

# Install default unum configuration file.
echo "unum config file is $install_etc_dir/config.json"
cp -f "$dist_dir/etc/opt/unum/config.json" "$install_etc_dir/config.json"

# Handle all-in-one related installation.
# Expects hostapd, dnsmasq, and /etc/default to be used
if (( install_aio )); then
    ln -sf "$extras_dir/sbin/minim-config" "/usr/bin/minim-config"
    echo "installed minim-config: /usr/bin/minim-config"

    declare mod_check
    if [[ -f "/etc/default/dnsmasq" ]]; then
        mod_check=$(awk '/^DNSMASQ_OPTS=.*unum.*$/ { print "1" }' /etc/default/dnsmasq)
        if [[ -z "$mod_check" ]]; then
            cp -f /etc/default/dnsmasq /etc/default/dnsmasq.pre-unum
            echo 'DNSMASQ_OPTS="--conf-file='"$install_etc_dir"'/dnsmasq.conf --log-facility='"$install_var_dir"'/dnsmasq.log"' >> /etc/default/dnsmasq
            echo "updated /etc/default/dnsmasq to use generated config"
        fi
    else
        echo "warning: did not automatically modify /etc/default/dnsmasq (it does not exist!)"
    fi

    if [[ -f "/etc/default/hostapd" ]]; then
        mod_check=$(awk '/^DAEMON_CONF.*unum.*$/ { print "1" }' /etc/default/hostapd)
        if [[ -z "$mod_check" ]]; then
            cp -f /etc/default/hostapd /etc/default/hostapd.pre-unum
            sed -i -E 's:^[#]?DAEMON_CONF.*:DAEMON_CONF="'"$install_etc_dir"'/hostapd-phy0.conf":' /etc/default/hostapd
            echo 'DAEMON_OPTS="-dd -t -f '"$install_var_dir"'/hostapd.log"' >> /etc/default/hostapd
            echo "updated /etc/default/hostapd to use generated config"
        fi
    else
        echo "warning: did not automatically modify /etc/default/hostapd (it does not exist!)"
    fi
fi

# Install systemd service for unum, and unum-aio if enabled.
if (( install_systemd_service )); then
    cp -f "$extras_dir/etc/systemd/unum.service" "/etc/systemd/system/unum.service"
    (( install_aio )) && cp -f "$extras_dir/etc/systemd/unum-aio.service" "/etc/systemd/system/unum-aio.service"
    if which systemctl > /dev/null 2>&1; then
        # this fails when ran inside a docker container
        systemctl daemon-reload > /dev/null 2>&1 || :
    fi
    echo "installed systemd service: /etc/systemd/system/unum.service"
    (( install_aio )) && echo "installed systemd service: /etc/systemd/system/unum-aio.service"
fi

# Add a file with the configured install settings for use in uninstallation.
echo "install_etc_dir=\"$install_etc_dir\"" >  "$install_dir/.installed"
echo "install_var_dir=\"$install_var_dir\"" >> "$install_dir/.installed"


# Post-install

if (( install_aio )); then
    echo
    echo "Unum agent installed but additional configuration is required!"
    echo
    echo 'Use the `minim-config` utility to complete this configuration.'
    echo 'In your terminal, run:'
    echo '    minim-config'
    echo

elif (( interactively )); then
    echo "Installation complete!"
    if (( install_profile )); then
        echo "Restart your terminal or run this command to refresh your current shell's"
        echo "environment:"
        echo "    source /etc/profile.d/unum.sh"
    fi
fi
