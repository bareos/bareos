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
# The binary is stripped and, unless BAREOS_SETUP_NO_UPX is set, additionally
# compressed with upx to further reduce its size for distribution.
#
# Usage: devtools/build-bareos-setup-static.sh [--arch amd64|arm64] [output-path]
#   --arch        target architecture, defaults to amd64. Can also be set via
#                 the BAREOS_SETUP_ARCH environment variable. Building for an
#                 architecture other than the host's requires qemu-user-static
#                 binfmt emulation to be registered.
#   output-path   defaults to ./bareos-setup, or ./bareos-setup-<arch> when
#                 --arch was given explicitly, so that builds for different
#                 architectures do not overwrite each other.

set -e
set -u

usage()
{
  cat <<'EOT'
Usage: devtools/build-bareos-setup-static.sh [--arch amd64|arm64] [output-path]
  --arch        target architecture, defaults to amd64. Can also be set via
                the BAREOS_SETUP_ARCH environment variable. Building for an
                architecture other than the host's requires qemu-user-static
                binfmt emulation to be registered.
  output-path   defaults to ./bareos-setup, or ./bareos-setup-<arch> when
                --arch was given explicitly, so that builds for different
                architectures do not overwrite each other.
EOT
}

arch="${BAREOS_SETUP_ARCH:-}"
arch_given=0
if [ -n "${arch}" ]; then
  arch_given=1
fi
positional=""
while [ $# -gt 0 ]; do
  case "$1" in
    --arch)
      if [ $# -lt 2 ]; then
        echo "build-bareos-setup-static.sh: --arch requires an argument" >&2
        exit 1
      fi
      arch="$2"
      arch_given=1
      shift 2
      ;;
    --arch=*)
      arch="${1#--arch=}"
      arch_given=1
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "build-bareos-setup-static.sh: unknown option $1" >&2
      usage >&2
      exit 1
      ;;
    *)
      if [ -n "${positional}" ]; then
        echo "build-bareos-setup-static.sh: too many arguments" >&2
        exit 1
      fi
      positional="$1"
      shift
      ;;
  esac
done
if [ $# -gt 0 ]; then
  positional="$1"
fi

arch="${arch:-amd64}"
case "${arch}" in
  amd64 | x86_64) arch="amd64" ;;
  arm64 | aarch64) arch="arm64" ;;
  *)
    echo "build-bareos-setup-static.sh: unsupported --arch '${arch}'," \
      "expected amd64 or arm64" >&2
    exit 1
    ;;
esac

topdir="$(realpath "$(dirname "$0")/..")"
# Tag the image per architecture so that builds for different targets don't
# invalidate each other's cached build environment.
image_tag="bareos-setup-static-build:${arch}"
default_output="bareos-setup"
if [ "${arch_given}" = 1 ]; then
  default_output="bareos-setup-${arch}"
fi
output_path="$(realpath -m "${positional:-${default_output}}")"
output_dir="$(dirname "${output_path}")"
output_name="$(basename "${output_path}")"

# Building for a foreign architecture only works if the kernel can execute the
# target binaries, i.e. if qemu-user-static is registered with binfmt_misc.
# Detect that up front, because otherwise the failure surfaces deep inside the
# container as an obscure "exec format error".
host_arch="$(uname -m)"
case "${host_arch}" in
  x86_64 | amd64) host_arch="amd64" ;;
  aarch64 | arm64) host_arch="arm64" ;;
esac
if [ "${arch}" != "${host_arch}" ]; then
  binfmt_name="qemu-aarch64"
  if [ "${arch}" = "amd64" ]; then
    binfmt_name="qemu-x86_64"
  fi
  if [ ! -e "/proc/sys/fs/binfmt_misc/${binfmt_name}" ]; then
    echo "build-bareos-setup-static.sh: building for ${arch} on a" \
      "${host_arch} host requires qemu-user-static binfmt emulation," \
      "but /proc/sys/fs/binfmt_misc/${binfmt_name} is not registered." >&2
    echo "Register it once with:" >&2
    echo "  podman run --rm --privileged" \
      "docker.io/multiarch/qemu-user-static --reset -p yes" >&2
    exit 1
  fi
fi

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
  (cd "${topdir}" && cmake -P write_version_files.cmake >/dev/null)
  generated_version_file=1
fi
cleanup()
{
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
  (cd "${topdir}" && cmake -P bareos-setup-vue/build-dist.cmake)
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
  --platform "linux/${arch}" \
  -t "${image_tag}" \
  "${topdir}/devtools/bareos-setup-static-build"

# The source tree is mounted read-only; the build itself happens in a
# container-local directory (not bind-mounted) to avoid permission/ownership
# issues, and only the resulting binary is copied out to the host via the
# /out bind mount.
"${container_engine}" run --rm \
  --platform "linux/${arch}" \
  -e "BAREOS_SETUP_NO_UPX=${BAREOS_SETUP_NO_UPX:-}" \
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
    if [ -z \"\${BAREOS_SETUP_NO_UPX:-}\" ]; then
      # upx does not support every target it can be installed for, so treat a
      # compression failure as a non-fatal size optimisation that was skipped
      # rather than as a build error. The uncompressed binary is fully usable.
      if ! upx --best --lzma '/out/${output_name}'; then
        echo \"Note: upx could not compress the ${arch} binary;\" \
          \"shipping it uncompressed.\" >&2
        # upx leaves a partially written file behind when it fails.
        cp /build/core/src/bareos-setup/bareos-setup '/out/${output_name}'
      fi
    fi
  "

echo "Built static bareos-setup binary for ${arch}: ${output_path}"
file "${output_path}" || true
