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

# Hostapd/wpa_supplicant management script

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

valid_config || exit 1

profile="${UNUM_ACTIVE_PROFILE:-default}"
radio="${1:-radio0}"

phydev=$(uci get "wireless.${radio}.phy")
hostapd_conf="$UNUM_VAR_DIR/hostapd-$phydev.conf"

# TODO: generate key if blank
# TODO: generate key if unum.default_next_startup.generate_passphrase=y
# TODO: replace default_ with ${profile}_
# TODO: check disabled everywhere
# TODO: start wpa_supplicant if necessary
# TODO: create proper conf files for both hostapd and wpa_supplicant
# TODO: final validation of all documented options

mkdir -p $(dirname "$hostapd_conf")

if uci_bool $(uci get unum.default_startup.replace_wifi_interface); then
    for ifn in $(iw dev | grep Interf | cut -d' ' -f2); do
        if [[ -z "$ifn" ]]; then
            continue
        fi
        echo "Removing existing WLAN interface $ifn"
        iw dev "$ifn" del
        sleep 1
    done
fi

# Get current wireless settings.
chan=$(uci get "wireless.${radio}.channel" || echo "auto")
wlan_ifname=$(uci get "wireless.${radio}.ifname")
network=$(uci get "wireless.${profile}_${radio}.network")
lan_ifname=$(uci get "network.$network.ifname")
enc=$(uci get "wireless.${profile}_${radio}.encryption")
key=$(uci get "wireless.${profile}_${radio}.key")
ssid=$(uci get "wireless.${profile}_${radio}.ssid")

# Default to 802.11g/n (2.4GHz)
hwmode="g"
case $(uci get "wireless.${radio}.hwmode") in
    11a ) hwmode="a" # 802.11n/ac (5GHz)
          ;;
    11g ) hwmode="g" # 802.11g/n (2.4GHz)
          ;;
    *   ) echo "warning: unknown hwmode specified, falling back to hwmode=$hwmode"
          ;;
esac

# Channel width for 802.11n and 802.11ac
htmode=$(uci get "wireless.${radio}.htmode")
# Force clients to use HT or VHT modes
force_ht=0
require_mode=$(uci get "wireless.${radio}.require_mode")
if [[ "$hwmode" == "g" ]]; then
    case "$require_mode"  in
        g ) # 'g' is the lowest supported mode anyway, no need to force_ht
            ;;
        "") # empty value is valid but ignored
            ;;
        n ) force_ht=1  # Allow only 802.11n clients
            ;;
        * ) echo "warning: require_mode must be empty or one of 'g' or 'n', got '$require_mode'"
            echo "ignoring require_mode option, force_ht=$force_ht"
            ;;
    esac
elif [[ "$hwmode" == "a" ]]; then
    case "$require_mode" in
        n ) # 'n' is the lowest supported mode anyway, no need to force_ht
            ;;
        "") # empty value is valid but ignored
            ;;
        ac) force_ht=1  # Allow only 802.11ac clients
            ;;
        * ) echo "warning: require_mode must be empty or one of 'n' or 'ac', got '$require_mode'"
            echo "ignoring require_mode option, force_ht=$force_ht"
            ;;
    esac
fi

# Ignore requests to broadcast SSID.
# Setting this option to 1 indicates an empty ssid is broadcast, ignoring
# any probes. Setting it to 2 indicates a clear ASCII ssid be broadcast,
# and may be required for some legacy clients that don't' support empty ssid.
ignore_broadcast_ssid=0
if uci get "wireless.${radio}.hidden" | uci_bool; then
    ignore_broadcast_ssid=1
fi

# Workaround to mitigate key re-installation attacks (ie. KRACK attack)
wpa_disable_eapol_key_retries=1
# Additional measure to mitigate key re-installation attacks against TDLS
tdls_prohibit=1

