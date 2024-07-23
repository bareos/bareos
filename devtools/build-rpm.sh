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

cleanup_paths=()
at_exit() {
  if [ "${#cleanup_paths[@]}" -gt 0 ]; then
    rm -rf "${cleanup_paths[@]}"
  fi
}
trap at_exit EXIT

pkg=bareos
use_tarball=0

while [ "${1:-}" ]; do
  case "$1" in
    --ulc)
      pkg=bareos-universal-client
      ;;
    --tarball)
      use_tarball=1
      tarball_opts=()
      ;;
    --fast-tarball)
      use_tarball=1
      tarball_opts=(--fast)
      ;;
    --)
      shift
      break # everything else will be passed to rpmbuild
      ;;
    *)
      echo "Unknown option '$1', to pass options to rpmbuild, add '--' as a separator"
      exit 1
      ;;
  esac
  shift
done

if [ -t ${BAREOS_VERSION:+x} ]; then
  BAREOS_VERSION="$(cmake -P get_version.cmake | sed -e 's,^-- ,,')"
fi

RPMDIR="$(rpm --eval "%{_rpmdir}")"
BUILDDIR="$(rpm --eval "%{_builddir}")"
mkdir -p "$RPMDIR" "$BUILDDIR"

CCACHE_BASEDIR="$(pwd)"
CCACHE_SLOPPINESS="file_macro"
export CCACHE_BASEDIR CCACHE_SLOPPINESS

# Make sure we use the default generator
unset CMAKE_GENERATOR


: "${spec:=core/platforms/packaging/$pkg.spec}"
spec="$(mktemp --suffix=.spec)"
cleanup_paths+=("$spec")
cat "core/platforms/packaging/$pkg.spec" >"$spec"

rpmbuild_opts=()
if [ $use_tarball -eq 1 ]; then
  rpm_sourcedir="$(mktemp -d)"
  cleanup_paths+=("$rpm_sourcedir")
  devtools/dist-tarball.sh "${tarball_opts[@]}" "$rpm_sourcedir"
  rpmbuild_opts+=("--define=_sourcedir $rpm_sourcedir")
  sed -i -e '/Source0: %{name}-%{version}.tar.gz/s/gz$/xz/' "$spec"

else
  rpmbuild_opts+=(--build-in-place)
fi

if rpm -q bareos-vmware-vix-disklib-devel; then
  rpmbuild_opts+=("--define=vmware 1")
fi

rpmdev-bumpspec \
  --comment="- See https://docs.bareos.org/release-notes/" \
  --userstring="Bareos Jenkins <noreply@bareos.com>" \
  --new="${BAREOS_VERSION}-${BUILD_ID:-1}%{?dist}" \
  "${spec}"

rpmbuild -bb --noclean "${rpmbuild_opts[@]}" "$@" "${spec}"
