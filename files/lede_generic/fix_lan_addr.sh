#!/bin/sh
# (C) 2018 minim.co

# Tracking run time in seconds
RUN_TIME="0"
DELAY=3
MAX_TIME="900"

while [ $RUN_TIME -lt $MAX_TIME ]; do
  OP_MODE=`/sbin/uci get minim.@unum[0].opmode`
  if ! [ "${OP_MODE#*_}" != "ap" ]; then
    echo "Not runnig LAN IP conflict check in AP mode"
    break
  fi

  IS_UP=`/sbin/uci -p /var/state get network.wan.up`
  if [ -z "$IS_UP" ]; then
    IS_UP=`/sbin/uci -p /var/state get network.wwan.up`
  fi

  if [ ! -z "$IS_UP" ] && [ $IS_UP -gt 0 ]; then
    GATEWAY_ROUTE_STR=`ip route | grep default`
    GATEWAY_IP=`echo $GATEWAY_ROUTE_STR | awk '{print $3}'`
    GATEWAY_IP_INT=`echo $GATEWAY_IP | tr . '\n' | awk '{s = s*256 + $1} END{print s}'`

    LAN_IP=`/sbin/uci -p /var/state get network.lan.ipaddr`
    LAN_IP_INT=`echo $LAN_IP | tr . '\n' | awk '{s = s*256 + $1} END{print s}'`

    LAN_MASK=`/sbin/uci -p /var/state get network.lan.netmask`
    LAN_MASK_INT=`echo $LAN_MASK | tr . '\n' | awk '{s = s*256 + $1} END{print s}'`
    XOR_LAN_MASK_INT=$(( $LAN_MASK_INT ^ 0xFFFFFFFF ))

    MIN_SUBNET_IP=$(( $LAN_IP_INT & $LAN_MASK_INT ))
    MAX_SUBNET_IP=$(( $LAN_IP_INT | $XOR_LAN_MASK_INT ))

    LAN_PROTO=`/sbin/uci -p /var/state get network.lan.proto`

    if [ $GATEWAY_IP_INT -ge $MIN_SUBNET_IP ] && [ $GATEWAY_IP_INT -le $MAX_SUBNET_IP ] && [ $LAN_PROTO == "static" ]; then
      if [ ${GATEWAY_IP:0:4} == '192.' ]; then
        NEW_LAN_IP="10.10.11.1"
      else
        NEW_LAN_IP="192.168.11.1"
      fi
      echo "Changing LAN IP to $NEW_LAN_IP, the old IP is $LAN_IP"
      /sbin/uci set network.lan.ipaddr=$NEW_LAN_IP
      /sbin/uci commit
      /sbin/ifup lan
      /etc/init.d/firewall restart
    else
      echo "LAN IP is $LAN_IP, no conflict detected"
    fi

    break
  fi

  sleep $DELAY
  is_on_boarding=`ps | grep run_on_board_sta.sh | grep -v grep`

  if [ "$is_on_boarding" == "" ]; then
    # Update the run time only after run_on_board_sta.sh's exit
    RUN_TIME=$(( $RUN_TIME + $DELAY ))
  fi
done
