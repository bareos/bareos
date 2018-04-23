# The Bareos MySQL / MariaDB Percona xtrabackup Plugin

This plugin uses Perconas xtrackup tool, to make full and incremental backups of Mysql / MariaDB databases.

The key features of xtrabackup are:

- Incremental backups
- Backups that complete quickly and reliably
- Uninterrupted transaction processing during backups
- Savings on disk space and network bandwidth
- Higher uptime due to faster restore time

Incremental backups only work for INNODB tables, when using MYISAM, only full backups can be created.

## Prerequisites
You need to have the 'mysql' client program and the xtrabackup tool installed.
More about xtrabackup: https://www.percona.com/software/mysql-database/percona-xtrabackup

You will also need the package *bareos-filedaemon-python-plugin* installed on your client.

## Compatibility

There are different versions of _xtrabackup_ available. Older versions required an extra binary called _innobackupex_, especially when dealing with myISAM tables. In newer versions, _innobackupex_ is just a sysmbolic link to _xtrabackup_.

We've tested some versions of xtrabackup together with the plugin:

| xtrabackup version | Status | Remarks |
| -----------------: |:------:| -------:|
|2.0.8| - | InnoDB only seems to work |
|2.3.5| + | |
|2.4.4| + | |

We've used the official Percona rpms on Centos 6 for testing.

## Installation ##

1. Make sure you have met the prerequisites.
2. Install the files *BareosFdPercona.py* and *bareos-fd-percona.py* in your Bareos plugin directory (usually */usr/lib64/bareos/plugins*)


## Configuration ##

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
                Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-percona"
        }
}
```

#### Options ####

You can append options to the plugin call as key=value pairs, separated by ':'.
Please read more about the Bareos Python Plugin Interface here: http://doc.bareos.org/master/html/bareos-manual-main-reference.html#Python-fdPlugin


##### defaultsfile ####

This parameter allows to specify a defaultsfile that shall be used for mysql(client) and *xtrabackup* command line utilities.
Example:

```
Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-percona:mycnf=/path/to/your/my.cnf"
```

##### dumpbinary #####

Command (with or without full path) to create the dumps. Default: *xtrabackup*

##### dumpoptions #####

Options to be used with the dumpbinary.
Default: 
   --backup --stream=xbstream

##### extradumpoptions #####

Additional options appended to dumpoptions.

###### Choosing databases ######

By default all found databases are backed up. You can restrict this
using the dumpoptions or extradumpoptions parameter. If you modify
dumpoptions, be careful that you include all necessary options.  See
*xtrabackup* documentation for details.


##### restorecommand
Command used for restoring, default: 
   xbstream -x -C

##### strictIncremental #####
Default: False

By default, an incremental will create data, even if the Log Sequence Number (LSN) wasn't increased since last backup. This is to ensure, that eventual changes to
MYISAM tables get into the backup. MYISAM does not support incremental backups, you will always get a full bakcup of these tables.

If set to true, no data will be written into backup, if the LSN wasn't changed.

##### log #####
Default: false

By default, no extra logfile is written on the FD running the plugin. If you want to have some additional debug information, you might specify a
logfile here. If you set a filename with path, this will be used. If you specify just a filename without path, the default path for logs
*/var/log/bareos/* will be prepended.

If you use a logfilename that matches */var/log/bareos/bareos\*.log*, it will be handled by logrotate.

## Backup ##

When running full backups, the plugin will call the _xtrabackup_ command with the according options. Format is _xbstream_. LSN information
will be written into a temporary directory, using the _--extra-lsndir_ option. The information (LSN) will be used to write a so called
restore object. This information is needed for following incremental jobs, so they are aware of the previous backups (and how far by
means of LSN) they went.

## Restore ##

With the usual Bareos restore mechanism a file-hierarchy will be created on the restore client under the default restore location:

*/tmp/bareos-restores/_percona/<jobid>*

Each restore job gets an own subdirectory, because Percona expects an empty directory. In that subdirectory,
a new directory is created for every backup job that was part of the Full-Incremental sequence.

The naming scheme is:
*fromLSN_toLSN_jobid*

Example:
```
/tmp/bareos-restores/_percona/351/
├── 00000000000000000000_00000000000010129154_0000000334
├── 00000000000010129154_00000000000010142295_0000000335
├── 00000000000010142295_00000000000010201260_0000000338
```  

This example shows the restore tree for restore job with ID 351. First subdirectory has all files
from the first full backup job with ID 334. It starts at LSN 0 and goes until LSN 10129154.

Next line is the first incremental job with ID 335, starting at LSN 10129154 until 10142295.
The third line is the 2nd incremental job with ID 338.

To further prepare the restored files, use the *xtrabackup --prepare* command. Read https://www.percona.com/doc/percona-xtrabackup/2.4/xtrabackup_bin/incremental_backups.html
for more information.


## Troubleshooting ##

If things don't work as expected, make sure:

- Bareos FileDaemon (FD) works in general, so that you can make simple file backups  and restores
- Bareos FD Python plugins work in genreral, try one of the shipped simple sample plugins
- Make sure *xtrabackup* works as user root, MySQL access needs to be configured properly

If this all does not help, you can start the Bareos FD in Debug mode and look deeper into it.

Support is available here: https://www.bareos.com
