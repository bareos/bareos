#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib/vmware-vix-disklib/lib64
exec bareos_vadp_dumper "$@"
