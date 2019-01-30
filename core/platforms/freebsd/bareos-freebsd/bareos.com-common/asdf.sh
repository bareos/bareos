#!/usr/local/bin/bash
sed 's|--with-\(.*\)=\(.*\)|-D\1=\2|g' |\
sed 's|--with-\(.*\) |-D\1=yes|g' |\
sed 's|--enable-\(.*\) |-D\1=yes|g'
