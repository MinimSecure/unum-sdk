#!/bin/sh

# Run the script in a folder containg certifcates .pem files.
# The script shows certificates expired as of now and the ones expiring a year from now.

function list_expired_before() {
  today=${1:-$(date +%s)}
  for cert in ./*.pem; do
    notafter=$(openssl x509 -noout -subject -enddate -in $cert | grep "notAfter=" | sed -e 's/notAfter=//')
    tgtdate=$(date -d "$notafter" +%s)
    #echo "tgtdate=$tgtdate"
    #echo "today=$today"
    if [ $tgtdate -le $today ]; then
      echo "$cert"
    fi
  done
}

echo "Expired:"
list_expired_before

let "nextyear = $(date +%s) + 365 * 24 * 60 * 60"
echo "Expiring in a year:"
list_expired_before $nextyear

