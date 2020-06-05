#!/bin/sh
/sbin/uci set uhttpd.main.redirect_https='0'
/sbin/uci commit uhttpd
