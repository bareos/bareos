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

BAREOS_SCRIPTS_DIR=$(dirname "$1")
BAREOS_CONFIG_DIR=/unused
. "$1"

print_debug()
{
  :
}
bls_files_verbose()
{
  printf '%s\n' "$fixture"
}
set_error()
{
  compression_error="$*"
}
fail()
{
  echo "FAIL: $*" >&2
  failures=$((failures + 1))
}

failures=0
fixture='Begin Job Session Record: VolSessionId=1 VolSessionTime=100 JobId=10 DataLen=200
FileIndex=1 Stream=1 UATTR DataLen=100 VolSessionId=1 VolSessionTime=100 | attributes
 | /backup/requested.bin
FileIndex=1 Stream=29 COMPRESSED DataLen=20 VolSessionId=1 VolSessionTime=100 | GZIP, level=6, version=1'

if ! is_file_compressed storage volume requested.bin; then
  fail "ordinary compressed file rejected"
fi
compression_error=""
check_compression storage volume 10 requested.bin GZIP 6 >/dev/null
if [ -n "$compression_error" ]; then
  fail "ordinary compression algorithm check rejected"
fi

for compressed_stream in "30 SPARSE-COMPRESSED" "31 WIN32-COMPRESSED"; do
  read -r stream label <<<"$compressed_stream"
  description="GZIP, level=6, version=1, length=20"
  if [ "$stream" = 30 ]; then
    description="Sparse: StartAddress=4294967296. $description"
  fi
  fixture="Begin Job Session Record: VolSessionId=1 VolSessionTime=100 JobId=10 DataLen=200
FileIndex=1 Stream=1 UATTR DataLen=100 VolSessionId=1 VolSessionTime=100 | attributes
 | /backup/requested.bin
FileIndex=1 Stream=$stream $label DataLen=40 VolSessionId=1 VolSessionTime=100 | $description"
  if ! is_file_compressed storage volume requested.bin; then
    fail "$label file rejected"
  fi
  compression_error=""
  check_compression storage volume 10 requested.bin gzip 6 >/dev/null
  if [ -n "$compression_error" ]; then
    fail "$label algorithm and level rejected"
  fi
  for wrong_compression in "LZO1X 6" "GZIP 9"; do
    read -r algorithm level <<<"$wrong_compression"
    compression_error=""
    check_compression storage volume 10 requested.bin "$algorithm" "$level" >/dev/null
    if [ -z "$compression_error" ]; then
      fail "$label accepted the wrong algorithm or level: $wrong_compression"
    fi
  done
done

for encrypted_stream in "32 ENCRYPTED-COMPRESSED" "33 ENCRYPTED-WIN32-COMPRESSED"; do
  read -r stream label <<<"$encrypted_stream"
  fixture="FileIndex=1 Stream=1 UATTR DataLen=100 VolSessionId=1 VolSessionTime=100 | attributes
 | /backup/requested.bin
FileIndex=1 Stream=$stream $label DataLen=40 VolSessionId=1 VolSessionTime=100 | Compression algorithm 0x1234 found, but not supported!"
  if ! is_file_compressed storage volume requested.bin; then
    fail "$label file rejected"
  fi
done

for uncompressed_stream in "6 SPARSE-DATA" "11 WIN32-DATA"; do
  read -r stream label <<<"$uncompressed_stream"
  fixture="Begin Job Session Record: VolSessionId=1 VolSessionTime=100 JobId=10 DataLen=200
FileIndex=1 Stream=1 UATTR DataLen=100 VolSessionId=1 VolSessionTime=100 | attributes
 | /backup/requested.bin
FileIndex=1 Stream=$stream $label DataLen=40 VolSessionId=1 VolSessionTime=100 | "
  if is_file_compressed storage volume requested.bin; then
    fail "uncompressed $label file accepted as compressed"
  fi
done

for other_session in "2 100" "1 101"; do
  read -r other_id other_time <<<"$other_session"
  # Data and delayed attributes need not follow their own session label.
  fixture="Begin Job Session Record: VolSessionId=1 VolSessionTime=100 JobId=10 DataLen=200
Begin Job Session Record: VolSessionId=$other_id VolSessionTime=$other_time JobId=11 DataLen=200
FileIndex=1 Stream=29 COMPRESSED DataLen=20 VolSessionId=$other_id VolSessionTime=$other_time | GZIP, level=6, version=1
FileIndex=1 Stream=2 DATA DataLen=100 VolSessionId=1 VolSessionTime=100 |
FileIndex=1 Stream=1 UATTR DataLen=100 VolSessionId=1 VolSessionTime=100 | attributes
 | /backup/requested.bin
FileIndex=1 Stream=1 UATTR DataLen=100 VolSessionId=$other_id VolSessionTime=$other_time | attributes
 | /backup/other.bin"
  if is_file_compressed storage volume requested.bin; then
    fail "compressed record from session $other_session accepted for another file"
  fi
  compression_error=""
  check_compression storage volume 10 requested.bin GZIP 6 >/dev/null
  if [ -z "$compression_error" ]; then
    fail "algorithm from session $other_session accepted for another file"
  fi
done

fixture=${fixture/other.bin/requested.bin}
if ! is_file_compressed storage volume /backup/requested.bin; then
  fail "compressed matching path in an unspecified job rejected"
fi
compression_error=""
check_compression storage volume 10 requested.bin GZIP 6 >/dev/null
if [ -z "$compression_error" ]; then
  fail "same path in another job overrides the requested JobId"
fi
compression_error=""
check_compression storage volume 11 requested.bin GZIP 6 >/dev/null
if [ -n "$compression_error" ]; then
  fail "compressed file in the requested JobId rejected"
fi

fixture='FileIndex=1 Stream=1 UATTR DataLen=100 | attributes
 | /backup/requested.bin
FileIndex=1 Stream=29 COMPRESSED DataLen=20 | GZIP, level=6, version=1'
compression_error=""
if is_file_compressed storage volume requested.bin; then
  fail "records without session identity accepted"
fi
if [ -z "$compression_error" ]; then
  fail "missing session identity not reported"
fi
compression_error=""
if check_compression storage volume 10 requested.bin GZIP 6 >/dev/null; then
  fail "algorithm check accepted records without session identity"
fi
if [ -z "$compression_error" ]; then
  fail "algorithm check did not report missing session identity"
fi

echo "$failures compression helper failures"
test "$failures" -eq 0
