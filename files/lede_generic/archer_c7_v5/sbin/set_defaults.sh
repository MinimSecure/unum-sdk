#!/bin/sh

CRED_PROVISIONED_FILE="/etc/unum/.credentials_provisioned"
WIFI_PROVISIONED_FILE="/etc/unum/.wifi_provisioned"

mkdir -p /var/tmp/

# Note that the partition is different from prior versions of Archers
macaddr=`hexdump -s 8 -n 6 -e '5/1 "%02x:" 1/1 "%02x"' /dev/mtd3`
if [ $(expr "x$macaddr" : 'x[0-9a-f]\{2\}:[0-9a-f]\{2\}:[0-9a-f]\{2\}:[0-9a-f]\{2\}:[0-9a-f]\{2\}:[0-9a-f]\{2\}$') -eq 0 ]; then
	logger -s "Unable to get device MAC address!"
        macaddr=""
else
	# That file has to be in /var, not /var/tmp (fix when refactoring)
	echo $macaddr > /var/tmp/deviceid.txt
fi

# We need key for both WiFi and credentials provisioning
if [ ! -e "$CRED_PROVISIONED_FILE" ] || [ ! -e "$WIFI_PROVISIONED_FILE" ]; then
	key="`hexdump -s 520 -n 8 -e '8/1 "%c"' /dev/mtd3`"
	if [ $(expr "x$key" : 'x[0-9]\{8\}$') -eq 0 ]; then
		logger -s "Unable to get device default key, skipping provisioning!"
		exit 0
	fi
fi

# Set default password
if [ ! -e "$CRED_PROVISIONED_FILE" ]; then
	echo -e "$key\n$key" | passwd root
	touch "$CRED_PROVISIONED_FILE"
fi

# Set default SSIDs
if [ ! -e "$WIFI_PROVISIONED_FILE" ]; then
	suffix=`hexdump -s 12 -n 2 -e '2/1 "%02X"' /dev/mtd3`
	if [ $(expr "x$suffix" : 'x[0-9A-F]\{4\}$') -eq 0 ]; then
        	logger -s "Unable to get SSID sufix, skipping WiFi provisioning!"
	        exit 0
	fi
	ssid24="TP-Link_${suffix}"
	ssid5="TP-Link_${suffix}_5G"
	uci -q batch <<-EOF
		set wireless.default_radio0.ssid=$ssid5
		set wireless.default_radio0.encryption="psk2"
		set wireless.default_radio0.key=$key
		set wireless.default_radio0.disabled=0
		set wireless.radio0.disabled=0
		set wireless.default_radio1.ssid=$ssid24
		set wireless.default_radio1.encryption="psk2"
		set wireless.default_radio1.key=$key
		set wireless.default_radio1.disabled=0
		set wireless.radio1.disabled=0
	EOF
	uci -q commit wireless
	touch "$WIFI_PROVISIONED_FILE"
fi

