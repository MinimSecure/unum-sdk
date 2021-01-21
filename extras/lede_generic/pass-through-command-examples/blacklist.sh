#!/bin/sh
# This is an example of the script for handling "blacklist" command from the
# pass-through command set under LEDE/OpenWRT. The "blacklist" command in the
# pass-trough command set "sources" this script (it's expected to be named
# /usr/bin/blacklist.sh) and passes to it the MAC addresses of the LAN devices
# and IP addresses of the destinations to white/blacklist. The lists are passed
# in the environment variables BLACKHOLED_MACS, BLACKLISTED_IPS and
# WHITELISTED_IPS

# Example how this script is invoked:
# WHITELISTED_IPS="34.207.26.129 20.190.254.217"
# BLACKHOLED_MACS=" 00:01:02:03:04:05 00:01:02:03:04:06 00:01:02:03:04:07"
# BLACKLISTED_IPS=" 1.2.3.4 1.2.3.5 1.2.3.6 1.2.3.7"
# [ -f /usr/bin/blacklist.sh ] && source /usr/bin/blacklist.sh

# Update the rules file
RULES_FILE=/tmp/firewall_unum_blacklist
echo "iptables -N unum_forward" > "$RULES_FILE"
for IP_ADDR in $WHITELISTED_IPS; do
  echo "iptables -A unum_forward -d $IP_ADDR -j ACCEPT" >> "$RULES_FILE"
  echo "iptables -A unum_forward -s $IP_ADDR -j ACCEPT" >> "$RULES_FILE"
done
for MAC_ADDR in $BLACKHOLED_MACS; do
  echo "iptables -A unum_forward -m mac --mac-source $MAC_ADDR -j DROP" >> "$RULES_FILE"
done
for IP_ADDR in $BLACKLISTED_IPS; do
  echo "iptables -A unum_forward -d $IP_ADDR -j DROP" >> "$RULES_FILE"
  echo "iptables -A unum_forward -s $IP_ADDR -j DROP" >> "$RULES_FILE"
done
echo "iptables -t filter -I FORWARD -j unum_forward" >> "$RULES_FILE"

# Update UCI config to run the rules script on config changes
if [ "$(uci set firewall.unum_blacklist)" != "include" ]; then
  uci set firewall.unum_blacklist=include
  uci set firewall.unum_blacklist.family=any
  uci set firewall.unum_blacklist.path=$RULES_FILE
  uci set firewall.unum_blacklist.reload=1
  uci set firewall.unum_blacklist.type=script
  uci commit
fi

# Apply the rules
/etc/init.d/firewall restart
