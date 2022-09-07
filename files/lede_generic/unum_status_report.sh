#!/bin/sh
# Called with $1 = old status and $2 = new status

AGENT_STATUS_UP_LED_FILE=/tmp/agent_status_up_led

if grep "can_connect" /tmp/provision_info.json > /dev/null; then
    touch $AGENT_STATUS_UP_LED_FILE
else
    rm -f $AGENT_STATUS_UP_LED_FILE
fi    

