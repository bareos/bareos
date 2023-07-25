# Bareos FileDaemon MySQL Plugin
This plugin makes backup of any database found in a MySQL. Each databases is stored in an individual files.
For restore select the needed database file, found in *@MYSQL* in the catalog.

## Prerequisites
The *mysqldump* and *mysql* command must be installed and user *root* must have read-access to the databases.
See below, if you want to use another user.

You need the packages *bareos-filedaemon-python-plugin* installed on your client.

## Installation
1. Make sure you have met the prerequisites.
2. Make the *bareos_tasks* directory available at the path specified with *module_path*

## Configuration

Activate your plugin directory in the *fd* resource configuration on the client
```
FileDaemon {                          
    Name = client-fd
    ...
    Plugin Directory = /usr/lib/bareos/plugins
}
```

Include the Plugin in the fileset definition on the director
```
FileSet {
    Name = "client-data"
        Include  {
            Options {
                Signature = XXH128
                Compression = LZ4
            }
            Plugin = "python:"
                     "module_path=/usr/lib/bareos/plugins:"
                     "module_name=bareos_tasks.mysql"
        }
    }
}
```

### Options
You can append options to the plugin call as key=value pairs, separated by ':'.
Please read more about the Bareos Python Plugin Interface here: https://docs.bareos.org/TasksAndConcepts/Plugins.html#python-fd-plugin

Example plugin options:
```
   Plugin = "python:"
            "module_path=/usr/lib/bareos/plugins:"
            "module_name=bareos_tasks.mysql:"
            "databases=db001,db002"
```

#### folder
Virtual folder used in catalog. Default: *@MYSQL*

#### mysql
Command (with or without full path) to *mysql* tool. Default: *mysql*

#### mysql_dump
Command (with or without full path) to *mysqldump* tool. Default: *mysqldump*

#### mysql_dump_options
Options to be used with the *mysqldump* tool. Default: *--events --single-transaction --add-drop-database --databases*

#### user
Username of the system user running the *mysql* and *mysqldump* tools. Default: *root* (as the bareos-fd normally runs as user *root*)

#### defaultsfile
This parameter allows to specify a defaultsfile that shall be used for mysql (client) and mysqldump command line utilities.

#### databases
Comma separated list of database names to backup, if unset all databases (except 'performance_schema' and 'information_schema') are dumped. Default: unset

#### exclude
Comma separated list of database names exclude from backup. Default: unset
