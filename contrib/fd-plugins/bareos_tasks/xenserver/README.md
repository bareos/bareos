# Bareos FileDaemon XenServer Plugin
This plugin makes backup of Xen Server. It can be used to save:

1. VM: using the *xe vm-export* command. The vm should not be running.
2. HOST: using the *xe host-backup* command
3. POOL DATABASE: using the *xe pool-dump-database* command

For restore select the needed file, found in */@XENSERVER* in the catalog.

## Prerequisites
The plugins has been developed and tested with Xen Server 7.1

You need the packages *bareos-filedaemon-python-plugin* installed on your client.

## Installation
1. Make sure you have met the prerequisites.
2. Install the files *BareosFdTaskClass.py*, *xenserver/BareosFdXenServerClass.py* and *xenserver/bareos-fd-xenserver.py* in your Bareos plugin directory (usually */usr/lib/bareos/plugins*)

## Configuration

Activate your plugin directory in the *fd* resource configuration on the client
```
FileDaemon {                          
    Name = client-fd
    ...
    Plugin Directory = /usr/lib/bareos/plugins
}
```

nclude the Plugin in the fileset definition on the director
```
FileSet {
    Name = "client-data"
        Include  {
            Options {
                compression = LZO
                signature = MD5
            }
            File = /etc
            #...
            Plugin = "python:module_path=/usr/lib/bareos/plugins:module_name=bareos-fd-xenserver"
        }
    }
}
```

### Options
You can append options to the plugin call as key=value pairs, separated by ':'.
Please read more about the Bareos Python Plugin Interface here: http://doc.bareos.org/master/html/bareos-manual-main-reference.html#Python-fdPlugin

Example plugin options:
```
    Plugin = "python:module_path=/usr/lib/bareos/plugins:module_name=bareos-fd-xenserver:host_backup=yes"
```

#### folder
Virtual folder used in catalog. Default: *@XENSERVER*

#### vms
Comma separated list of VM names to backup. Default: unset

#### host_backup
Set to *no* to disable host backup. Default: *yes*

#### pool_dump_database
Set to *no* to disable pool dump database. Default: *yes*
