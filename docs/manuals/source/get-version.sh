#!/bin/bash

if type -p cmake3 >/dev/null 2>&1; then
  cmake="$(type -p cmake3)"
else
  cmake="$(type -p cmake)"
fi

"$cmake" -P "$(dirname "$0")/../../../get_version.cmake" | sed -e 's/^-- //'
