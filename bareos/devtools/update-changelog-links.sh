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

# This script will update link references at the end of a CHANGELOG.md
# 1. remove all references (lines starting with "[...]: "
# 2. look for reference tokens ([...]) and for each one that adheres to a
#    known format add a reference to the correct url.
#    A message will be shown for each unsupported token.
#
# Currently supported:
# * "unreleased"    -> link to github master branch
# * version numbers -> link to gitgub release
# * issue numbers   -> link to bugs.bareos.org
# * pull requests   -> link to github PR

topdir="$(realpath "$(dirname "$0")/..")"
pushd "$topdir" >/dev/null

if [ ! -f CHANGELOG.md ]; then
  echo "No CHANGELOG.md to work with" >&2
  exit 1
fi

tmp="$(mktemp)"

function cleanup {
  rm -f "$tmp"
}
trap cleanup EXIT

# strip all references first
grep -v -E '^\[[^]]+\]: ' CHANGELOG.md > "$tmp"

# grab every reference token, repetitions only once
readarray -t ref_tokens < <(
  grep -o -E '\[[^]]+\]' "$tmp" \
  | sed -e 's/^\[//;s/\]$//;' \
  | sort -uV)

for ref_tok in "${ref_tokens[@]}"; do
  case "$ref_tok" in
    Unreleased)
      echo '[unreleased]: https://github.com/bareos/bareos/tree/master' \
        >>"$tmp"
      ;;
    [0-9].[0-9].[0-9]|\
    [0-9].[0-9].[0-9][0-9]|\
    [0-9].[0-9][0-9].[0-9]|\
    [0-9].[0-9][0-9].[0-9][0-9]|\
    [0-9][0-9].[0-9].[0-9]|\
    [0-9][0-9].[0-9].[0-9][0-9]|\
    [0-9][0-9].[0-9][0-9].[0-9]|\
    [0-9][0-9].[0-9][0-9].[0-9][0-9])
      printf \
        "[%s]: https://github.com/bareos/bareos/releases/tag/Release%%2F%s\n" \
        "$ref_tok" \
        "$ref_tok" \
        >>"$tmp"
      ;;
    Issue\ \#[0-9]*|\
    \#[0-9]*)
      ticket="$(grep -o -E '[0-9]+$' <<< "$ref_tok")"
      printf "[%s]: https://bugs.bareos.org/view.php?id=%d\n" \
        "$ref_tok" \
        "$ticket" \
        >>"$tmp"
      ;;
    PR\ \#[0-9]*)
      pr="$(grep -o -E '[0-9]+$' <<< "$ref_tok")"
      printf "[%s]: https://github.com/bareos/bareos/pull/%d\n" \
        "$ref_tok" \
        "$pr" \
        >>"$tmp"
      ;;
    *)
      echo "Ignoring token [$ref_tok]" >&2
      ;;
  esac
done

mv "$tmp" CHANGELOG.md
