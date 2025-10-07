.. _BarriPlugin:

Barri Plugin
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Barri
   single: Barri Plugin

The Barri Plugin can create disaster recovery images of computers running the
Windows operating system. Barri stands for `Bareos Recovery Imager` and is a
tool collection for creating Windows disaster recovery images and for
recovering Windows Computers:

Barri offers a variety of options to create and recover barri disaster recovery images.

The Barri Plugin for the Windows Filedaemon is the most common way of creating
barri images.

Backup operation
^^^^^^^^^^^^^^^^
A Computer running the Bareos Filedaemon with the Barri plugin can create a
disaster recovery image out of the running Computer without interfering with
the normal operation.

A second option to create a barri disaster recovery file is to use the Windows
commandline tool `barri-cli.exe`. As `barri-cli.exe` uses the exact same functionality
to create the image as the plugin, it also creates the image during normal operation
and does not require a shutdown of the computer or similar.

Restore operation
^^^^^^^^^^^^^^^^^

There are multiple options to recover computer from a backup created with barri.

Two options use a linux live recovery system to recover the computer:

A Linux Filedaemon with the barri filedaemon plugin can be run inside of the
linux live system. The Linux Live system needs to be configured to have network
access and to start the linux filedaemon that needs to be connected to the
bareos system. The recovery then is performed by running a bareos restore
command restoring the barri backup directly to the attached disks inside of the
live system.

The second option to recover a barri backup using a Linux Live Image is to
recover the image via the `barri-cli` linux cli command. Therefore the existing
.barri backup needs to be restored to a local file on the backup server first.
This file then can be used to restore the computer from the linux commandline.
The imagefile can be made available via removable media, network or any other
means. Using the `barri-cli` makes a recovery possible in a complete disconnected
environment.


In cases where linux does not have a driver that supports the hardware and thus the
system drives are not visible from the linux system, there is a third option to recover
the `.barri` file from the windows command line using `barri-cli.exe` for windows.
This option requires Windows PE, which is a live windows that can be created and preloaded
with special drivers if required.

It is included in Bareos since :sinceVersion:`25.0.0: Barri Plugin`.

Installation
^^^^^^^^^^^^

Linux
  Install the package **bareos-filedaemon-barri-plugin** via your package manager (e.g. apt or dnf)
Windows
  Install the Windows installation package


Configuration of the Windows FD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Make sure to add or enable the following settings in your Windows |fd| configuration:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = "C:/Program Files/bareos/plugins"
     Plugin Names = "barri"
     ...
   }

After a restart of the bareos service on windows, the ouput of the **status client** command shows that the plugin
was successfully loaded:

.. code-block:: bconsole
   :caption: status client on windows |fd| with barri plugin loaded

   *<input>status client=windowsdr-fd</input>
   [...] 
   windr-fd Version: 25.0.0~pre1046.c3bf07897 (29 September 2025)  VSS Microsoft Windows 8 Professional (build 9200), 64-bit
   Daemon started 01-Okt-25 10:01. Jobs: run=0 running=0, self-compiled binary
    Sizeof: boffset_t=8 size_t=8 debug=0 trace=0 bwlimit=0kB/s
   Plugin Info:
    Plugin     : barri-fd.dll
    Description: This plugin allows you to backup your windows system for disaster recovery.
    Version    : 0.9.0 (Juli 2025)
    Author     : Sebastian Sura
    License    : Bareos AGPLv3
    Usage      : barri:unknown disks:unknown partitions:unknown extents:ignore disks=<disks to ignore>
   
     unknown disks: try to save disks, that contain no snapshotted data
     unknown partitions: try to save partitions, that contain no snapshotted data
     unknown extents: try to save even unsnapshotted parts of partitions
     disks to ignore: a comma-separated list of disk ids (i.e. '1,2,5') of disks
                      to not backup
   
   [...] 
   

Now a fileset and a job need to be configured on the bareos server.
Make sure to set the fileset option ``Portable = yes``.

