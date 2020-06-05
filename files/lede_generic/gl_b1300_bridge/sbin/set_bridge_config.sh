#!/bin/sh

/sbin/uci import minim < /dev/null
/sbin/uci add minim unum
/sbin/uci set minim.@unum[0].opmode='ap'