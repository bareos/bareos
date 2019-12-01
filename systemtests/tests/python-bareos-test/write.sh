#!/bin/sh

file=$1
shift

printf "date=%s\n" "$(LANG=C date)" | tee --append $file
for i in "$@"; do
    echo "$i" | tee --append $file
done
