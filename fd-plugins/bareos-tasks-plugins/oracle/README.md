# Bareos FileDaemon Oracle Plugin
This plugin makes a backup of an oracle database using exp tool in a single file.
For restore select the needed database file, found in */@ORACLE* in the catalog.

## Prerequisites
The *exp* tool must be installed and user *oracle* must have read-access to the database.
See below, if you want to use another user.

You need the packages *bareos-filedaemon-python-plugin* installed on your client.

## Installation
1. Make sure you have met the prerequisites.
2. Install the files  *BareosFdTaskClass.py*, *oracle/BareosFdOracleClass.py* and *oracle/bareos-fd-oracle.py* in your Bareos plugin directory (usually */usr/lib/bareos/plugins*)

## Configuration

Activate your plugin directory in the *fd* resource configuration on the client
```
FileDaemon {                          
  Name = client-fd
  ...
  Plugin Directory = /usr/lib/bareos/plugins
}
```

Include the Plugin in the *fileset* definition on the director
```
FileSet {
    Name = "client-data"
        Include  {
            Options {
                    compression = LZ4
                    signature = MD5
            }
            File = /etc
            #...
            Plugin = "python:module_path=/usr/lib/bareos/plugins:module_name=bareos-fd-oracle:db_sid=DBSID:db_user=DBUSER:db_password=DBPASSWORD"
        }
    }
}
```

### Options
You can append options to the plugin call as key=value pairs, separated by ':'.
Please read more about the Bareos Python Plugin Interface here: http://doc.bareos.org/master/html/bareos-manual-main-reference.html#Python-fdPlugin

Example plugin options:
```
    Plugin = "python:module_path=/usr/lib/bareos/plugins:module_name=bareos-fd-oracle:db_sid=MAIN:db_user=oracle:db_password=secret"
```

#### folder
Virtual folder used in catalog. Default: *@ORACLE*

#### ora_exp
Command (with or without full path) to *exp* tool. Default: *exp*

#### ora_home
Full path to oracle home. Default: Required

#### ora_user
Username of the system user running the *ora_exp* tool. Default: *oracle*

#### ora_exp_options
Options to be used with the *exp* tool. Default: *full=yes statistics=none*

#### db_sid
Specifies the *sid* of the oracle instance to use. Required

#### db_user
Specifies the username of the user performing the export. Required

#### db_password
Specifies the password of the user performing the export. Required