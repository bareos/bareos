# bareos-fd-mariadb-dump Plugin

This plugin makes a backup of each database found in a mariadbd cluster into a single file.
For restore select the needed database file, found in `@mariabackup@` in the restore tree.

## Prerequisites

The `mariadb-dump` and `mariadb` command must be installed and user root (running the fd) must have read-access to the databases.
See below, if you want to use another user / password or want to restrict database access for the backup user.

You need the packages `bareos-filedaemon-python3-plugin` installed on your client.

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
                Plugin = "python:"
                         "module_path=/usr/lib64/bareos/plugins:"
                         "module_name=bareos-fd-mariadb-dump:"
        }
}
```

#### Options ####

##### Databases #####

By default all found databases (except `performance_schema` and `information_schema`) are backuped, if the paramater 'db' is unset.
You can have the plugin only certain databases saved, if you add the parameter `db=db1,db2`, with a comma separated list of datbases.
Also you can exclude specific database from backup by `ignore_db=` option with a comma separated list ignored databases.

Example plugin string:
```
    Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-mariadb-dump:db=test,mariadb"
```

This will backup only the databases with name 'test' and 'mariadb'.

##### Database host #####

By default the mariadb connection is made to localhost. If want to connect to another host, use

```
host=hostname
```

##### mariadb-dump options #####

By default the option string
```
--events --single-transaction --add-drop-database --databases
```
is used.

You may overwrite the whole option string with parameter dumpoptions or just suppress the latter two, which are needed to include drop and
create database statements into the dump. With drop_and_recreate set to `false`, these options are skipped.

##### defaultsfile option ####

This parameter allows to specify a defaultsfile that shall be used for mariadb(client) and mariadb-dump command line utilities.

##### Database access /  user and password  #####

By default the root user (the user, which runs the Bareos filedaemon) is used to connect to the database. We recommend that you set
a password for the user and store it in your `/root/.my.cnf` or use the defaultsfile option to point to another client configuration file.
You can however set a user and / or password as plugin options:
```
user=username:password=secret
```

Minimum privileges for the backup user are:
```
GRANT USAGE ON *.* TO 'bareos'@'localhost' IDENTIFIED BY 'bareos';
GRANT SELECT, LOCK TABLES, SHOW VIEW, EVENT, TRIGGER ON *.* TO 'bareos'@'localhost'
```

##### dumpbinary #####

Command (with or without full path) to create the dumps.
Default: mariadb-dump
