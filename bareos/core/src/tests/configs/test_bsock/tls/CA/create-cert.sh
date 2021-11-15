#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

HOSTNAME="$1"

# create key
openssl genrsa -out "$HOSTNAME-key.pem" 4096

# crate csr
openssl req -new -sha256 -key "$HOSTNAME-key.pem" -subj "/C=DE/ST=NRW/O=Bareos GmbH & Co. KG/CN=$HOSTNAME" -out  "$HOSTNAME.csr"

# sign
openssl x509 -req -in "$HOSTNAME.csr" -CA bareosca.crt -CAkey bareosca.key -CAcreateserial -out  "$HOSTNAME-cert.pem" -days 3650 -sha256
