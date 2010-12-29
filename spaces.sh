#!/bin/bash

FIND=$(which find)
SED=$(which sed)

# look for every single source file and remove the trailing spaces
$FIND $1 -name \*.h -o -name \*.c -exec $SED -i 's:\s\+$::g' {} \; > /dev/null 2>&1
