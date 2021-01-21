#!/bin/sh
# This is an example of the script for handling "dns_filter_update" command
# from the pass-through command set under LEDE/OpenWRT. The "dns_filter_update"
# command in the pass-trough command set "sources" this script (it's expected
# to be named /usr/bin/dns_filter_update.sh) and passes to it the the following:
# - EDNS_MODE environment variable that is set to 1 if the LAN has content
#   filtering turned on and requires EDNS to be enabled and DNS caching to be
#   disabled (it's set only if the firmware release is tagged with the
#   "edns0_content_filter" feature flag).
# - DNS_REDIRECT environment variable contains a list of IP address pairs
#   in the form "1.2.3.4/5.6.7.8 ...". The DNS requests from the first (LAN)
#   address of each pair have to be redirected to the second (Minim DNS server)
#   address. The rules should be at the top of the PREROUTING chain of the
#   "nat" table.
# - EDNS_LAN_REDIRECT environment variable contains the IP address of the
#   gateway primary LAN interface. It can be used to force all the LAN
#   DNS requests to go through the router's DNS relay server. It's set
#   only if the firmware release is tagged with the "edns0_content_filter"
#   feature flag. The rules have to be at the top of the PREROUTING chain
#   of the "nat" table.
# - EDNS_RTR_REDIRECT environment variable contains the IP address where to
#   redirect the router's outgoing DNS requests. It's set only if the
#   firmware release is tagged with the "edns0_content_filter" feature flag.
#   The rules have to be at the top of the OUTPUT chain of the "nat" table.

# Example how this script is invoked for EDNS-capable firmware:
# EDNS_MODE=1 # note: might be 0 if content filtering is disabled
# DNS_REDIRECT=""
# EDNS_LAN_REDIRECT="192.168.11.1"
# EDNS_RTR_REDIRECT="1.2.3.4"
# [ -f /usr/bin/dns_filter_update.sh ] && source /usr/bin/dns_filter_update.sh

# Example how this script is invoked for non-EDNS firmware:
# EDNS_MODE=0 # note: cannot be 1 for non-EDNS firmware
# DNS_REDIRECT=" 1.2.3.4/5.6.7.8 4.3.2.1/1.1.2.2"
# EDNS_LAN_REDIRECT=""
# EDNS_RTR_REDIRECT=""
# [ -f /usr/bin/dns_filter_update.sh ] && source /usr/bin/dns_filter_update.sh

# Check settings for EDNS-based content filtering
ADDMAC_MODE="$(uci get dhcp.@dnsmasq[0].addmac 2> /dev/null)"
CACHE_SIZE="$(uci get dhcp.@dnsmasq[0].cachesize 2> /dev/null)"
if [ "$ADDMAC_MODE" == "base64" ] && [ "$CACHE_SIZE" == "0" ]; then
  CURRENT_EDNS_MODE=1
else
  CURRENT_EDNS_MODE=0
fi
if [ "$CURRENT_EDNS_MODE" != "$EDNS_MODE" ]; then
  if [ "$EDNS_MODE" = "0" ]; then
    uci delete dhcp.@dnsmasq[0].addmac
    uci delete dhcp.@dnsmasq[0].cachesize
  else
    uci set dhcp.@dnsmasq[0].addmac='base64'
    uci set dhcp.@dnsmasq[0].cachesize='0'
  fi
  uci commit
  /etc/init.d/dnsmasq restart
fi

RULES_FILE=/tmp/firewall_unum_dns

# Update the rules script, prepare chains first
cat << 'EOF_RULES' > $RULES_FILE
iptables -t nat -N unum_dns_filter
iptables -t nat -F unum_dns_filter
iptables -t nat -N unum_dns_output
iptables -t nat -F unum_dns_output

iptables -t nat -D PREROUTING -m tcp -p tcp --dport 53 -j unum_dns_filter
iptables -t nat -D PREROUTING -m udp -p udp --dport 53 -j unum_dns_filter
iptables -t nat -D OUTPUT -m tcp -p tcp --dport 53 -j unum_dns_output
iptables -t nat -D OUTPUT -m udp -p udp --dport 53 -j unum_dns_output
iptables -t nat -I PREROUTING -m tcp -p tcp --dport 53 -j unum_dns_filter
iptables -t nat -I PREROUTING -m udp -p udp --dport 53 -j unum_dns_filter
iptables -t nat -I OUTPUT ! -d 127.0.0.1 -m tcp -p tcp --dport 53 -j unum_dns_output
iptables -t nat -I OUTPUT ! -d 127.0.0.1 -m udp -p udp --dport 53 -j unum_dns_output

EOF_RULES

# Add dynamically created rules
for IP_ADDR_PAIR in $DNS_REDIRECT; do
  IP_ADDR1=${IP_ADDR_PAIR%/*}
  IP_ADDR2=${IP_ADDR_PAIR#*/}
  echo "iptables -t nat -A unum_dns_filter -s $IP_ADDR1 -m tcp -p tcp --dport 53 -j DNAT --to-destination $IP_ADDR2" >> "$RULES_FILE"
  echo "iptables -t nat -A unum_dns_filter -s $IP_ADDR1 -m udp -p udp --dport 53 -j DNAT --to-destination $IP_ADDR2" >> "$RULES_FILE"
  echo "conntrack -D -s $IP_ADDR1" >> "$RULES_FILE"
done
for IP_ADDR in $EDNS_LAN_REDIRECT; do
  echo "iptables -t nat -A unum_dns_filter -m tcp -p tcp --dport 53 -j DNAT --to-destination $IP_ADDR" >> "$RULES_FILE"
  echo "iptables -t nat -A unum_dns_filter -m udp -p udp --dport 53 -j DNAT --to-destination $IP_ADDR" >> "$RULES_FILE"
done
for IP_ADDR in $EDNS_RTR_REDIRECT; do
  echo "iptables -t nat -A unum_dns_output -m tcp -p tcp --dport 53 -j DNAT --to-destination $IP_ADDR" >> "$RULES_FILE"
  echo "iptables -t nat -A unum_dns_output -m udp -p udp --dport 53 -j DNAT --to-destination $IP_ADDR" >> "$RULES_FILE"
done

# Update UCI config to run the rules script on config changes
if [ "$(uci set firewall.unum_dns)" != "include" ]; then
  uci set firewall.unum_dns=include
  uci set firewall.unum_dns.family=any
  uci set firewall.unum_dns.path=$RULES_FILE
  uci set firewall.unum_dns.reload=1
  uci set firewall.unum_dns.type=script
  uci commit
fi

# Apply the rules
/etc/init.d/firewall restart
