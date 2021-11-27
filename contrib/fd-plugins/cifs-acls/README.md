# Bareos Plugin to backup NT ACLs from mounted CIFS filesystems

## Introduction

At the moment, Bareos only supports backing up NT ACLs (Windows) permissions using the Bareos Windows client.
However, backing up "shared drives" via SMB (i.e. NAS) doesn't work natively.
Years ago (<https://groups.google.com/g/bareos-users/c/WI0Ak5fpGcw?pli=1>) I found out that the way Bareos implements its file/folder lookups prohibits it:
* Bareos uses the`GetFileAttributes` API function to test if the file/folder is present and bails out if it fails. Unfortunately, that function doesn't work on remote drives.
* Back then, I tweaked the code to bypass that check and succeeded at being able to backup those drives with the ACLs permissions.
* It was however an ugly fix that could not be integrated into the official code.

Eight years forward and in the process of moving to a new Bareos environment, it looks like it is still not supported. Since then a lot changed and the Linux `cifs` module is much more stable and gives access natively to those ACLs as Extended Attributes. They can be read and parsed using the `getcifsacl` tool or with a simple `getxattr`system call to retrieve the `system.cifs_acl`attribute containing a raw data blob containing the ACLs. 

The `system.cifs_acl` attribute is not listed using the `listxattr` call by design and needs to be specified explicitly for retrieval. It is then not saved by default by Bareos.

After some thoughts, a native C PoC and some code reading, I decided that it would be easier to just implement it using a Python plugin for the `bareos-fd` client. And much more easier it was !

So here it is. 

Olivier Gagnon

## Requirements

* Recent Linux kernel and `cifs-utils`
* Bareos client compiled with `HAVE_XATTR`and Python3 support
* Python3 `xattr` module
* A mounted CIFS remote share to backup

### Test for requirements
* `cat /proc/fs/cifs/DebugData | grep XATTR` 
* `python -c 'import xattr'`
* `getcifsacl </mnt/mountedcifs/existingfile>`
* `getfattr -n system.cifs_acl </mnt/mountedcifs/existingfile>`

## Installation
* Copy the **BareosFdPluginCifsAcls.py** and **bareosfd-cifs-acls.py** files in your Bareos plugin directory (usually */usr/lib64/bareos/plugins*)

### Activate the python3 plugin in the fd resource conf on the client
```
Client (or FileDaemon) {                          
  Name = client-fd
  ...
  Plugin Directory = /usr/lib64/bareos/plugins
  Plugin Names = "python3"
}
```
### Include the Plugin in the fileset definition in the Options block
```
FileSet {
  Name = "cifs-files"
  Include {
    Options {
      ...
      Plugin =  "python3"
                ":module_path=/usr/lib64/bareos/plugins"
                ":module_name=bareosfd-cifs-acls"
    }
    File = "</mnt/mountedcifstobackup>"
    ...
  }
} 
```
* *Note*: A specific Restore Job should be created using that FileSet or manually changed to it when restoring.

## TODOs

* Add Options parameters (ie. white/black listing files/directories)
* Add more extensive checks depending of the environment (only tested on two Linux system)

## Authors

* Olivier Gagnon

## License

This project is licensed under the GNU Affero General Public License v3.0 License - see the [LICENSE](https://raw.githubusercontent.com/bareos/bareos/master/core/AGPL-3.0.txt) file for details
