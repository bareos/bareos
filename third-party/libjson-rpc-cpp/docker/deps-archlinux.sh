#!/bin/bash
set -evu
pacman -Syyu --noconfirm \
    sudo \
    sed \
    grep \
    awk \
    fakeroot \
    wget \
    cmake \
    make \
    gcc \
    git \
    jsoncpp \
    libmicrohttpd \
    curl \
    hiredis \
    redis \
    argtable \
    p11-kit