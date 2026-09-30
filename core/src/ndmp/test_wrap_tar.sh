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

# Round trip test for the wrap_tar NDMP formatter used by ndmjob.
# usage: test_wrap_tar.sh /path/to/wrap_tar

set -e
set -u
set -o pipefail

WRAP_TAR="$1"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

fail()
{
  echo "FAIL: $*" >&2
  exit 1
}

src="${work}/src"
longdir="${src}/$(printf 'd%.0s' {1..80})/$(printf 'e%.0s' {1..80})"
mkdir -p "${src}/sub" "${src}/empty-dir" "${longdir}"
echo "hello" >"${src}/file"
: >"${src}/empty-file"
head -c 300000 /dev/urandom >"${src}/sub/random"
echo "long" >"${longdir}/file-with-a-long-path"
ln -s file "${src}/link"
chmod 0750 "${src}/sub"

export WRAP_TAR_DUMPDATES="${work}/dumpdates"

# changes in the second the backup starts are included in the next level
sleep 1

# full backup (level 0) with file history
"${WRAP_TAR}" -c -I "${work}/full.idx" -E "FILESYSTEM=${src}" -E HIST=f \
  -E LEVEL=0 -E UPDATE=y >"${work}/full.img"

grep -q "^HF /file @" "${work}/full.idx" || fail "no file history for /file"
grep -q "^HF /sub/random @" "${work}/full.idx" || fail "no history for random"
grep -q "file-with-a-long-path" "${work}/full.idx" || fail "no long name"
grep -q "^0 [0-9]* ${src}\$" "${WRAP_TAR_DUMPDATES}" || fail "no dumpdates"

# the image must be a valid tar archive
if command -v tar >/dev/null; then
  tar -tf "${work}/full.img" | grep -q "^./sub/random$" \
    || fail "tar can not list the image"
fi

# the fhinfo must point to the header of the member
offset="$(sed -n 's|^HF /file @\([0-9]*\) .*|\1|p' "${work}/full.idx")"
dd if="${work}/full.img" bs=1 skip="${offset}" count=6 2>/dev/null \
  | grep -q "^./file" || fail "fhinfo of /file is wrong"

# restore everything to a new location, empty original name == all
"${WRAP_TAR}" -x -E "FILESYSTEM=${src}" "" @- "${work}/all" \
  <"${work}/full.img"
diff -r "${src}" "${work}/all" || fail "full restore differs"
[ "$(readlink "${work}/all/link")" = "file" ] || fail "symlink not restored"
[ "$(stat -c %a "${work}/all/sub")" = "750" ] || fail "mode not restored"

# restore a single file to a different name
"${WRAP_TAR}" -x -E "FILESYSTEM=${src}" /sub/random @0 "${work}/single/r" \
  <"${work}/full.img"
cmp "${src}/sub/random" "${work}/single/r" || fail "single restore differs"
[ "$(find "${work}/single" -type f | wc -l)" -eq 1 ] \
  || fail "single restore restored too much"

# restore a directory
"${WRAP_TAR}" -x -E "FILESYSTEM=${src}" /sub @- "${work}/dir" \
  <"${work}/full.img"
cmp "${src}/sub/random" "${work}/dir/random" || fail "dir restore differs"
[ ! -e "${work}/dir/file" ] || fail "dir restore restored too much"

# every name list entry gets a recovery result, the most specific entry wins
"${WRAP_TAR}" -x -I "${work}/rr.idx" -E "FILESYSTEM=${src}" \
  /sub @- "${work}/rr/sub" /sub/random @- "${work}/rr/random" \
  /missing @- "${work}/rr/missing" <"${work}/full.img"
grep -q "^RR 0 /sub$" "${work}/rr.idx" || fail "no result for /sub"
grep -q "^RR 0 /sub/random$" "${work}/rr.idx" || fail "no result for random"
grep -q "^RR 2 /missing$" "${work}/rr.idx" || fail "no not found result"
cmp "${src}/sub/random" "${work}/rr/random" || fail "nested entry differs"
[ ! -e "${work}/rr/sub/random" ] || fail "less specific entry used"

# extraction errors are reported for the entry, not as formatter failure
mkdir -p "${work}/ro"
chmod 0555 "${work}/ro"
if [ ! -w "${work}/ro" ]; then
  "${WRAP_TAR}" -x -I "${work}/ro.idx" -E "FILESYSTEM=${src}" \
    /file @- "${work}/ro/file" <"${work}/full.img" \
    || fail "extraction error made the formatter fail"
  grep -q "^RR 13 /file$" "${work}/ro.idx" || fail "no EACCES result"
fi
chmod 0755 "${work}/ro"

# file history from an existing image
"${WRAP_TAR}" -t -I "${work}/t.idx" -E HIST=f <"${work}/full.img"
grep -q "^HF /sub/random @" "${work}/t.idx" || fail "no -t file history"

# incremental (level 1) only contains changed files, but all directories
sleep 1
echo "new" >"${src}/sub/new-file"
"${WRAP_TAR}" -c -I "${work}/incr.idx" -E "FILESYSTEM=${src}" -E HIST=f \
  -E LEVEL=1 -E UPDATE=y >"${work}/incr.img"
grep -q "^HF /sub/new-file @" "${work}/incr.idx" || fail "new file missing"
grep -q "^HF /sub @" "${work}/incr.idx" || fail "directory missing"
if grep -q "^HF /sub/random @" "${work}/incr.idx"; then
  fail "unchanged file in incremental"
fi
grep -q "^1 [0-9]* ${src}\$" "${WRAP_TAR_DUMPDATES}" || fail "no level 1 date"

# a new full backup removes the higher level dumpdates
"${WRAP_TAR}" -c -E "FILESYSTEM=${src}" -E LEVEL=0 -E UPDATE=y >/dev/null
if grep -q "^1 " "${WRAP_TAR_DUMPDATES}"; then
  fail "level 1 dumpdate not removed"
fi

# paths escaping the restore location are refused
if command -v tar >/dev/null; then
  mkdir -p "${work}/evil/a"
  echo evil >"${work}/evil/x"
  (cd "${work}/evil/a" && tar -cf "${work}/evil.img" -P ../x)
  if "${WRAP_TAR}" -x -E "FILESYSTEM=/" "" @- "${work}/evil-restore/a" \
    <"${work}/evil.img" 2>/dev/null; then
    fail "unsafe path accepted"
  fi
  [ ! -e "${work}/evil-restore/x" ] || fail "unsafe path restored"
fi

echo "wrap_tar test passed"
