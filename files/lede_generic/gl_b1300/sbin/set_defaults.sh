#!/bin/sh
/sbin/set_deviceid.sh
/sbin/set_default_ssid.sh
/sbin/create_on_board_sta.sh
/sbin/run_on_board_sta.sh &
/sbin/set_default_password.sh
