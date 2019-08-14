#!/bin/sh

# create and add an empty unittest file

if [ "x$1" = "x" ]; then
  echo "You should enter: $0 testname"
  exit 1
fi

if [ -f "$1.cc" ]; then
  echo "Filename for test already exists: $1.cc"
  exit 1
fi

if grep -w "$1.cc" CMakeLists.txt; then
  echo "$1.cc already exists in CMakeLists.txt"
  exit 1
fi

file_created="false"
if cat > "$1.cc" <<EOF
/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "gtest/gtest.h"

EOF
then
  if git add "$1.cc"; then
    file_created="true"
  fi
fi #if cat > "$1.cc" <<EOF

if ! "$file_created"; then
  echo "File creation failed: $1.cc"
  exit 1
fi

if ! sed "s/@replace@/$1/g" < test_cmakelists_template.txt.in >> CMakeLists.txt; then
  echo "Adding to CMakeLists.txt failed"
  exit 1
fi

exit 0

