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

PIDOF=/bin/false
PGREP=$(command -v pgrep)
PS=$(command -v ps)
prefix="bareos_test_lookup_$$"
name="${prefix}.x"
other_pid=""
regex_pid=""
exact_pid=""
trap 'kill ${other_pid} ${regex_pid} ${exact_pid} 2>/dev/null || true' EXIT

bash -c 'exec -a "$1" sleep 30' _ "${name}-other" &
other_pid=$!
bash -c 'exec -a "$1" sleep 30' _ "${prefix}Ax" &
regex_pid=$!
sleep 0.1

if [ -n "$(pidofproc "$name" 0)" ]; then
  echo "Found a different process instead of $name" >&2
  exit 1
fi
killproc "$name" 0 -TERM >/dev/null
kill -0 "$other_pid" "$regex_pid"

bash -c 'exec -a "$1" sleep 30' _ "/tmp/$name" &
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
