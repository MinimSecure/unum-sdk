#!/bin/sh
# For Gl-Inet B1300, the serial number is the auth_info_key
. m_functions.sh && echo -n $(get_b1300_serial_num)
