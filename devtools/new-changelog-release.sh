#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

# this script edits the current release in the release notes or adds a new
# unreleased section

prog="$(basename "$0")"
topdir="$(realpath "$(dirname "$0")/..")"
pushd "$topdir" >/dev/null

if [ $# -ne 2 ] && [ "${1:-}" != unreleased ]; then
  echo "usage: $prog {<version> <date>|unreleased}" >&2
  exit 1
fi

if [ ! -f CHANGELOG.md ]; then
  echo "No CHANGELOG.md to work with" >&2
  exit 1
fi

tmp="$(mktemp)"

function cleanup {
  rm -f "$tmp"
}
trap cleanup EXIT

if [ $# -eq 2 ]; then
  version="$1" date="$2" awk '
  {
    if($0 == "## [Unreleased]") {
      printf("## [%s] - %s\n", ENVIRON["version"], ENVIRON["date"])
    } else {
      print
    }
  }
  ' CHANGELOG.md >"$tmp"

else
  awk '
  BEGIN {
    found=0
  }
  /^## \[Unreleased\]/ {
    found=1
  }
  /^## \[/ {
    if(found == 0 && $0 != "## [Unreleased]") {
      printf("## [Unreleased]\n\n")
      found=1
    }
  }
  {
    print
  }
  ' CHANGELOG.md >"$tmp"
fi

if diff -u CHANGELOG.md "$tmp" >/dev/null; then
  echo "$prog: CHANGELOG.md is unchanged."
else
  mv "$tmp" CHANGELOG.md
  echo "$prog: updated CHANGELOG.md"
  echo "$prog: run update-changelog-links.sh to update link references"
fi
