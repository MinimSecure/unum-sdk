#!/bin/sh

# Run the script in a folder containg certifcates .pem files.
# The script shows certificates with a weak key.

function list_weak_certs() {
  for cert in ./*.pem; do
    weak=$(openssl x509 -noout -text -in $cert | grep "Public-Key: (1024 bit)" )
    if [ ! -z "$weak" ]; then
      echo "$cert"
    fi
  done
}

echo "Weak:"
list_weak_certs
