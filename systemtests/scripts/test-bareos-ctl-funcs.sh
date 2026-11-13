#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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
. "$1"
python_executable="${2:?Python interpreter path required}"

unset BAREOS_CONFIG_DIR
PIDOF=/bin/false
PGREP=$(command -v pgrep)
PS=$(command -v ps)
prefix="bareos_test_lookup_$$"
name="${prefix}.x"
other_pid=""
regex_pid=""
exact_pid=""
config_other_pid=""
config_exact_pid=""
config_trailing_pid=""
trap 'kill ${other_pid} ${regex_pid} ${exact_pid} ${config_other_pid} ${config_exact_pid} ${config_trailing_pid} 2>/dev/null || true' EXIT

bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' _ "${name}-other" "$python_executable" &
other_pid=$!
bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' _ "${prefix}Ax" "$python_executable" &
regex_pid=$!
sleep 0.1

if [ -n "$(pidofproc "$name" 0)" ]; then
  echo "Found a different process instead of $name" >&2
  exit 1
fi
killproc "$name" 0 -TERM >/dev/null
kill -0 "$other_pid" "$regex_pid"

bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' _ "/tmp/$name" "$python_executable" &
exact_pid=$!
sleep 0.1

if [ "$(pidofproc "$name" 0)" != "$exact_pid" ]; then
  echo "Did not find the exact process using pgrep" >&2
  exit 1
fi

PGREP=""
if [ "$(pidofproc "$name" 0)" != "$exact_pid" ]; then
  echo "Did not find the exact process using ps" >&2
  exit 1
fi

kill "$exact_pid"
wait "$exact_pid" 2>/dev/null || true
exact_pid=""
if [ -n "$(pidofproc "$name" 0)" ]; then
  echo "ps found a different process whose name starts with $name" >&2
  exit 1
fi

BAREOS_CONFIG_DIR="/tmp/${prefix}_config"
bash -c 'exec -a "$1" "$3" -c "import time; time.sleep(30)" -c "$2"' \
  _ "/tmp/$name" "${BAREOS_CONFIG_DIR}-other" "$python_executable" &
config_other_pid=$!
bash -c 'exec -a "$1" "$3" -c "import time; time.sleep(30)" -c "$2"' \
  _ "/tmp/$name" "$BAREOS_CONFIG_DIR" "$python_executable" &
config_exact_pid=$!
bash -c 'exec -a "$1" "$3" -c "import time; time.sleep(30)" -c "$2" --extra' \
  _ "/tmp/$name" "$BAREOS_CONFIG_DIR" "$python_executable" &
config_trailing_pid=$!
sleep 0.1

if [ "$(filter_pids_by_config_dir "$config_trailing_pid")" != "$config_trailing_pid" ]; then
  echo "Did not match a config directory followed by another argument" >&2
  exit 1
fi

PGREP=$(command -v pgrep)
if [ "$(pidofproc "$name" 0)" != "$config_exact_pid" ]; then
  echo "pgrep selected a process with a different config directory" >&2
  exit 1
fi

PGREP=""
if [ "$(pidofproc "$name" 0)" != "$config_exact_pid" ]; then
  echo "ps selected a process with a different config directory" >&2
  exit 1
fi

killproc "$name" 0 -TERM >/dev/null
kill -0 "$config_other_pid"
