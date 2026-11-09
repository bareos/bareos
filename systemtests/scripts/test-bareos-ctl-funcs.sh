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

if [ "${2:-}" = --delayed-exec ]; then
  bash()
  {
    exec bash -c 'sleep 0.3; exec bash "$@"' _ "$@"
  }
  export -f bash
fi

wait_for_process_argv0()
{
  local process_pid="$1"
  local expected="$2"
  local attempt args
  for ((attempt = 0; attempt < 100; attempt++)); do
    if ! args=$("$PS" -o args= -p "$process_pid" 2>/dev/null); then
      echo "Process $process_pid exited before starting $expected" >&2
      return 1
    fi
    case "$args" in
      "$expected" | "$expected "*) return 0 ;;
    esac
    sleep 0.05
  done
  echo "Timed out waiting for process $process_pid to start $expected; got: $args" >&2
  return 1
}

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
pidof_mock=$(mktemp)
trap 'kill ${other_pid} ${regex_pid} ${exact_pid} ${config_other_pid} ${config_exact_pid} ${config_trailing_pid} 2>/dev/null || true; rm -f -- "$pidof_mock"' EXIT

bash -c 'exec -a "$1" python3 -c "import time; time.sleep(30)"' _ "${name}-other" &
other_pid=$!
bash -c 'exec -a "$1" python3 -c "import time; time.sleep(30)"' _ "${prefix}Ax" &
regex_pid=$!
wait_for_process_argv0 "$other_pid" "${name}-other"
wait_for_process_argv0 "$regex_pid" "${prefix}Ax"

if [ -n "$(pidofproc "$name" 0)" ]; then
  echo "Found a different process instead of $name" >&2
  exit 1
fi
killproc "$name" 0 -TERM >/dev/null
kill -0 "$other_pid" "$regex_pid"

bash -c 'exec -a "$1" python3 -c "import time; time.sleep(30)"' _ "/tmp/$name" &
exact_pid=$!
wait_for_process_argv0 "$exact_pid" "/tmp/$name"

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
bash -c 'exec -a "$1" python3 -c "import time; time.sleep(30)" -c "$2"' \
  _ "/tmp/$name" "${BAREOS_CONFIG_DIR}-other" &
config_other_pid=$!
bash -c 'exec -a "$1" python3 -c "import time; time.sleep(30)" -c "$2"' \
  _ "/tmp/$name" "$BAREOS_CONFIG_DIR" &
config_exact_pid=$!
bash -c 'exec -a "$1" python3 -c "import time; time.sleep(30)" -c "$2" --extra' \
  _ "/tmp/$name" "$BAREOS_CONFIG_DIR" &
config_trailing_pid=$!
wait_for_process_argv0 "$config_other_pid" "/tmp/$name"
wait_for_process_argv0 "$config_exact_pid" "/tmp/$name"
wait_for_process_argv0 "$config_trailing_pid" "/tmp/$name"

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

cat >"$pidof_mock" <<'SH'
#!/bin/bash
if [ "$#" -ne 1 ] || [ "$1" != "$LOOKUP_NAME" ]; then
  exit 1
fi
printf '%s\n' "$LOOKUP_OTHER_PID" "$LOOKUP_EXACT_PID"
SH
chmod +x "$pidof_mock"
if ! (
  export LOOKUP_NAME="$name" LOOKUP_OTHER_PID="$config_other_pid" \
    LOOKUP_EXACT_PID="$config_exact_pid"
  PIDOF="$pidof_mock"
  PGREP=""
  ps()
  {
    return 1
  }
  [ "$(pidofproc "/tmp/$name" 0)" = "$config_exact_pid" ]
); then
  echo "pidof did not match the basename with the exact config directory" >&2
  exit 1
fi

killproc "$name" 0 -TERM >/dev/null
kill -0 "$config_other_pid"
