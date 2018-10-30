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

# Dnsmasq management script

set -eo pipefail

source "$(dirname "$BASH_SOURCE")/unum_env.sh"

valid_config || exit 1

dnsmasq_conf="$UNUM_VAR_DIR/dnsmasq.conf"

# Read interface names from stored UCI configuration.
loifname=$(uci get network.loopback.ifname)
wanifname=$(uci get network.wan.ifname)
lanifname=$(uci get network.lan.ifname)

dnsmasq_opt() {
    local name="$1"
    local key="dhcp.@dnsmasq[-1]"
    [ -z "$name" ] || key="$key.$name"
    echo $(uci -d',' get "$key")
    return $?
}

dnsmasq_opt_bool() {
    if [[ $(dnsmasq_opt "$1") == "1" ]]; then
        return 0
    fi
    return 1
}
interface=$(dnsmasq_opt interface)
notinterface=$(dnsmasq_opt notinterface)
if [[ "$interface" == "" && "$notinterface" == "" ]]; then
    interface="$loifname,$lanifname"
fi
[ -z "$interface" ] ||    echo "interface=$loifname,$lanifname" >  "$dnsmasq_conf"
[ -z "$notinterface" ] || echo "except-interface=$wanifname"    >> "$dnsmasq_conf"

localv=$(dnsmasq_opt local)
[ -z "$localv" ] || echo "local=$localv" >> "$dnsmasq_conf"

domain=$(dnsmasq_opt domain)
[ -z "$domain" ] || echo "domain=$domain" >> "$dnsmasq_conf"

dns_port=$(dnsmasq_opt port)
[ -z "$dns_port" ] || echo "port=$dns_port" >> "$dnsmasq_conf"

resolv_file=$(dnsmasq_opt resolvfile)
[ -z "$resolv_file" ] || echo "resolv-file=$resolv_file" >> "$dnsmasq_conf"

lease_file=$(dnsmasq_opt leasefile)
[ -z "$lease_file" ] || echo "dhcp-leasefile=$lease_file" >> "$dnsmasq_conf"


dnsmasq_opt_bool "noresolv" && echo "no-resolv" >> "$dnsmasq_conf"
while IFS=',' read server; do
    [ -z "$server" ] || echo "server=$server" >> "$dnsmasq_conf"
done <<< "$(dnsmasq_opt server)"

dnsmasq_opt_bool "expandhosts" && echo "expand-hosts" >> "$dnsmasq_conf"
dnsmasq_opt_bool "nonegcache" && echo "no-negcache" >> "$dnsmasq_conf"
dnsmasq_opt_bool "authoritative" && echo "dhcp-authoritative" >> "$dnsmasq_conf"
dnsmasq_opt_bool "domainneeded" && echo "domain-needed" >> "$dnsmasq_conf"
dnsmasq_opt_bool "boguspriv" && echo "bogus-priv" >> "$dnsmasq_conf"
dnsmasq_opt_bool "filterwin2k" && echo "filterwin2k" >> "$dnsmasq_conf"
dnsmasq_opt_bool "rebind_protection" && echo "stop-dns-rebind" >> "$dnsmasq_conf"
dnsmasq_opt_bool "rebind_localhost" && echo "rebind-localhost-ok" >> "$dnsmasq_conf"
dnsmasq_opt_bool "nonwildcard" && echo "bind-interfaces" >> "$dnsmasq_conf"
dnsmasq_opt_bool "localservice" && echo "local-service" >> "$dnsmasq_conf"
dnsmasq_opt_bool "localise_queries" && echo "localise-queries" >> "$dnsmasq_conf"

if [[ "$(uci get dhcp.lan.ignore)" != "1" ]]; then
    lanip=$(uci get network.lan.ipaddr)
    lannetmask=$(uci get network.lan.netmask)

    if [[ "$lannetmask" != "255.255.255.0" ]]; then
        # TODO: The values calculated for dhcp make assumptions around this
        echo "Warning: unsupported subnet mask: $lannetmask"
    fi

    netprefix=$(echo $lanip | awk 'BEGIN{FS="."}  {printf("%s.%s.%s", $1,$2,$3);}')
    dhcpstart="$netprefix.$(uci get dhcp.lan.start)"
    dhcpend="$netprefix.$(expr $(uci get dhcp.lan.start) + $(uci get dhcp.lan.limit))"
    leasetime=$(uci get dhcp.lan.leasetime)

    echo "dhcp-range=$dhcpstart,$dhcpend,$lannetmask,$leasetime" >> "$dnsmasq_conf"
else
    echo "Warning: not configuring DHCP on the LAN interface"
fi

if [[ "$(uci get dhcp.wan.ignore)" != "1" ]]; then
    echo "Warning: configuring DHCP on the WAN interface is not supported"
fi

dnsmasq_opt_dhcp_host() {
    local val="$1"
    local idx="$2"
    local key="$3"
    local ip=$(uci get dhcp.@host[$idx].ip)
    local mac=$(uci get dhcp.@host[$idx].mac)
    local name=$(uci get dhcp.@host[$idx].name)
    echo "dhcp-host=$mac,$ip,$name" >> "$dnsmasq_conf"
}
uci_iter dnsmasq_opt_dhcp_host "dhcp.@host[#]"

echo "Starting dnsmasq..."
dnsmasq -C "$dnsmasq_conf"
