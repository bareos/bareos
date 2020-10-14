#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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


set -e
set -u

print_header()
{
   TEXT="$1"
   printf "#\n"
   printf "# %s\n" "$TEXT"
   printf "#\n"
}

if [ "${COVERITY_SCAN:-}" ]; then
   # run configure with default options
   debian/rules override_dh_auto_configure
   eval "$COVERITY_SCAN_BUILD"
   echo "result: $?"
   exit 0
fi


print_header "build Bareos core packages"
# https://www.debian.org/doc/debian-policy/ch-source.html#s-debianrules-options
export DEB_BUILD_OPTIONS="nocheck"
fakeroot debian/rules binary

print_header "create Debian package repository"
cd ..
dpkg-scanpackages . > Packages
gzip --keep Packages
ls -la Packages*
printf 'deb file:%s /\n' $PWD > /tmp/bareos.list
sudo cp /tmp/bareos.list /etc/apt/sources.list.d/bareos.list
cd -

PKGS="bareos bareos-database-$DB"
if [ "${BUILD_WEBUI:-}" ]; then
    PKGS="$PKGS bareos-webui"
fi
print_header "install Bareos packages: $PKGS"
sudo apt-get -qq update --allow-insecure-repositories || true
sudo apt-get install -y --allow-unauthenticated $PKGS
