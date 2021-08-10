## Bareos Plugin for OpenVZ/Virtuozzo 7 containers

This plugin allows the Bareos filedaemon to backup and restore OpenVZ/Virtuozzo 7 container environments.
It is written in Python and calls the `prlctl` and `ploop` commands to handle the backup of the container.

### Features
* Backup of stopped container
* Backup of running container (without downtime/freeze)
* Restore of container
* Restore with different disksize possible
* Restore of files inside a running container
* Copy-on-write snapshots before backup (if CT is running)
* automatic merging of COW snapshots after backup finished (without downtime/freeze)
* Lockfiles can be used to
  * prevent multiple instances
  * prevent other tools (like `pcompact`) from running during backup
* Supports excludes via parameter
* Supports blocker files via parameter
  Which can be created inside the container and prevent a backup

### Requiremnents
The plugin must be installed on a server which run VZ7 containers with default layout.

* OpenVZ/virtuozzo 7 is used (CentOS 7 based)
* Bareos 20 is used with Python3 supported (Python2 might still work)
* container uses ploop image on default path
* container has only _one_ ploop image in use

### Parameters
* `name` (mandatory): The name of the container
* `uuid` (mandatory): The UUID of the container
* `blocker` (optional): A glob which defines blocker files inside the container
* `lockfile` (optional): The path of the lockfile on the VZ7 host
* `excluded_backup_paths` (optional): Comma seperate paths to exclude from the backup
* `restore` (optional): selects if a full container (`=ct`) restore or a file restore (`=file`) should be done. Defaults to `ct`

### Known issues
#### 'accurate' not working with file restores
When restoring files inside a running container, they loose their mtime and are create with the current timestamps.
This error message is probably related:
```
/usr/lib64/bareos/plugins/BareosFdPluginVz7CtFs.py:385: RuntimeWarning: Writing negative value into unsigned field
  statpacket.atime = osstat.st_atime
```

#### UTF-8 filename handling can crash Bareos FD with Python 3
Since Bareos 20 with Python 3 there is a bug with (Bareos internal Ticket TT4200769) in the handling of bad UTF-8 encoded filenames
which will crash the FD.
The plugin uses `os.walk` which on Python 2 return a binary strings and on Python 3 return UTF-8 encoded strings.
Therefore the result changed and this triggers a bug in Bareos FD Core:

Python 2: `'K\xdcBLER.csv'` \
Python 3: `'K\udcdcBLER.csv'`

### Example Usage

In the fileset generic values can be configured
```fileset.conf
FileSet {
  Name = "v7_ct"
  Include {
    Options {
      Signature = SHA256
    }
    Plugin = "python3:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vz7_ct_fs:blocker=tmp/backup_blocker:lockfile=/tmp/bareos_backup_cts"
  }
}
```

Additional parameters can be set in the job config:
```job.conf
Job {
  Name = backup-testcustomer.testproject.q3arena
  Client = testcustomer
  Storage = default
  JobDefs = backup-vz7
  FileSet = v7_ct
  FdPluginOptions = "python3:name=testcustomer.testproject.q3arena:uuid=2d109ae4-5231-475f-bc44-5cabf5d8503b:excluded_backup_paths='/home/admin'"
}
```

With these configs you can run a backup with

```
run job=backup-testcustomer.testproject.q3arena
```

Or restore a full container with
```
restore client=testhost restoreclient=testhost jobid=2342 pluginoptions="python:restore=ct:disk=250" all done
```

Or restore files inside a running container with
```
restore file=/etc/resolv.conf client=testserver restoreclient=testserver restorejob=restore-files \
  pluginoptions="python:name=bareos-plugin:uuid=4b16639b-dbb9-45a5-b4c2-9f7ae90dfe2c:restore=files" \
  regexwhere=!^/mnt/bareos/2d109ae4-5231-475f-bc44-5cabf5d8503b/!/vz/root/2d109ae4-5231-475f-bc44-5cabf5d8503b/tmp/bareos-restore/! \
  fileset=vz7_ct jobid=2342
```
Not need to define extra parameters in `restore-files` Job.
