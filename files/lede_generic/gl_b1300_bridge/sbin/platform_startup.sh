#!/bin/sh
# (c) 2019-2020 minim.co

echo  2 > /sys/class/net/eth0/queues/rx-0/rps_cpus 
echo  2 > /sys/class/net/eth1/queues/rx-0/rps_cpus 
echo  2 > /sys/class/net/eth0/queues/rx-1/rps_cpus 
echo  2 > /sys/class/net/eth1/queues/rx-1/rps_cpus 
echo  4 > /sys/class/net/eth0/queues/rx-2/rps_cpus 
echo  4 > /sys/class/net/eth1/queues/rx-2/rps_cpus 
echo  8 > /sys/class/net/eth0/queues/rx-3/rps_cpus 
echo  8 > /sys/class/net/eth1/queues/rx-3/rps_cpus 
