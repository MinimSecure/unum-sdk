#!/bin/sh
# For minim_r14, the serial number is the auth_info_key
. m_functions.sh && echo -n $(get_serial_num)
