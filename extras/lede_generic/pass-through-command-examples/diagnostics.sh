#!/bin/sh
# This is an example of the script for handling "diagnostics" command
# from the pass-through command set under LEDE/OpenWRT. The "diagnostics"
# command in the pass-trough command set "sources" this script (it's expected
# to be named /usr/bin/diagnostics.sh). The script is expected to collect
# all the device diagnostics for troubleshooting the platform except the
# agent logs (they are collected automaticlly prior to sourcing this
# script). It has to follow those conventions:
# - Store the information in the files under the working (as the control
#   is passed to the script) directory. The subfolders can be used and
#   encouraged to group the data and avoid clutter. Note: the REPORT_DIR
#   environment variable pointing to the working directory is available
#   for the script to read if necessary.
# - Log the operations as they are performed by appending single line
#   messages to "./status.txt" file. The messages should be suitable for
#   logging in the agent log and being shown to the user in the mobile
#   app during off-line report creation.

# Example how to call this script for testing:
# REPORT_DIR=/tmp/work_dir
# rm -Rf $REPORT_DIR
# mkdir -p $REPORT_DIR
# cd $REPORT_DIR
# date > status
# source /usr/bin/diagnostics.sh
# echo "Platform-specific report content:"
# cd $REPORT_DIR
# ls -lr .

echo "Collecting OpenWRT system log..." >> status.txt
{ echo "/sbin/logread output:"; /sbin/logread; } > ./syslog.txt 2>&1

echo "Collecting kernel log..." >> status.txt
{ echo "/bin/dmesg output:"; /bin/dmesg; } > ./dmesg_out.txt 2>&1

echo "Collecting process list..." >> status.txt
{ echo "/bin/ps output:"; /bin/ps; } > ./ps_out.txt 2>&1

echo "Capturing memory usage information..." >> status.txt
{ echo "cat /proc/meminfo:"; cat /proc/meminfo; } > ./meminfo.txt 2>&1
