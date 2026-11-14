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
export COLUMNS=80
prefix="bareos_test_lookup_$$"
name="${prefix}.x"
other_pid=""
regex_pid=""
exact_pid=""
config_other_pid=""
config_exact_pid=""
config_trailing_pid=""
long_path_pid=""
valgrind_pid=""
valgrind_other_pid=""
valgrind_argument_pid=""
trap 'kill ${other_pid} ${regex_pid} ${exact_pid} ${config_other_pid} ${config_exact_pid} ${config_trailing_pid} ${long_path_pid} ${valgrind_pid} ${valgrind_other_pid} ${valgrind_argument_pid} 2>/dev/null || true' EXIT

wait_for_command()
{
  local pid="$1" expected="$2" args attempt
  for attempt in {1..100}; do
    if ! args=$("$PS" -ww -p "$pid" -o args=); then
      echo "Child $pid exited before executing $expected" >&2
      return 1
    fi
    if [[ "$args" == "$expected" || "$args" == "$expected "* ]]; then
      return 0
    fi
    sleep 0.05
  done
  echo "Timed out waiting for child $pid to execute $expected" >&2
  return 1
}

bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' _ "${name}-other" "$python_executable" &
other_pid=$!
wait_for_command "$other_pid" "${name}-other"
bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' _ "${prefix}Ax" "$python_executable" &
regex_pid=$!
wait_for_command "$regex_pid" "${prefix}Ax"

if [ -n "$(pidofproc "$name" 0)" ]; then
  echo "Found a different process instead of $name" >&2
  exit 1
fi
killproc "$name" 0 -TERM >/dev/null
kill -0 "$other_pid" "$regex_pid"

bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' _ "/tmp/$name" "$python_executable" &
exact_pid=$!
wait_for_command "$exact_pid" "/tmp/$name"

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

long_directory="/tmp/$(printf '%0128d' 0)/$(printf '%0128d' 0)"
BAREOS_CONFIG_DIR="${long_directory}/${prefix}_config"
bash -c 'exec -a "$1" "$3" -c "import time; time.sleep(30)" -c "$2"' \
  _ "/tmp/$name" "${BAREOS_CONFIG_DIR}-other" "$python_executable" &
config_other_pid=$!
bash -c 'exec -a "$1" "$3" -c "import time; time.sleep(30)" -c "$2"' \
  _ "/tmp/$name" "$BAREOS_CONFIG_DIR" "$python_executable" &
config_exact_pid=$!
bash -c 'exec -a "$1" "$3" -c "import time; time.sleep(30)" -c "$2" --extra' \
  _ "/tmp/$name" "$BAREOS_CONFIG_DIR" "$python_executable" &
config_trailing_pid=$!
wait_for_command "$config_other_pid" "/tmp/$name"
wait_for_command "$config_exact_pid" "/tmp/$name"
wait_for_command "$config_trailing_pid" "/tmp/$name"

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

unset BAREOS_CONFIG_DIR
long_name="${name}-long"
bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' \
  _ "${long_directory}/${long_name}" "$python_executable" &
long_path_pid=$!
wait_for_command "$long_path_pid" "${long_directory}/${long_name}"

PGREP=$(command -v pgrep)
if [ "$(pidofproc "$long_name" 0)" != "$long_path_pid" ]; then
  echo "pgrep did not find a process with a long executable path" >&2
  exit 1
fi

PGREP=""
if [ "$(pidofproc "$long_name" 0)" != "$long_path_pid" ]; then
  echo "ps did not find a process with a long executable path" >&2
  exit 1
fi

BAREOS_CONFIG_DIR="${long_directory}/${prefix}_valgrind"
# exec -a supplies the command line seen by both ps and pgrep without
# requiring Valgrind to be installed on the test host.
for command_line in \
  "/usr/bin/valgrind --leak-check=full /tmp/${name}-other -c ${BAREOS_CONFIG_DIR}" \
  "/usr/bin/valgrind --log-file=/tmp/$name /tmp/other -c ${BAREOS_CONFIG_DIR} /tmp/$name" \
  "/usr/bin/valgrind --leak-check=full /tmp/$name -c ${BAREOS_CONFIG_DIR}-other"; do
  bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' \
    _ "$command_line" "$python_executable" &
  valgrind_other_pid="$valgrind_other_pid $!"
  wait_for_command "$!" "$command_line"
done
bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' \
  _ "/usr/bin/not-valgrind /tmp/$name -c ${BAREOS_CONFIG_DIR}" "$python_executable" &
valgrind_argument_pid=$!
wait_for_command "$valgrind_argument_pid" "/usr/bin/not-valgrind /tmp/$name"

for PGREP in "$(command -v pgrep)" ""; do
  if [ -n "$(pidofproc "$name" 0)" ]; then
    echo "Matched a non-daemon token or a different Valgrind config" >&2
    exit 1
  fi
done

bash -c 'exec -a "$1" "$2" -c "import time; time.sleep(30)"' \
  _ "/usr/bin/valgrind --leak-check=full --log-file=/tmp/valgrind.log /tmp/$name -v -c ${BAREOS_CONFIG_DIR}" "$python_executable" &
valgrind_pid=$!
wait_for_command "$valgrind_pid" "/usr/bin/valgrind --leak-check=full --log-file=/tmp/valgrind.log /tmp/$name"

for PGREP in "$(command -v pgrep)" ""; do
  if [ "$(pidofproc "$name" 0)" != "$valgrind_pid" ]; then
    echo "Did not find the Valgrind-wrapped daemon without a pidfile" >&2
    exit 1
  fi
  status "$name" 0 >/dev/null
done
killproc "$name" 0 -TERM >/dev/null
wait "$valgrind_pid" 2>/dev/null || true
valgrind_pid=""
kill -0 $valgrind_other_pid "$valgrind_argument_pid"
