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
#
# Builds a single, fully static bareos-setup binary inside the musl/Alpine
# container defined in devtools/bareos-setup-static-build/Dockerfile, so
# that it runs unmodified on every distro supported by the setup wizard.
#
# Usage: devtools/build-bareos-setup-static.sh [output-path]
#   output-path defaults to ./bareos-setup (in the current directory).

set -e
set -u

topdir="$(realpath "$(dirname "$0")/..")"
image_tag="bareos-setup-static-build"
output_path="$(realpath -m "${1:-bareos-setup}")"
output_dir="$(dirname "${output_path}")"
output_name="$(basename "${output_path}")"

mkdir -p "${output_dir}"

# cmake/BareosVersion.cmake is the officially supported way to provide
# version information when git metadata isn't available/usable inside the
# build environment (e.g. here, where the source tree is bind-mounted
# read-only and, if this is a git worktree, its .git file may point outside
# the mount). Generate it on the host, where git works normally, following
# the same mechanism used for source-tarball builds (devtools/dist-tarball.sh).
version_file="${topdir}/cmake/BareosVersion.cmake"
generated_version_file=0
if [ ! -r "${version_file}" ]; then
  ( cd "${topdir}" && cmake -P write_version_files.cmake >/dev/null )
  generated_version_file=1
fi
cleanup() {
  if [ "${generated_version_file}" = 1 ]; then
    rm -f "${version_file}"
  fi
}
trap cleanup EXIT

# The bareos-setup CMakeLists.txt always builds the Vue frontend directly
# in-source (bareos-setup-vue/{node_modules,dist}, both gitignored) via
# `npm ci`/`npm run build`, with no option to redirect that to an
# out-of-tree directory. Since the source tree is bind-mounted read-only
# into the container (to avoid the build polluting/ownership-mismatching
# the host checkout), pre-build the Vue dist bundle on the host, where npm
# is expected to be available, before the container-based native build.
dist_index="${topdir}/bareos-setup-vue/dist/index.html"
if [ ! -r "${dist_index}" ]; then
  ( cd "${topdir}" && cmake -P bareos-setup-vue/build-dist.cmake )
fi

container_engine="${CONTAINER_ENGINE:-}"
if [ -z "$container_engine" ]; then
  if command -v podman >/dev/null 2>&1; then
    container_engine=podman
  elif command -v docker >/dev/null 2>&1; then
    container_engine=docker
  else
    echo "build-bareos-setup-static.sh: neither podman nor docker found" >&2
    exit 1
  fi
fi

"${container_engine}" build \
  -t "${image_tag}" \
  "${topdir}/devtools/bareos-setup-static-build"

# The source tree is mounted read-only; the build itself happens in a
# container-local directory (not bind-mounted) to avoid permission/ownership
# issues, and only the resulting binary is copied out to the host via the
# /out bind mount.
"${container_engine}" run --rm \
  -v "${topdir}:/src:ro" \
  -v "${output_dir}:/out" \
  "${image_tag}" \
  sh -c "
    set -e
    mkdir -p /build
    cmake -S /src -B /build -G Ninja \
      -DBAREOS_SETUP_STATIC=ON \
      -Dacl=OFF -Dndmp=OFF \
      -DBUILD_TESTING=OFF -DENABLE_GRPC=OFF -DENABLE_SYSTEMTESTS=OFF \
      -DENABLE_WEBUI=OFF \
      -Ddocs-build-json=OFF -Dtraymonitor=OFF -Dwebui-vue=OFF
    cmake --build /build --target bareos-setup --parallel \"\$(nproc)\"
    cp /build/core/src/bareos-setup/bareos-setup '/out/${output_name}'
  "

echo "Built static bareos-setup binary: ${output_path}"
file "${output_path}" || true
