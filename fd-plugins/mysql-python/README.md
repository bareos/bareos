# MySQL Plugin

This plugin makes a backup of each database found in mysql in a single file.
For restore select the needed database file, found in `/_mysqlbackups_` in the catalog.

## Prerequisites
The `mysqldump` and `mysql` command must be installed and user root (running the fd) must have read-access to the databases. See below, if you
want to use another user / password or want to restrict database access for the backup user.

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

### Include the Plugin in the fileset definition on the director
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

#### Options ####

##### Databases #####
By default all found databases are backuped, if the paramater 'db' is unset. You can have the plugin only certain databases saved, if you add
the parameter db=db1,db2, with a comma separated list of datbases.
Example plugin string:
```
               Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-mysql:db=test,mysql"
```
This will backup only the databases with name 'test' and 'mysql'.

##### Database host #####

By default the mysql connection is made to localhost. If want to connect to another host, use
```
mysqlhost=hostname
```

##### mysqldump options #####

By default the opition string
```
--events --single-transaction --add-drop-database --databases
```
is used.

You may overwrite the whole option string with parameter dumpoptions or just supress the latter two, which are needed to include drop and
create database statements into the dump. With drop_and_recreate set to 'true', these options are skipped.


##### Database access /  user and password  #####

By default the root user (the user, which runs the Bareos filedaemon) is used to connect to the database. We recommend that you set
a password for the user and store it in your my.cnf. You can however set a user and / or password as plugin options:
```
mysqluser=username:mysqlpassword=secret
```

Minimum privileges for the backup user are:
```
GRANT USAGE ON *.* TO 'bareos'@'localhost' IDENTIFIED BY 'bareos';
GRANT SELECT, LOCK TABLES, SHOW VIEW, EVENT, TRIGGER ON *.* TO 'bareos'@'localhost'
```
##### dumpbinary #####

Command (with or without full path) to create the dumps. Default: mysqldump

