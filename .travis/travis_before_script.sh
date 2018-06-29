#!/bin/bash

print_header()
{
   TEXT="$1"
   printf "#\n"
   printf "# %s\n" "$TEXT"
   printf "#\n"
}

cd core
if [ "${COVERITY_SCAN}" ]
then
   # run configure with default options
   debian/rules override_dh_auto_configure
   eval "$COVERITY_SCAN_BUILD"
else
   print_header "build Bareos core packages"
   fakeroot debian/rules binary
fi
if [ "${BUILD_WEBUI}" ]
then
	cd ../webui
	# to avoid timestamp conflicts while autoconfiguring we refresh every file
	touch *
	if [ "${COVERITY_SCAN}" ]
	then
	   # run configure with default options
	   debian/rules override_dh_auto_configure
	   eval "$COVERITY_SCAN_BUILD"
	else
	   print_header "build Bareos webui packages"
	   fakeroot debian/rules binary
	fi
fi
print_header "create Debian package repository"
cd ..
dpkg-scanpackages . > Packages
gzip --keep Packages
ls -la Packages*
printf 'deb file:%s /\n' $PWD > /tmp/bareos.list
sudo cp /tmp/bareos.list /etc/apt/sources.list.d/bareos.list
cd -

print_header "install Bareos core package"
sudo apt-get -qq update
sudo apt-get install -y --force-yes bareos bareos-database-$DB
if [ "${BUILD_WEBUI}" ]; then sudo apt-get install -y --force-yes bareos-webui; fi
