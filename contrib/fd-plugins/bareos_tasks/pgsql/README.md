# Bareos FileDaemon PostgreSQL Plugin
This plugin makes backup of any database found in a PostgreSQL.
Each databases is stored in an individual files.
For restore select the needed database file, found in *@PGSQL*.

## Prerequisites
The *pg_dump* and *psql* command must be installed and user *postgres* must have read-access to the databases.
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
    Name = "postgresql-data"
    Include  {
        Options {
            Signature = XXH128
            Compression = LZ4
        }
        Plugin = "python:"
                 "module_name=bareos_tasks.pgsql"
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
             "module_name=bareos_tasks.pgsql:"
             "databases=db001,db002"
```

#### folder
Virtual folder used in catalog. Default: *@PGSQL*

#### psql
Command (with or without full path) to *psql* tool. Default: *psql*

#### pg_dump
Command (with or without full path) to *pg_dump* tool. Default: *pg_dump*

#### pg_dump_options
Options to be used with the *pg_dump* tool. Default: *--no-owner --no-acl*

#### pg_host
Specifies the host name of the machine on which the server is running.
If the value begins with a slash,
it is used as the directory for the Unix-domain socket.

#### pg_port
Specifies the TCP port
or the local Unix-domain socket file extension
on which the server is listening for connections.

#### pg_user
Specifies the user that will be used to run the *psql* and *pg_dump* tools.  If this option is not set, the default is *postgres*. If this option is set to the empty string (`pg_user=''`), then the user running *bareos-fd* is used.  Default: *postgres*

#### databases
Comma separated list of database names to backup, if unset all databases (except 'postgres', 'template1' and 'template0') are dumped. Default: unset

#### exclude
Comma separated list of database names exclude from backup. Default: unset
