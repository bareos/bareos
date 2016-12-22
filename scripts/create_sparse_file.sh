#!/bin/sh

filename=${1:-sparse.dat}
size=${2:-100M}

echo "start" > $filename
dd if=/dev/zero of=$filename bs=1 count=0 seek=$size 2>/dev/null
echo "end" >> $filename

size="`du --block-size=1 --apparent-size ${filename} | cut -f 1`"
realsize="`du --block-size=1 ${filename} | cut -f 1`"

printf "$filename created.\n"
printf "size=%s\n" "$size"
printf "realsize=%s\n" "$realsize"

if [ "$realsize" -gt "$size" ]; then
    printf "ERROR: realsize has to be smaller than size.\n"
    exit 1
fi
