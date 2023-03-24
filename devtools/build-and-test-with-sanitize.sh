#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2023 Bareos GmbH & Co. KG
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

set -u
set -e
set -x
shopt -s nullglob

# make sure we're in bareos's toplevel dir
if [ ! -f core/src/include/bareos.h ]; then
  echo "$0: Invoke from Bareos' toplevel directory" >&2
  exit 2
fi

nproc="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)"

export CTEST_PARALLEL_LEVEL=20

if [ -z ${CMAKE_BUILD_PARALLEL_LEVEL+x} ]; then
  export CMAKE_BUILD_PARALLEL_LEVEL="$nproc"
fi

rm -rf cmake-build

cmake \
  -S . \
  -B cmake-build \
  -DENABLE_SANITIZERS=yes \
  -Dpostgresql=yes
cmake --build cmake-build

# avoid problems in containers with sanitizers
# see https://github.com/google/sanitizers/issues/1322
export ASAN_OPTIONS=intercept_tls_get_addr=0

cd cmake-build

# preset CTestCostData.txt
mkdir -p Temporary/Testing
cp ../CTestCostData.txt Temporary/Testing

export REGRESS_DEBUG=1
ctest \
  --script CTestScript.cmake \
  --verbose \
  --label-exclude "broken.*" \
  || echo "ctest failed"
