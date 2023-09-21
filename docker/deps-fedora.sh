#!/bin/bash
set -evu
dnf -y install \
    gcc-c++ \
    jsoncpp-devel \
    libcurl-devel \
    libmicrohttpd-devel \
    catch-devel \
    git \
    cmake \
    make \
    argtable-devel \
    hiredis-devel \
    redis