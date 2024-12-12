# chunk_check

chunk_check.py is a python script that will check chunked volumes.

For a given pool `--pool` and a configured chunk size `--chunk_size`
it will iterate all volumes of that pool.

For each volume, the total size is verified to check if total chunks
size and catalog size are similar. The number of chunks is checked
to see if they are sequential and that all except last one are sized
with the defined chunk size.

Limitation: the script expect that all volumes of the pool have been
created by the same device, so have the same `MediaType`and same
`Storage`

## installation

Install python3-bareos if not yet on your system.

Place the script in an appropriate location
`/usr/lib/bareos/scripts/` 

Prepare a dedicated console with pair user/password

```conf 
Console {
  Name = "admin-notls"
  Password = "secret"
  Description = "Restricted console used by chunk_check.py script"
  Profile = "operator"
  TLS Enable = No
}
```

be sure to have .api available in operateor profile

```
Profile {
   Name = operator
   Description = "Profile allowing normal Bareos operations."

   Command ACL = !.bvfs_clear_cache, !.exit, !.sql
   Command ACL = !configure, !create, !delete, !purge, !prune, !sqlquery, !umount, !unmount
   Command ACL = .api
   Command ACL = *all*

   Catalog ACL = *all*
   Client ACL = *all*
   FileSet ACL = *all*
   Job ACL = *all*
   Plugin Options ACL = *all*
   Pool ACL = *all*
   Schedule ACL = *all*
   Storage ACL = *all*
   Where ACL = *all*
}

```


## usage


```bash
s3cfg=$(pwd)/etc/s3cfg bucket=bareos-backups \
python3 ./chunk_check.py \
    --dir_address="localhost" \
    --dir_port=9101 \
    --console_user="admin-notls" \
    --console_pw="secret" \
    --s3cmd=/usr/lib/bareos/scripts/s3cmd-wrapper.sh
    --chunk_size=131072000 \
    --pool="Full" \

UserWarning: Connection encryption via TLS-PSK is not available
(not available in 'ssl' and extra module 'sslpsk' is not installed).

(1/4) Checking volume cc01
  Skipping because of status = Error ...
(2/4) Checking volume cc02
  ERROR: chunk 1 has index 2.  Missing chunk ?
(3/4) Checking volume cc03
  OK
(4/4) Checking volume cc04
  OK

```


