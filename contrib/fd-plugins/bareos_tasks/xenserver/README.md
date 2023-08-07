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
                     "module_name=bareos_tasks.xenserver"
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
             "module_name=bareos_tasks.xenserver:"
             "host_backup=yes"
```

#### folder
Virtual folder used in catalog. Default: *@XENSERVER*

#### vms
Comma separated list of VM names to backup. Default: unset

#### host_backup
Set to *no* to disable host backup. Default: *yes*

#### pool_dump_database
Set to *no* to disable pool dump database. Default: *yes*