case "$enc" in
	wpa2*|*psk2*)
		wpa=2
        ;;
	wpa*|*psk*)
	    echo "warning: WPA-PSK is known to be insecure and cannot support full 802.11n speeds"
		wpa=1
	    ;;
	*mixed*)
	    echo "warning: using known insecure mixed mode for wireless auth. WPA2-PSK is recommended"
		wpa=3
	    ;;
	*)
	    echo "warning: wireless AP is configured with open authentication. WPA2-PSK is recommended"
		wpa=0
	    ;;
esac

wpa_cipher="TKIP"
rsn_cipher=
case "$enc" in
    *tkip+ccmp*|*ccmp+tkip*)
        wpa_cipher="TKIP"
        rsn_cipher="CCMP"
        ;;
    *tkip*)
        wpa_cipher="TKIP"
        rsn_cipher=
        ;;
    *ccmp*)
        # Note that Windows clients may run into problems using the CCMP wpa_cipher
        wpa_cipher="CCMP"
        rsn_cipher="CCMP"
        ;;
    *)
        echo "warning: did not match configured wireless encryption scheme '$enc' to known ciphers"
        echo "using defaults in hostapd conf: 'wpa_pairwise=$wpa_cipher', 'rsn_pairwise=$rsn_cipher'"
        ;;
esac

if [[ "$rsn_cipher" != "CCMP" ]]; then
    echo "warning: 802.11n clients require CCMP authentication mode for full transfer speeds"
    echo "value specified in hostapd conf: 'rsn_pairwise=$rsn_cipher'"
fi

driver="nl80211"
adapter_type=$(uci get "wireless.${radio}.type" || :)
case "$adapter_type" in
    *80211)
        driver="nl80211"
        ;;
    broadcom|bcm*|brcm*)
        driver="broadcom"
        ;;
    *)
        echo "warning: unknown adapter type '$adapter_type', expected 'broadcom' or 'mac80211'"
        echo "defaulting to 'driver=$driver'"
        ;;
esac

echo "interface=$wlan_ifname" > "$hostapd_conf"
# If the WLAN interface is different from the LAN interface, presume hostapd
# should create a bridge interface if one does not exist, and add the WLAN
# interface to it.
if [[ "$wlan_ifname" != "$lan_ifname" ]]; then
    echo "bridge=$lan_ifname" >> "$hostapd_conf"
fi

echo "driver=$driver" >> "$hostapd_conf"
# Always enable 802.11n
echo "ieee80211n=1"  >> "$hostapd_conf"
if [[ "$hwmode" == "a" && "$htmode" =~ ^VHT ]]; then
    # 802.11ac and a VHT channel are configured, enable ac support
    echo "ieee80211ac=1"  >> "$hostapd_conf"
fi
echo "hw_mode=$hwmode" >> "$hostapd_conf"

# Required for proper HT (.11n) and VHT (.11ac)
echo "wmm_enabled=1" >> "$hostapd_conf"

# Create the hostapd runtime config file.
if [ $wpa -gt 0 ]; then
	echo "auth_algs=1" >> "$hostapd_conf"
	echo "wpa=$wpa" >> "$hostapd_conf"
	echo "wpa_pairwise=$wpa_cipher" >> "$hostapd_conf"
	echo "rsn_pairwise=$rsn_cipher" >> "$hostapd_conf"
	echo "ssid=$ssid" >> "$hostapd_conf"
	echo "wpa_passphrase=$key" >> "$hostapd_conf"
else
	echo "auth_algs=1" >> "$hostapd_conf"
	echo "wpa=0" >> "$hostapd_conf"
	echo "ssid=$ssid" >> "$hostapd_conf"
fi

if uci_bool $(uci get unum.default_startup.rfkill_enable_wifi); then
	rfkill unblock wifi
fi

if uci_bool $(uci get unum.default_startup.replace_wifi_interface); then
    echo "Creating WLAN interface $wlan_ifname"
    iw phy $phydev interface add $wlan_ifname type monitor
fi

if uci_bool $(uci get unum.default_startup.kill_wpa_supplicant); then
    killwait wpa_supplicant
fi

echo "Starting hostapd..."
hostapd -B "$hostapd_conf"
