#!/bin/sh
# (C) 2018 minim.co

source m_functions.sh

len=`cat /etc/shadow  | grep "root:" | awk -F':' '{print $2}' | wc | awk '{print $2}'`
if [ "$len" = "0" ]; then
	suff=$(get_b1300_serial_num)
	stuff=`echo $suff | /usr/bin/cut -c-8`

	echo $stuff >/tmp/mistuff.txt 
	echo $stuff >>/tmp/mistuff.txt
	passwd  root < /tmp/mistuff.txt
	rm /tmp/mistuff.txt
fi

