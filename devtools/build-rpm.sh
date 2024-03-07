#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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
set -e
set -x
shopt -s nullglob

if [ -t ${BAREOS_VERSION:+x} ]; then
  BAREOS_VERSION="$(cmake -P get_version.cmake | sed -e 's,^-- ,,')"
fi

RPMDIR="$(rpm --eval "%{_rpmdir}")"
BUILDDIR="$(rpm --eval "%{_builddir}")"
for d in $RPMDIR $BUILDDIR ; do
      [ ! -d "$d" ] && mkdir -p "$d"
done

CCACHE_BASEDIR="$(pwd)"
CCACHE_SLOPPINESS="file_macro"
export CCACHE_BASEDIR CCACHE_SLOPPINESS

# Make sure we use the default generator
unset CMAKE_GENERATOR

spec="core/platforms/packaging/bareos.spec"

rpmdev-bumpspec \
  --comment="- See https://docs.bareos.org/release-notes/" \
  --userstring="Bareos Jenkins <noreply@bareos.com>" \
  --new="${BAREOS_VERSION}-${BUILD_ID:-1}%{?dist}" \
  "${spec}"

opts=()
if rpm -q bareos-vmware-vix-disklib-devel; then
  opts+=("--define=vmware 1")
fi
rpmbuild -bb --build-in-place --noclean "${opts[@]}" "$@" "${spec}"
