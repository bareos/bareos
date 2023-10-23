#!/bin/bash

source "defs"

Path="etc"
DirClients="$Path/bareos/bareos-dir.d/client/other-clients.conf.in"
rm -f "$DirClients"
touch "$DirClients"
for Client in "${Clients[@]}"; do
    Dir="$Path/$Client"
    rm -rf "$Dir"
    cp -r "$Path/dummyclient" "$Dir"
    NumThreads=$(echo "$Client" | cut -d'_' -f2)
    cat <<END_OF_DATA > "$Dir/bareos-fd.d/client/myself.conf.in"
Client {
       Name = $Client
       Working Directory = "@working_dir@"
       FD Port = @fd2_port@
       MaximumWorkersPerJob=$NumThreads
}
END_OF_DATA
    cat <<END_OF_DATA >> "$DirClients"
Client {
       Name = $Client
       Description = "Director resource for $Client"
       Address = @hostname@
       Password = "@fd_password@"
       FD Port = @fd2_port@
}
END_OF_DATA
done
