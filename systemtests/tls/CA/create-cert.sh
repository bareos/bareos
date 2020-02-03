#!/bin/bash
HOSTNAME="$1"

# create key
openssl genrsa -out "$HOSTNAME-key.pem" 4096

# crate csr
openssl req -new -sha256 -key "$HOSTNAME-key.pem" -subj "/C=DE/ST=NRW/O=Bareos GmbH & Co. KG/CN=$HOSTNAME" -out  "$HOSTNAME.csr"

# sign
openssl x509 -req -in "$HOSTNAME.csr" -CA bareosca.crt -CAkey bareosca.key -CAcreateserial -out  "$HOSTNAME-cert.pem" -days 3650 -sha256
