#!/bin/bash
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

if [ -z ${CTEST_PARALLEL_LEVEL+x} ]; then
  export CTEST_PARALLEL_LEVEL="$nproc"
fi
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

cd cmake-build
export REGRESS_DEBUG=1
ctest \
  --script CTestScript.cmake \
  --verbose \
  --label-exclude "broken.*" \
  || echo "ctest failed"
