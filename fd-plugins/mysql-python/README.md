# MySQL Plugin

This plugin makes a backup of each database found in mysql in a single file.
For restore select the needed database file, found in `/_mysqlbackups_` in the catalog.

## Prerequisites
The `mysqldump` and `mysql` command must be installed and user root (running the fd) must have read-access to the databases.
You need the packages bareos-filedaemon-python-plugin installed on your client.

## Configuration

### Activate your plugin directory in the fd resource conf on the client
```
FileDaemon {                          
  Name = client-fd
  ...
  Plugin Directory = /usr/lib64/bareos/plugins
}
```

### Include the Plugin in the fileset definition of the job resource on the director
```
FileSet {
    Name = "client-data"
       Include  {
                Options {
                        compression=GZIP
                        signature = MD5
                }
                File = /etc
                #...
                Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-mysql"
        }
}
```
