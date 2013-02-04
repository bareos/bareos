#!/bin/sh
aclocal -I m4
autoreconf -i -f
./configure $@