.. note::

   Please be sure to set the fileset option ``Portable = yes``
   


.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Barri Plugin Job and FileSet definition

   Job {
     Name = "barri"
     Client = "windowsdr-fd"
     JobDefs = "DefaultJob"
     FileSet = "barri_fileset"
   }

   FileSet {
     Name = "barri_fileset"

     Include {
       Options {
            Signature = XXH128
            Compression = LZ4
            Portable = yes  # this is required
       }
       Plugin = "barri"
     }
   }


Backup with the Windows barri-fd Plugin 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::
   It is not possible to recover a backup created with the barri-fd plugin on windows back to a filedaemon with the barri plugin on windows.
   In a running windows it is not possible to overwrite the drives.

   

In order to make a backup, run the job defined above (``barri``) to make a disaster recovery image backup.


.. code-block:: bconsole
   :caption: running the disaster recovery backup with the windows filedaemon plugin

   *<input>run job=barri yes</input>
   *<input>messages</input>
     [...]
     [...] windr-fd JobId 1: Version: 25.0.0~pre1046.c3bf07897 (29 September 2025) Microsoft Windows 8 Professional (build 9200), 64-bit
     [...] windr-fd JobId 1: setting up vss
     [...] bareos-sd JobId 1: JustInTime Reservation: Finding drive to reserve.
     [...] bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
     [...] bareos-sd JobId 1: Using Device "FileStorage" (/var/lib/bareos/storage) to write.
     [...] bareos-sd JobId 1: Labeled new Volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
     [...] bareos-sd JobId 1: Moving to end of data on volume "Full-0001"
     [...] bareos-sd JobId 1: Ready to append to end of Volume "Full-0001" size=223
     [...] windr-fd JobId 1: creating a vss snapshot
     [...] windr-fd JobId 1: ... done! (=> Id = {FE763C79-9FC4-429F-80A1-458EAA09FE54})
     [...] windr-fd JobId 1: examining volume \\?\Volume{ea8f2bab-dd31-4ba5-aa39-482c24b2589e}\ (\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy8)
     [...] windr-fd JobId 1: ... done!
     [...] windr-fd JobId 1: examining volume \\?\Volume{106f2185-d422-407a-b928-198db1c4566d}\ (\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy9)
     [...] windr-fd JobId 1: ... done!
     [...] windr-fd JobId 1: examining volume \\?\Volume{5d9d24bf-b2fd-47a4-8577-594f29792d0e}\ (\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy10)
     [...] windr-fd JobId 1: ... done!
     [...] windr-fd JobId 1: collecting info from disk \\.\PhysicalDrive0
     [...] windr-fd JobId 1: => recorded!
     [...] windr-fd JobId 1: generating backup plan
     [...] windr-fd JobId 1: disk 0/partition 1 has no snapshotted data => recorded (special-partition)
     [...] windr-fd JobId 1: ... done!
     [...] bareos-sd JobId 1: Releasing device "FileStorage" (/var/lib/bareos/storage).
     [...] bareos-sd JobId 1: Elapsed time=00:16:57, Transfer rate=34.22 M Bytes/second
     [...] bareos-dir JobId 1: Insert of attributes batch table with 2 entries start
     [...] bareos-dir JobId 1: Insert of attributes batch table done
     [...] bareos-dir JobId 1: Bareos bareos-dir 25.0.0~pre1074.e81033063 (01Oct25):
     Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
     JobId:                  1
     Job:                    barri.2025-10-01_14.37.37_04
     Backup Level:           Full (upgraded from Incremental)
     Client:                 "windowsdr-fd" 25.0.0~pre1046.c3bf07897 (29Sep25) Microsoft Windows 8 Professional (build 9200), 64-bit,Windows-x64
     FileSet:                "barri_fileset" 2025-10-01 14:37:37
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         01-Okt-2025 14:37:37
     Start time:             01-Okt-2025 14:37:39
     End time:               01-Okt-2025 14:54:38
     Elapsed time:           16 mins 59 secs
     Priority:               10
     Allow Mixed Priority:   no
     FD Files Written:       2
     SD Files Written:       2
     FD Bytes Written:       34,806,828,300 (34.80 GB)
     SD Bytes Written:       34,806,828,529 (34.80 GB)
     Rate:                   34157,8 KB/s
     Software Compression:   66,0 % (lz4)
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Full-0001
     Volume Session Id:      3
     Volume Session Time:    1759317946
     Last Volume Bytes:      34,812,710,996 (34.81 GB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK


As the joblog tells us, two files have been backed up. With the command ``list files jobid=1`` we can check what files have been backed up:


.. code-block:: bconsole
   :caption: list of files that have been backed up

   *<input>list files jobid=1</input>
    [...]
    @barri@/windr-fd.barri
    @barri@/windr-fd.log

The plugin backed up two files:

``windr-fd.barri``
  This is the `.barri` file containing the disaster recovery data itself.
``windr-fd.log``
  This file contains a logfile that was recorded during the creation of the ``.barri`` file.


Restore with the Linux FD Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Start Live Linux. In this example we use Fedora42 Live Linux.
* Install bareos-filedaemon and bareos-filedaemon bareos-filedaemon-barri-plugin

* Configure:


.. code-block:: bareosconfig
   :caption: :file:`/etc/bareos/bareos-fd.d/client/myself.conf` on linux live system

   Client {
     Name = localhost-live-fd
     Plugin Directory = "/usr/lib64/bareos/plugins"
     Plugin Names = "barri"
   }


Now we need to determine which device the target disk or disks have.
This can be achieved by using the command :command:`lsblk` which shows us
the detected block devices of the computer:

.. code-block:: sh
   :caption: use ``lsblk`` to determine the target disk(s)
   :emphasize-lines: 9

   NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
   loop0         7:0    0     2G  1 loop /run/rootfsbase
   sda           8:0    1 117.2G  0 disk 
   ├─sda1        8:1    1   2.2G  0 part /run/initramfs/live
   └─sda2        8:2    1    30M  0 part 
   sdb           8:16   1 120.3G  0 disk 
   └─sdb1        8:17   1 120.3G  0 part /run/media/liveuser/SanDisk
   zram0       251:0    0     8G  0 disk [SWAP]
   <input>nvme0n1     259:0    0 238.5G  0 disk</input>

The demo system has currently two usb devices which show up as ``sda`` and ``sdb`` and a
nvme device shown as ``nvme0n1``. 
As the ``nvme0n1`` has a size of ``238.5G``, it is the device that we need to restore to.

The target device of the disaster recovery is passed to the barri filedaemon plugin via the plugin options as follows:

.. code-block:: bareosconfig

   barri:files=/dev/first_device_to_be_restored[,/dev/second_device_to_be_restored]...

Which results in our case to:

.. code-block:: bareosconfig

   barri:<input>files=/dev/nvme0n1</input>


.. code-block:: bconsole
   :caption: Example restore to local VMDK

   *<input>restore jobid=1</input>
   You have selected the following JobId: 1
   
   Building directory tree for JobId(s) 1 ...  
   2 files inserted into the tree.
   
   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.
   
   cwd is: /
   $ <input>mark *</input>
   3 files newly marked.
   $ <input>done</input>
   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.3.bsr
   
   The job will require the following
      Volume(s)                 Storage(s)                SD Device(s)
   ===========================================================================
      
       Full-0001                 File                      FileStorage              
   
   Volumes marked with "*" are online.
   
   
   2 files selected to be restored.
   
   Defined Clients:
   1: bareos-fd
   2: linuxlive-fd
   3: windowsdr-fd
   Select the Client (1-3): <input>2</input>
   Using Catalog "MyCatalog"
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.3.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         LinuxAll
   Backup Client:   linuxlive-fd
   Restore Client:  linuxlive-fd
   Format:          Native
   Storage:         File
   When:            2025-10-02 15:10:49
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no): <input>mod</input>
   Parameters to modify:
    1: Level
    2: Storage
    3: Job
    4: FileSet
    5: Restore Client
    6: Backup Format
    7: When
    8: Priority
    9: Bootstrap
   10: Where
   11: File Relocation
   12: Replace
   13: JobId
   14: Plugin Options
   Select parameter to modify (1-14): <input>14</input>
   Please enter Plugin Options string: <input>barri:files=/dev/nvme0n1</input>
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.3.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         LinuxAll
   Backup Client:   linuxlive-fd
   Restore Client:  linuxlive-fd
   Format:          Native
   Storage:         File
   When:            2025-10-02 15:10:49
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  barri:files=/dev/nvme0n1
   OK to run? (yes/mod/no): <input>yes</input>
   * you have messages
   <input>messages</input>
     [...] bareos-dir JobId 5: Version: 25.0.0~pre1074.e81033063 (01 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
     [...] bareos-dir JobId 5: Start Restore Job RestoreFiles.2025-10-02_15.15.46_16
     [...] bareos-dir JobId 5: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
     [...] bareos-dir JobId 5:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
     [...] bareos-dir JobId 5: Using Device "FileStorage" to read.
     [...] bareos-dir JobId 5: Connected Client: linuxlive-fd at linuxlive:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
     [...] bareos-dir JobId 5:  Handshake: Immediate TLS 
     [...] bareos-dir JobId 5:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
     [...] bareos-sd JobId 5: Connected File Daemon at linuxlive:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
     [...] bareos-sd JobId 5: Version: 25.0.0~pre1074.e81033063 (01 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
     [...] bareos-sd JobId 5: Ready to read from volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
     [...] bareos-sd JobId 5: Forward spacing Volume "Full-0001" to file:block 0:223.
     [...] localhost-live-fd JobId 5: Version: 25.0.0~pre1077.b73cecf21 (01 October 2025) Fedora 42
     [...] localhost-live-fd JobId 5: restore started (file = /tmp/bareos-restores/@barri@/windr-fd.barri)
     [...] localhost-live-fd JobId 5: Restoring 1 Disks
     [...] localhost-live-fd JobId 5: Restoring disk 0
     [...] localhost-live-fd JobId 5: disk 0 is neither a block device, nor a regular file.  Continuing without size check
     [...] bareos-sd JobId 5: End of Volume at file 8 on device "FileStorage" (/var/lib/bareos/storage), Volume "Full-0001"
     [...] bareos-sd JobId 5: End of all volumes.
     [...] bareos-sd JobId 5: Releasing device "FileStorage" (/var/lib/bareos/storage).
     [...] localhost-live-fd JobId 5: restore finished
     [...] localhost-live-fd JobId 5: starting restore of backup log into debug log (level = 300)
     [...] localhost-live-fd JobId 5: restore of backup log is done
     [...] localhost-live-fd JobId 5: restore finished
     [...] bareos-dir JobId 5: Bareos bareos-dir 25.0.0~pre1074.e81033063 (01Oct25):
     Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
     JobId:                  5
     Job:                    RestoreFiles.2025-10-02_15.15.46_16
     Restore Client:         "linuxlive-fd" 25.0.0~pre1077.b73cecf21 (01Oct25) Fedora 42,redhat
     Start time:             02-Okt-2025 15:15:48
     End time:               02-Okt-2025 15:33:27
     Elapsed time:           17 mins 39 secs
     Files Expected:         2
     Files Restored:         2
     Bytes Restored:         102,381,443,939
     Rate:                   96677,5 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Restore OK
   
   



Restore with the Linux barri-cli commandline tool
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^



Restore with the Windows barri-cli.exe commandline tool
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^



Restore using Bareos WebUI
''''''''''''''''''''''''''

Since :sinceVersion:`22.0.0: Barri Plugin: Restore using WebUI` it is possible to use the Bareos WebUI to restore Barri Plugin jobs.

When using the WebUI to restore a Barri Plugin job, it is **important** to set *Merge all client file sets* to **no** and *Merge all jobs up to the last full backup together* to **yes**. In the *File selection* all files must be selected. Only restoring selected virtual disks will probably not work and is currently unsupported. The Bareos WebUI will detect if a plugin based jobs is restore and will then show an additional *Plugin options* field, here a plugin options string starting with ``python:`` as described above can be entered.

.. image:: /include/images/bareos-webui-restore-with-pluginoptions.*
   :width: 80.0%

Restore to local VMDK File
^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Barri Plugin; VMDK files>`\

Since :sinceVersion:`15.2.3: Barri Plugin: restore to VMDK files` it is possible to restore to local VMDK files. That means, instead of directly restoring a disk that belongs to the VM, the restore creates VMDK disk image files on the filesystem of the system that runs the |fd|. As the VM that the backup was taken from is not affected by this, it can remain switched on while restoring to local VMDK. Such a restored VMDK file can then be uploaded to a
|vsphere| datastore or accessed by tools like `guestfish <https://libguestfs.org/guestfish.1.html>`_ to extract single files.

For restoring to local VMDK, the plugin option :strong:`localvmdk=yes` must be passed. The following example shows how to perform such a restore using :command:`bconsole`:

.. code-block:: bconsole
   :caption: Example restore to local VMDK

   *<input>restore</input>
   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"

   First you select one or more JobIds that contain files
   to be restored. You will be presented several methods
   of specifying the JobIds. Then you will be allowed to
   select which files from those JobIds are to be restored.

   To select the JobIds, you have the following choices:
        1: List last 20 Jobs run
        ...
        5: Select the most recent backup for a client
        ...
       13: Cancel
   Select item:  (1-13): <input>5</input>
   Automatically selected Client: vmw5-bareos-centos6-64-devel-fd
   The defined FileSet resources are:
        1: Catalog
        ...
        5: PyTestSetVmware-test02
        6: PyTestSetVmware-test03
        ...
   Select FileSet resource (1-10): <input>5</input>
   +-------+-------+----------+---------------+---------------------+------------------+
   | jobid | level | jobfiles | jobbytes      | starttime           | volumename       |
   +-------+-------+----------+---------------+---------------------+------------------+
   |   625 | F     |        4 | 4,733,002,754 | 2016-02-18 10:32:03 | Full-0067        |
   ...
   You have selected the following JobIds: 625,626,631,632,635

   Building directory tree for JobId(s) 625,626,631,632,635 ...
   10 files inserted into the tree.

   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.

   cwd is: /
   $ <input>mark *</input>
   10 files marked.
   $ <input>done</input>
   Bootstrap records written to /var/lib/bareos/vmw5-bareos-centos6-64-devel-dir.restore.1.bsr

   The job will require the following
      Volume(s)                 Storage(s)                SD Device(s)
   ===========================================================================

       Full-0001                 File                      FileStorage
       ...
       Incremental-0078          File                      FileStorage

   Volumes marked with "*" are online.

   10 files selected to be restored.

   Using Catalog "MyCatalog"
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/vmw5-bareos-centos6-64-devel-dir.restore.1.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         Linux All
   Backup Client:   vmw5-bareos-centos6-64-devel-fd
   Restore Client:  vmw5-bareos-centos6-64-devel-fd
   Format:          Native
   Storage:         File
   When:            2016-02-25 15:06:48
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no): <input>mod</input>
   Parameters to modify:
        1: Level
        ...
       14: Plugin Options
   Select parameter to modify (1-14): <input>14</input>
   Please enter Plugin Options string: <input>python:localvmdk=yes</input>
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/vmw5-bareos-centos6-64-devel-dir.restore.1.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         Linux All
   Backup Client:   vmw5-bareos-centos6-64-devel-fd
   Restore Client:  vmw5-bareos-centos6-64-devel-fd
   Format:          Native
   Storage:         File
   When:            2016-02-25 15:06:48
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  python: module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware: dc=dass5:folder=/: vmname=stephand-test02: vcserver=virtualcenter5.dass-it:vcuser=bakadm@vsphere.local: vcpass=Bak.Adm-1234: localvmdk=yes
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=639

Note: Since Bareos :sinceVersion:`15.2.3: Add additional python plugin options`
it is sufficient to add Python plugin options, e.g. by

.. code-block:: bareosconfig

   python:localvmdk=yes

Before, all Python plugin must be repeated and the additional be added, like:

.. code-block:: bareosconfig

   "python:module_name=bareos-fd-vmware:dc=dass5:folder=/:vmname=stephand-test02:vcserver=virtualcenter5.dass-it:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234:localvmdk=yes"

After the restore process has finished, the restored VMDK files can be found under \path{/tmp/bareos-restores/}:

.. code-block:: shell-session
   :caption: Example result of restore to local VMDK

   $ <input>ls -laR /tmp/bareos-restores</input>
   /tmp/bareos-restores:
   total 28
   drwxr-x--x.  3 root root  4096 Feb 25 15:47 .
   drwxrwxrwt. 17 root root 20480 Feb 25 15:44 ..
   drwxr-xr-x.  2 root root  4096 Feb 25 15:19 [ESX5-PS100] stephand-test02

   $ <input>ls -la "/tmp/bareos-restores/[ESX5-PS100] stephand-test02"</input>
   /tmp/bareos-restores/[ESX5-PS100] stephand-test02:
   total 7898292
   drwxr-xr-x. 2 root root       4096 Feb 25 15:19 .
   drwxr-x--x. 3 root root       4096 Feb 25 15:47 ..
   -rw-------. 1 root root 2075197440 Feb 25 15:19 stephand-test02_1.vmdk
   -rw-------. 1 root root 6012731392 Feb 25 15:19 stephand-test02.vmdk


Description of all Plugin Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Note that all plugin options that have been used at backup time, are passed on restore. The VM metadata is saved in restoreobjects both in the catalog DB and volume, it is used on restore if the VM must be recreated. Most options are used for both backup and restore, some which can be only used for restore start with **restore_**. Where nothing special is mentioned regarding restore, it is normally not necessary or useful to override that option on restore.

vcserver (mandatory on backup)
   Hostname (FQDN) or IP address of vCenter server. Restore to different vCenter Server is unsupported.

vcuser (mandatory on backup)
   Username for API access to vCenter, eg. administrator@vsphere.local

vcpass (mandatory on backup)
   Password for API access to vCenter

dc (mandatory on backup)
   Datacenter name. This can be optionally passed on restore to recreate the VM in a different datacenter.

folder (mandatory on backup)
   The VM folder in which the VM to be backed up resides. This must be given like a UNIX path with ``/`` as separator. On restore if defined will recreate the VM in a different folder. The given folder must exist before starting the restore.

vmname (mandatory on backup)
   The name of the VM to be backed up. On restore it is possible to override this option in order to recreate the VM with a different name.

vcthumbprint (optional)
   Thumbprint of the vCenter SSL Certificate, which is the SHA1 checksum of the SSL Certificate

transport (optional)
   Normally the transport mode mode will be autonegotiated, eg. if the system that runs this plugin is a VM has storage access to the datastore of the VM that's being backed up, the *hotadd* transport will be used. Otherwise the *nbd* or *nbdssl* transport. For details about transport modes see the VDDK documentation. This option can be used to force the given transport mode.

log_path (optional)
   The default log path is :file:`/var/log/bareos/`. A different path can be specified using this option, it will be used for ``bareos_vadp_dumper`` log files.

localvmdk (optional)
   Restore to local ``.vmdk`` file(s) instead of restore to VM. Default is *no*.

vadp_dumper_verbose (optional)
   When setting ``vadp_dumper_verbose=yes``, the option ``-v`` will be added when running ``bareos_vadp_dumper``. This can be helpful for debugging purposes.

verifyssl (optional)
   By default the validity of SSL certificates will be verified. By setting ``verifyssl=no`` this can be disabled.

quiesce (optional)
   The backed up VM will be triggered to quiesce its filesystems before creating a snapshot by default to increase the data consistency. This can fail or take very long on a VM which runs heavy I/O workload. When setting ``quiesce=no`` the quiescing will be skipped, but the snapshot may be inconsistent. It is not recommended to use this option, instead try stop heavy I/O load before snapshot. This could be possible by running pre-freeze and post-thaw actions which can be configured in Barri tools, see Barri documentation for details.

cleanup_tmpfiles (optional)
   By default, temporary files created by the plugin will be cleaned up after backup or restore. When setting ``cleanup_tmpfiles=no`` they will be left over, this can be helpful for debugging purposes. Since :sinceVersion:`22.0.0: Barri Plugin`

restore_esxhost (optional)
   By default, if a VM to be restored does not exist, it will be recreated on the same host that it has been running on at backup time. Use this option to restore on the given ESX host. Since :sinceVersion:`22.0.0: Barri Plugin`

restore_cluster (optional)
   Instead of specifying *restore_esxhost*, it is also possible to specify a cluster name using this option, the ESX host will be autoselected in that case, if DRS is configured properly. Since :sinceVersion:`22.0.0: Barri Plugin`

restore_datastore (optional)
   By default, if a VM to be restored does not exist, it will be recreated in the same datastore where it was stored at backup time. Use this option to restore on the given datastore. Since :sinceVersion:`22.0.0: Barri Plugin`. As :sinceVersion:`22.1.0: Barri Plugin` it is possible to backup and restore VMs with disks on multiple datastores, when using this option, it will only change the datastore of the disks which were stored in the same datastore as the VM, the other disks will be recreated on the same datastore they were backed up from.

restore_resourcepool (optional)
   By default, if a VM to be restored does not exist, it will be recreated in the same resource pool which it has in been in at backup time. Using this option allows to override this and specify a different resource pool. Since :sinceVersion:`22.0.0: Barri Plugin`

restore_powerstate (optional)
   By **default**, after restore a VM will be set to its **previous powerstate** which means the powerstate at backup time. When specifying ``restore_powerstate=off`` the VM will stay powered off after restore. Also can be forced to on with ``restore_powerstate=on``. Note that this will only work if DRS is configured to **fully automated**, otherwise the API request to power on a VM will be ignored. Since :sinceVersion:`22.0.0: Barri Plugin`

snapshot_retries (optional)
   Number of retries when taking a snapshot fails (default: 3). The most common cause of snapshot failure is "error while quiescing the virtual machine". In this case usually retrying helps. If not, also check if a pre-freeze script is used on the VM, as a non-zero exit code will cause a quiescing error. The pre-freeze and post-thaw scripts are executed by BarriTools. Since :sinceVersion:`22.1.0: Barri Plugin`

snapshot_retry_wait (optional)
  Time in seconds to wait before the next snapshot retry (default: 5). Since :sinceVersion:`22.1.0: Barri Plugin`

poweron_timeout (optional)
   Timeout in seconds to wait for a VM to be powered on after restore, default 15s. When a VM is powered on after restore (see also the option *restore_powerstate* above), the plugin will check if it succeeded by checking the power state. If it is not powered on within this timeout, the restore job will issue a warning message.

enable_cbt (optional)
   When using ``enable_cbt=yes``, the plugin will enable CBT (changed block
   tracking) if possible and it is not yet enabled.
   It is required that no snapshot exists when enabling CBT, otherwise the
   plugin will emit an error message.
   By default this option is set to **yes** since :sinceVersion:`23.0.0: Barri Plugin`
   so that ``vmware_cbt_tool.py`` is no longer necessary to enable CBT.
   This option exists since :sinceVersion:`22.1.1: Barri Plugin`

do_io_in_core (optional)
   With the option ``do_io_in_core=yes`` the data stream from the
   `bareos_vadp_dumper` will be processed directly by the Bareos core via file descriptor.
   When set to **no**, the data stream is read and written by the Python plugin
   code from the file descriptor and exchanged with the core over a buffer.
   So enabling this can improve the performance and will save CPU resource consumption.
   See :ref:`section-Python Plugin API` for more details.
   By default this is set to **yes**. Since :sinceVersion:`23.0.0: Barri Plugin`

vadp_dumper_multithreading (optional)
   The option ``vadp_dumper_multithreading=yes`` enables multithreading when
   running `bareos_vadp_dumper`, so that it will run one reader and one writer
   thread.
   By default it is set to **yes** as nowadays CPUs usually have multiple cores,
   so this improves the performance in most cases.
   Since :sinceVersion:`23.0.0: Barri Plugin`

vadp_dumper_sectors_per_call (optional)
   This option can be used to optimize the performance. The default value it is set
   to **16384**. This is the smallest value that achieved the maximum
   throughput in our benchmark tests.
   Together with ``vadp_dumper_multithreading=yes`` this setting can improve
   the backup performance significantly.
   Since :sinceVersion:`23.0.0: Barri Plugin`

vadp_dumper_query_allocated_blocks_chunk_size (optional)
   The `bareos_vadp_dumper` uses a VDDK function to query the allocated blocks
   of virtual disks since :sinceVersion:`23.0.0: Barri Plugin`.
   Especially for full backups, this normally leads to smaller and implicitly faster backups.
   By setting this plugin option, the chunk size which is passed to that
   function can be set.  The default value for the chunk size is **1024**.
   Allowed values are powers of two between 128 and 131072, inclusive.
   In our benchmark tests with small VMs of 3GB size, this value did not have
   any performance impact. However, with more data it might have an impact on
   the backup performance.

fallback_to_full_cbt (optional)
   In some situations requesting the CBT information for an incremental backup
   can fail, for example when the CBT information had to be reset. In that
   case, by default the plugin will fall back to request full level CBT
   information, which leads to a successful incremental backup job, but it
   will have the size of a full level backup job. As a consequence, restore
   time would increase, so a warning will be emitted in the job log, recommending
   a new full level backup.
   By setting ``fallback_to_full_cbt=no``, the job will not request full level
   CBT and terminate immediately with failure instead. This can be used if it
   is desired to run a subsequent new full level backup. Note that this does not
   happen automatically, a new full level job must be run manually afterwards,
   but this can be automated by using a post backup script, for details see
   https://github.com/bareos/bareos/tree/master/contrib/misc/reschedule_job_as_full

   Since :sinceVersion:`23.0.3: Barri Plugin`

restore_allow_disks_mismatch (optional)
   When using VSAN, restoring with recreating the VM can fail because the plugin
   detects a disk mismatch, as when using VSAN obviously recreated disks get a
   generated backing disk path. When passing the plugin option
   ``restore_allow_disks_mismatch=yes``, this disk match check will allow a
   mismatch and continue the restore.
   This option will only be used when recreating the VM to be restored.

   Since :sinceVersion:`23.0.4: Barri Plugin`

nvram_connect_timeout (optional)
   When backing up or restoring NVRAM, this is done by HTTPS connection to
   the vCenter server. The connect timeout in seconds can be changed using this
   parameter, the default is 30s.

   Since :sinceVersion:`25.0.5: Barri Plugin`

nvram_readwrite_timeout (optional)
   When backing up or restoring NVRAM, this is done by reading or writing it
   via HTTPS connection to/from the vCenter server. The timeout in seconds 
   can be changed using this parameter, the default is 60s.

   Since :sinceVersion:`25.0.5: Barri Plugin`


uuid (deprecated)
   The uuid option could be used instead of *dc*, *folder* and *vmname* to uniquely address a VM for backup. As the plugin since :sinceVersion:`22.0.0: Barri Plugin` is able recreate VMs in a different datacenter, folder or datastore, this option has become useless. When using uuid, restoring is only possible to the same still existing VM. It is recommended to change the configuration, as the uuid option will be dropped in the next version.
