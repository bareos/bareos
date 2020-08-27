#!/bin/bash

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
