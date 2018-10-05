#!/bin/bash

sudo apt-get -qq update
# qt5 should be used. Remove qt4-dev to avoid confusion.
sudo apt-get remove libqt4-dev
sudo pip install sauceclient selenium

cd core
dpkg-checkbuilddeps 2> /tmp/dpkg-builddeps || true
if [ $BUILD_WEBUI ]
then
	cd ../webui
	dpkg-checkbuilddeps 2>> /tmp/dpkg-builddeps || true
fi
cat /tmp/dpkg-builddeps
sed -e "s/^.*:.*:\s//" -e "s/\s([^)]*)//g" -e "s/|/ /g" -e "s/ /\n/g" /tmp/dpkg-builddeps > /tmp/build_depends
sudo apt-get -q --assume-yes install fakeroot
cat /tmp/build_depends | while read pkg; do
echo "installing $pkg"
sudo apt-get -q --assume-yes install $pkg
done
true

