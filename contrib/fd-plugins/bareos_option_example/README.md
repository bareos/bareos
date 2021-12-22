# FD Option Plugin stub 
Sample for an option plugin. The method handle_backup_file gets called for each file in the backup, you can use this
as template for your option plugin to implement some extra action on files.

## Prerequisites
You need the package `bareos-filedaemon-python-plugin` installed on your client.

## Configuration

### Activate your plugin directory in the fd resource conf on the client
```
FileDaemon {                          
  Name = client-fd
  ...
  Plugin Directory = /usr/lib64/bareos/plugins
}
```

### Include the Plugin in the fileset / option  definition on the director
```
FileSet {
    Name = "client-data"
       Include  {
                Options {
                        compression=GZIP
                        signature = MD5
                        Plugin = "python:"
                                 "module_path=/usr/lib64/bareos/plugins:"
                                 "module_name=bareos_option_example"
                }
                File = /etc
                #...
        }
}
```
