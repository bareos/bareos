#!/bin/bash

if type -p cmake3 >/dev/null 2>&1; then
  cmake="$(type -p cmake3)"
else
  cmake="$(type -p cmake)"
fi

pushd "$(dirname "$0")/../../.." >/dev/null
"$cmake" -P get_version.cmake | sed -e 's/^-- //'
