#!/bin/sh

SOURCE=$1
DEST=$2

for i in `sed -n -r "s|File: (etc/bareos/bareos-dir.d/.*)|${DEST}/\1|p" $SOURCE/debian/univention-bareos.univention-config-registry`; do
    mkdir -p "`dirname $i`"
    ln -s /usr/share/univention-bareos/bareos-dir-restart "$i" || true
done

exit 0
