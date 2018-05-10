#!/bin/bash

print_header()
{
   TEXT="$1"
   printf "#\n"
   printf "# %s\n" "$TEXT"
   printf "#\n"
}

cd core
if [ "${COVERITY_SCAN}" ]; then
   # run configure with default options
   debian/rules override_dh_auto_configure
   eval "$COVERITY_SCAN_BUILD"
else
   print_header "build Bareos packages"
   fakeroot debian/rules binary

   print_header "create Debian package repository"
   cd ..
   dpkg-scanpackages . > Packages
   gzip --keep Packages
   ls -la Packages*
   printf 'deb file:%s /\n' $PWD > /tmp/bareos.list
   sudo cp /tmp/bareos.list /etc/apt/sources.list.d/bareos.list
   cd -

   print_header "install Bareos packages"
   sudo apt-get -qq update
   sudo apt-get install -y --force-yes bareos bareos-database-$DB
fi
