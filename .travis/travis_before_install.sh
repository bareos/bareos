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

sudo apt-get -qq update
# qt5 should be used. Remove qt4-dev to avoid confusion.
sudo apt-get remove libqt4-dev
dpkg-checkbuilddeps 2> /tmp/dpkg-builddeps || true
if [ "${BUILD_WEBUI:-}" ]; then
    sudo -H pip install --upgrade pip 'urllib3>=1.22'
    sudo -H pip install sauceclient selenium
fi
cat /tmp/dpkg-builddeps
sed -e "s/^.*:.*:\s//" -e "s/\s([^)]*)//g" -e "s/|/ /g" -e "s/ /\n/g" /tmp/dpkg-builddeps > /tmp/build_depends
sudo apt-get -q --assume-yes install fakeroot
cat /tmp/build_depends | while read pkg; do
  echo "installing $pkg"
  sudo apt-get -q --assume-yes install $pkg || true
done
