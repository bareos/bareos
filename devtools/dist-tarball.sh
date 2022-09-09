#!/usr/bin/env bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2022 Bareos GmbH & Co. KG
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
set -u

# We need our environment so that
#  * sort uses binary sort order
#  * all timestamps are treated as UTC
LANG=C
TZ=UTC
export LANG TZ

cleanup_version_files=0
at_exit() {
  if [ "$cleanup_version_files" -gt 0 ]; then
    rm -f cmake/BareosVersion.cmake
  fi
}
trap at_exit EXIT

git="${GIT:-$(command -v git)}"
cmake="${CMAKE:-$(command -v cmake)}"

# As we use extended features of GNU tar and the -z option from GNU sort, we
# check if gtar/gsort are available and prefer them over tar/sort
if [ -n "${TAR:-}" ]; then
  tar="$TAR"
else
  for cmd in gtar tar; do
    tar="$(command -v "$cmd" || :)"
    [ -n "$tar" ] && break
  done
fi

if [ -n "${SORT:-}" ]; then
  sort="$SORT"
else
  for cmd in gsort sort; do
    sort="$(command -v "$cmd" || :)"
    [ -n "$sort" ] && break
  done
fi

md5sum="$(command -v md5sum || :)"
md5cmd="$(command -v md5 || :)"
if [ -n "${md5sum}" ]; then
  list_file_cmd=( "$md5sum" --tag --binary )
elif [ -n "${md5cmd}" ]; then
  list_file_cmd=( "${md5cmd}" )
else
  list_file_cmd=( ls -l )
fi

if ! xz="$(command -v xz)"; then
  echo "Cannot find »xz« compressor executable." >&2
  exit 2
fi

# The directory we're going to pack up
topdir="$(dirname "$0")/.."

# The directory where the tarball will be written to
destdir=${1:-/tmp}
if [ ! -d "${destdir}" ]; then
  echo "Not a directory $destdir" >&2
  exit 1
fi

abs_destdir="$(realpath "${destdir}")"

pushd "${topdir}" >/dev/null

changed_files="$("$git" diff --name-only; "$git" diff --name-only --cached)"

if [ -n "${changed_files}" ]; then
  echo "You have modified/deleted files in your working-copy" >&2
  "$sort" -u <<< "${changed_files}"
  exit 2
fi

# get version of Bareos to name tarball and directory
version="$("$cmake" -P get_version.cmake | sed -e 's/^-- //' | sed -e 's/-pre/~pre/g')"
# get timestamp of latest commit to use as mtime for all files in tarball
timestamp="$("$git" show --quiet --date='format-local:%Y-%m-%d %H:%M:%S' --format='%cd')"

basename="bareos-${version}"
archive_file="${abs_destdir}/${basename}.tar.xz"

args=(--transform "s#^./#${basename}/#" \
      --format=ustar \
      --numeric-owner \
      --owner=0 \
      --group=0 \
      --mode="go-w" \
      "--mtime=$timestamp" \
      --null \
      --no-recursion \
    )

if [ ! -r cmake/BareosVersion.cmake ]; then
  cleanup_version_files=1
  cmake -P write_version_files.cmake >/dev/null
fi

(echo -ne 'cmake/BareosVersion.cmake\0'; "$git" ls-files -z) | \
"$sort" -u -z | \
"$tar" "${args[@]}" -cf - --files-from - | \
"$xz" --threads=0 -c -6 > "${archive_file}"

echo -n "Wrote tarball: "
"${list_file_cmd[@]}" "${archive_file}"
