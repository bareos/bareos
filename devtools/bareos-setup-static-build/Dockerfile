# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# Build environment for producing a single, fully static bareos-setup
# binary that runs unmodified on every distro supported by the setup
# wizard, regardless of that distro's glibc/OpenSSL/Jansson version.
#
# Alpine/musl is used instead of glibc because bareos-setup uses no
# NSS-dependent libc functions (getpwnam, gethostbyname, getaddrinfo, ...)
# and performs all real HTTPS traffic via the system "curl" rather than
# in-process TLS, so a fully static musl binary carries none of the usual
# risks of static glibc linking.

FROM alpine:3.20

# Deliberately no nodejs/npm here: bareos-setup's CMakeLists.txt always
# runs the Vue frontend build directly in-source
# (bareos-setup-vue/{node_modules,dist}), which requires write access to
# the (read-only, bind-mounted) source tree. build-bareos-setup-static.sh
# pre-builds bareos-setup-vue/dist on the host instead, and relies on npm
# not being found in this image so CMake reuses that prebuilt dist as-is.
RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    python3 \
    openssl-dev \
    openssl-libs-static \
    jansson-dev \
    jansson-static \
    zlib-dev \
    zlib-static \
    lzo-dev \
    readline-dev \
    readline-static \
    postgresql16-dev \
    upx \
    git

WORKDIR /src
