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

`chech_chunk.py` can be installed system wide by installing package
`bareos-contrib-tools`.

Otherwise the script in an appropriate location like
`/usr/lib/bareos/scripts/` or `/usr/local/bin/`

Prepare a dedicated, named console to use with the script.
if you don't have tls psk python support, you have to set
`TLS Enable = No`

```conf 
Console {
  Name = "chunk_check"
  Password = "secret"
  Description = "Restricted console used by chunk_check.py tool"
  Profile = "chunk_check"
}
```

Make sure to have the `.api` and `list volumes` command allowed in
the profile, by either allowing them explicitly, or checking that
they are not.

```
Profile {
   Name = "chunk_check"
   Description = "Restricted profile to use with chunk_check.py tool"

   Command ACL = ".api"
   Command ACL = "list"

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
    --s3cmd=/usr/lib/bareos/scripts/s3cmd-wrapper.sh \
    --chunk_size=262144000 \
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

