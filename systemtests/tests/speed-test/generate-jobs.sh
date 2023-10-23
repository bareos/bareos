#!/bin/bash

source "defs"

Path="etc/bareos/bareos-dir.d/job"
for Client in "${Clients[@]}"; do
    File="$Path/Backup-$Client.conf.in"
    rm -f "$File"
    touch "$File"
    for Type in "${Types[@]}"; do
	cat <<END_OF_DATA >> "$File"
Job {
    Name = "Backup-$Client-$Type"
    JobDefs = "DefaultJob"
    Client  = "$Client"
    Fileset = "$Type"
}
END_OF_DATA
    done
done
