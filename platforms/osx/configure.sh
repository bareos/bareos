#!/bin/sh

# TODO: why /usr/local? Use /usr instead?
BAREOS_PREFIX=/usr/local
#BAREOS_PMDOC=${WORKING_DIR}/installer.pmdoc

# Flags for the toolchain
CONFIGFLAGS="--enable-client-only \
    --prefix=${BAREOS_PREFIX} \
    --with-archivedir=${BAREOS_PREFIX}/var/bareos \
    --with-configtemplatedir=${BAREOS_PREFIX}/lib/bareos/defaultconfigs \
    --with-scriptdir=${BAREOS_PREFIX}/lib/bareos/scripts \
    --with-plugindir=${BAREOS_PREFIX}/lib/bareos/plugins \
    --with-fd-password=XXX_REPLACE_WITH_CLIENT_PASSWORD_XXX \
    --with-mon-fd-password=XXX_REPLACE_WITH_CLIENT_MONITOR_PASSWORD_XXX \
    --with-basename=XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX \
    --with-hostname=XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX \
    --with-python \
    "

# failed to compile console
#    --disable-conio --enable-readline \


#export CPPFLAGS=
#export CFLAGS="-g -Wall -O2"
export CFLAGS="-g -O2"
export CXXFLAGS="${CFLAGS}"
#export LDFLAGS=

./configure ${CONFIGFLAGS} "$@"

