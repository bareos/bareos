#!/bin/sh
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2016-2020 Bareos GmbH & Co. KG
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

set -x
echo "$0: SOURCE=$1, DEST=$2"

SOURCE=$1
DEST=$2

for i in `sed -n -r "s|File: (etc/bareos/bareos-dir.d/.*)|${DEST}/\1|p" $SOURCE/debian/univention-bareos.univention-config-registry`; do
    mkdir -p "`dirname $i`"
    ln -s /usr/share/univention-bareos/bareos-dir-restart "$i" || true
done

exit 0
