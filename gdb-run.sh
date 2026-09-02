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

set -u
set -o pipefail

if [ "$#" -eq 0 ]; then
  printf 'Usage: %s PROGRAM [ARGUMENT ...]\n' "$0" >&2
  exit 2
fi

if ! command -v gdb >/dev/null 2>&1; then
  printf '%s: gdb was not found in PATH\n' "$0" >&2
  exit 127
fi

log_file="$(mktemp "${TMPDIR:-/tmp}/gdb-run.XXXXXX")"
cleanup()
{
  rm -f "$log_file"
}
trap cleanup EXIT

gdb_commands=(
  -ex 'set pagination off'
  -ex run
  -ex 'source /usr/share/gdb/auto-load/usr/bin/python3.10-gdb.py'
  -ex 'thread apply all bt full'
  -ex 'info shared'
  -ex 'info file'
  -ex 'info proc mappings'
  -ex 'info registers'
  -ex 'disass'
)

program_name="$(basename -- "$1")"
case "$program_name" in
  python | python[0-9]*)
    gdb_commands+=(
      -ex 'printf "\n==== Python stacktrace (all threads) ====\n"'
      -ex 'thread apply all py-bt'
    )
    ;;
esac

gdb --quiet --batch "${gdb_commands[@]}" --args "$@" 2>&1 \
  | tee "$log_file"
gdb_status="${PIPESTATUS[0]}"

if grep -qE '^Program (received|terminated with) signal SIGSEGV' "$log_file"; then
  printf '\n==== SIGSEGV stacktrace (all threads) ====\n' >&2
  printf 'The complete stacktrace is included above.\n' >&2
fi

exit "$gdb_status"
