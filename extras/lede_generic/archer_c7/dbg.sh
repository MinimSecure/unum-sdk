#!/bin/bash

MYDIR=$(dirname "$0")

if [ "$1" = "" ] || [ "$2" = "" ]; then 
  echo "Usage: $0 <IP_ADDR> <APP> [APPOPTIONS...]"
  echo "Start remote debug session for an APP on the AP."
  echo "APP - host pathname to the app binary, will upload to /var."
  exit 0
fi
IP=$1
SSHOPT="-o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no"
TPATH=/var
APP=$(basename "$2")
APPPNAME="$2"
shift
shift
APPOPTIONS=$*

echo "Copying over the files..."
scp "$MYDIR/gdbserver" "$APPPNAME" root@$IP:$TPATH/
echo "Starting gdbserver on the $IP"
ssh root@$IP "sh -c '$TPATH/gdbserver :1234 $TPATH/$APP $APPOPTIONS &' 1>/dev/null 2>&1 </dev/null"
echo "Starting gdb and connecting to the remote ..."
"$MYDIR/gdb" --ex "target remote $IP:1234"

