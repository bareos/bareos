.. _BarriPlugin:

Barri Plugin
~~~~~~~~~~~~

.. index::
   single: Plugin; Barri
   single: Barri Plugin
   single: Windows Disaster Recovery

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

There are multiple options to recover a computer from a backup created with barri.

Two options use a linux live recovery system to recover the computer:

* A Linux Filedaemon with the barri filedaemon plugin can be run inside of the
  linux live system. The Linux Live system needs to be configured to have
  network access and to start the linux filedaemon that needs to be connected
  to the bareos system. The recovery then is performed by running a bareos
  restore command restoring the barri backup directly to the attached disks
  inside of the live system.

* The second option to recover a barri backup using a Linux Live Image is to
  recover the image via the `barri-cli` linux cli command. Therefore the
  existing .barri backup needs to be restored to a local file on the backup
  server first. This file then can be used to restore the computer from the
  linux commandline. The imagefile can be made available via removable media,
  network or any other means. Using the `barri-cli` makes a recovery possible
  in a complete disconnected environment.


In cases where linux does not have a driver that supports the hardware and thus the
system drives are not visible from the linux system, there is a third option to recover
the `.barri` file from the windows command line using `barri-cli.exe` for windows.

If you can connect your disk to another Windows computer (e.g. via portable
disk case), you can recover the disk there.

If you need to run the recovery on the original computer, this option requires
Windows PE, which is a live windows that can be created and preloaded with
special drivers if required.

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
Make sure to set

* :config:option:`dir/fileset/include/options/portable = yes`, and
* :config:option:`dir/fileset/EnableVss = No`

in the fileset.
The portable setting allows to recover the files also on |fd| running on other OS than Windows.
The VSS setting avoids an extra VSS snapshot that is not needed as barri does the VSS snapshot handling itself.

.. note::
   Please be sure to set

   * the fileset option :config:option:`dir/fileset/include/options/portable = yes`, and
   * the fileset directive :config:option:`dir/fileset/EnableVss = No`.

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
     Enable VSS = no  # avoid extra VSS snapshot

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
   Currently it is not possible to recover a barri backup via the windows barri-fd plugin.
   For recovery use the linux plugin or one of the cli tools.

   

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
    @BARRI/windr-fd.barri
    @BARRI/windr-fd.log

The plugin backed up two files:

``windr-fd.barri``
  This is the `.barri` file containing the disaster recovery data itself.
``windr-fd.log``
  This file contains a logfile that was recorded during the creation of the ``.barri`` file.


Restore with the Linux FD Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Start Live Linux. In this example we use **Fedora 42** Live Linux.
* Install bareos-filedaemon and bareos-filedaemon-barri-plugin

* Configure:


.. code-block:: bareosconfig
   :caption: :file:`/etc/bareos/bareos-fd.d/client/myself.conf` on linux live system

   Client {
     Name = localhost-live-fd
     Plugin Directory = "/usr/lib64/bareos/plugins"
     Plugin Names = "barri"
   }


Now we need to determine to which devices we want to restore the backed up disks.
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

In our case this results in:

.. code-block:: bareosconfig

   barri:<input>files=/dev/nvme0n1</input>


.. code-block:: bconsole
   :caption: Example disaster restore via plugin in linux live system

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
     [...] localhost-live-fd JobId 5: restore started (file = /tmp/bareos-restores/@BARRI/windr-fd.barri)
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
   
   

Restore the :file:`.barri` image to the filesystem
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Additionally to the option to restore the :file:`.barri` disaster recovery
image via the filedaemon plugin, it is also possible to recover the
:file:`.barri` image file to the filesystem. This is required to recover a
Windows Computer via the Linux or Windows version of the `barri-cli.exe` tool.

Recovering the :file:`.barri` image file to the filesystem is easy.

If the |fd| that is selected for the recovery has the barri-fd plugin loaded,
you can use the `copy=` plugin option to save the image to disk.

When recovered to a filedaemon that has not loaded the **barri** plugin,
the barri image will get restored as normal file in the filesystem.
Just mark the :file:`.barri` disaster recovery image for recovery
and send the restore to the filedaemon of the bareos server:


.. code-block:: bconsole
   :caption: Example restore to the filesystem

   *<input>restore jobid=13</input>
   You have selected the following JobId: 13
   
   Building directory tree for JobId(s) 13 ...  
   2 files inserted into the tree.
   
   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.
   
   cwd is: /
   $ <input>mark *</input>
   3 files newly marked.
   $ <input>done</input>
   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.7.bsr
   [...]
   2 files selected to be restored.
   
   Defined Clients:
   1: bareos-fd
   2: linuxlive-fd
   3: windowsdr-fd
   Select the Client (1-3): <input>1</input>
   Using Catalog "MyCatalog"
   Run Restore job
   JobName:         RestoreFiles
   [...]
   Plugin Options:  *None*
   OK to run? (yes/mod/no): <input>yes</input>
   [...]
   * <input>messages</input>
   [...] bareos-dir JobId 14: Version: 25.0.0~pre1074.e81033063 (01 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 14: Start Restore Job RestoreFiles.2025-10-08_09.31.59_10
   [...] bareos-dir JobId 14: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 14:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 14: Using Device "FileStorage" to read.
   [...] bareos-dir JobId 14: Probing client protocol... (result will be saved until config reload)
   [...] bareos-dir JobId 14: Connected Client: bareos-fd at localhost:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 14:    Handshake: Immediate TLS 
   [...] bareos-dir JobId 14:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] barri-server.bareos-fd JobId 14: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] barri-server.bareos-fd JobId 14:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 14: Version: 25.0.0~pre1074.e81033063 (01 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-sd JobId 14: Ready to read from volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
   [...] barri-server.bareos-fd JobId 14: Version: 25.0.0~pre1074.e81033063 (01 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-sd JobId 14: Forward spacing Volume "Full-0001" to file:block 0:223.
   [...] bareos-sd JobId 14: End of Volume at file 12 on device "FileStorage" (/var/lib/bareos/storage), Volume "Full-0001"
   [...] bareos-sd JobId 14: Ready to read from volume "Full-0002" on device "FileStorage" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 14: Forward spacing Volume "Full-0002" to file:block 0:223.
   [...] bareos-sd JobId 14: End of Volume at file 0 on device "FileStorage" (/var/lib/bareos/storage), Volume "Full-0002"
   [...] bareos-sd JobId 14: End of all volumes.
   [...] bareos-sd JobId 14: Releasing device "FileStorage" (/var/lib/bareos/storage).
   [...] bareos-dir JobId 14: Bareos bareos-dir 25.0.0~pre1074.e81033063 (01Oct25):
    Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
    JobId:                  14
    Job:                    RestoreFiles.2025-10-08_09.31.59_10
    Restore Client:         "bareos-fd" 25.0.0~pre1074.e81033063 (01Oct25) AlmaLinux Kitten release 10 (Lion Cub),redhat
    Start time:             08-Okt-2025 09:32:01
    End time:               08-Okt-2025 09:47:20
    Elapsed time:           15 mins 19 secs
    Files Expected:         2
    Files Restored:         2
    Bytes Restored:         133,917,942,676
    Rate:                   145721,4 KB/s
    FD Errors:              0
    FD termination status:  OK
    SD termination status:  OK
    Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
    Job triggered by:       User
    Termination:            Restore OK


The file will be restored into the file :file:`<where>/@BARRI/winddr-fd.barri`.




Restore with the Linux barri-cli commandline tool
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The :command:`barri-cli` linux commandline tool is part of the **bareos-filedaemon-barri-plugin** package.
To be able to use it in a linux live system, you can either install it into the live system, or install it onto another
computer and copy the tool into the live system via portable drive.

After installation, the :command:`barri-cli` is available as :file:`/usr/bin/barri-cli`.

For the recovery we prepare a USB drive with both the :file:`barri-cli` binary and the :file:`windr-fd.barri` disaster recovery
image.

The recovery process is as follows:

#. Boot the computer to be recovered into a live linux system. In this test we used the Fedora Live Installation System.
#. Mount the prepared USB drive with :command:`barri-cli` and the :file:`.barri` image file into the live system.
#. Open a shell with root rights in a terminal and change to the USB Drive mount point. (e.g. :file:`/run/media/liveuser/Barri`)
#. Use :command:`lsblk` to determine the name of the drive(s) that the recovery should overwrite. In our case it is :file:`nvme0n1`
#. Call barri-cli to restore the :file:`windr-fd.barri` onto the drive.

.. code-block:: sh
   :caption: recover from a live linux with :command:`barri-cli`

   root@livesystem: <input>./barri-cli restore --files /dev/nvme0n1 < @BARRI/windr-fd.barri</input>
   barri - bareos recovery imager
   Version 25.0.0~pre1093.c2d6db38b (06 October 2025)
   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
   Get professional support from https://www.bareos.com
   Restoring 1 Disks
   Restoring disk 0
   disk 0 is neither a block device, nor a regular file.  Continuing without size check
   [====>                                             ] 9% [00m:43s<07m:22s] 261.80 MiB/s                                                       
   





As barri-cli reads the image file from **standard input** (**stdin**) it is very flexible.
If the live linux has network connectivity, the barri file can be piped
directly through ssh. Also it is possible to provide the image via http,
network drive or any other means that make sense in your environment.

Examples:

.. code-block:: sh
   :caption: Provide the image via ssh to :command:`barri-cli`

   <input>ssh root@livesytem barri-cli restore --files /dev/nvme0n1 < /path/to/barri/image.barri</input>

.. code-block:: sh
   :caption: Provide the image via curl download to :command:`barri-cli`

   <input>curl --silent http://url/of/the/barri_file.barri | barri-cli restore --files /dev/nvme0n1</input>



Restore with the Windows barri-cli.exe commandline tool
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the linux live system approach is unsuitable, e.g. because the disks are not recognized because of missing
drivers, it is possible to use the Windows Version of :command:`barri-cli` for recovery.

One approch is to connect the disk that needs to be recovered to a second
Windows Machine and recover if from there.

The second approach is to prepare a Windows PE live system and add the required
drivers so that the Windows live system recognizes the disks on the original hardware.

In any case, make sure that the disk is not in use, as the recovery will fail if the disk is in use by windows.



Preparing :command:`barri-cli.exe` to be run on an USB Stick
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

barri-cli.exe depends on the c++ runtime dlls and on zlib1.dll.
We suggest to create a folder barri on your USB Stick (F:) and copy the required files into that folder with the following commands:

.. code-block:: bat
   :caption: copy_barri_cli_to_drive.bat
   
   rem Set your driveletter in the next line
   set DRIVE=F:\
   
   set BARRIDIR=%DRIVE%\barri
   md %BARRIDIR%
   copy "C:\Program Files\Bareos\barri-cli.exe" %BARRIDIR%
   copy "C:\Program Files\Bareos\zlib1.dll" %BARRIDIR%
   copy "C:\Windows\System32\msvcp140*.dll" %BARRIDIR%
   copy "C:\Windows\System32\vcruntime140*.dll" %BARRIDIR%

The resulting directory should look similar to the following:

.. code-block:: bat

   barri
   ├── barri-cli.exe
   ├── msvcp140_1.dll
   ├── msvcp140_2.dll
   ├── msvcp140_atomic_wait.dll
   ├── msvcp140_clr0400.dll
   ├── msvcp140_codecvt_ids.dll
   ├── msvcp140.dll
   ├── vcruntime140_1.dll
   ├── vcruntime140_clr0400.dll
   ├── vcruntime140.dll
   ├── vcruntime140_threads.dll
   └── zlib1.dl




Creating a Windows PE image 
'''''''''''''''''''''''''''

Please check the
`Microsoft Documentation <https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/winpe-create-usb-bootable-drive?view=windows-11>`_
for the details. We will only give a short overview of the steps to create a Windows PE Boot Image.

* Install the Windows **Assessment and Deployment Kit (ADK) deployment tools** and the **ADK Windows PE Add-ons**
* Start the "Start the Deployment and Imaging Tools Environment as an administrator" which  lies in :file:`C:\\Program Files (x86)\\Windows Kits\\10\\Assessment and Deployment Kit\\Deployment Tools\\DandISetEnv.bat`
  
Change into the Windows PE Directory for amd64:

.. code-block:: bat

   cd "..\Windows Preinstallation Environment\amd64"

Prepare the Windows PE Working Set:

.. code-block:: bat

   copype amd64 C:\WinPE_amd64
   md C:\WinPE_amd64\mount
   Dism /Mount-Image /ImageFile:"en-us\winpe.wim" /index:1 /MountDir:"C:\WinPE_amd64\mount"

Add latest updates to the Image:

.. code-block:: bat

   Dism /Add-Package /Image:"C:\WinPE_amd64\mount" /PackagePath:"C:\Users\User\Downloads\windows10.0-kb5065744-x86_b709e76641ebeb7eb36e79dfb9f469b9611a82b8.msu"
   Dism /Add-Package /Image:"C:\WinPE_amd64\mount" /PackagePath:"C:\Users\User\Downloads\windows10.0-kb5065755-x86-ndp48_69a8659b2ce13e5d2db14494a95683ef39dd0b42.msu"


Lock in the updates:

.. code-block:: bat

   md C:\temp
   Dism /Cleanup-Image /Image:C:\WinPE_amd64\mount /Startcomponentcleanup /Resetbase /ScratchDir:C:\temp

Unmount the Image

.. code-block:: bat

   Dism /Unmount-Image /MountDir:"C:\WinPE_amd64\mount" /commit

Write the Windows PE Bootmedium to Drive D:

.. code-block:: bat

   MakeWinPEMedia /UFD C:\WinPE_amd64 D: /bootex

.. note::

   Installing the Updates was required to successfully boot with Secure Boot enabled in our test environment.

.. note::

   The Windows PE Boot Medium cannot exceed 32GB in size. For Workarounds please see the Microsoft Documentation


Recovery in Windows PE using barri-cli.exe
''''''''''''''''''''''''''''''''''''''''''

Now use the just created USB Drive to boot the Computer that you want to
restore. After successful bootup the computer show a cmd.exe commandline
window like below:


.. image:: /include/images/WindowsPE.*
   :width: 80.0%


It is now safe to remove the Windows PE USB Disk, and we suggest to do so to
reduce the number of disks that are connected to the computer and to also free
one additional hardware port.

Now connect the USB Drive to the Windows PE Computer. You can use the
commandline tool :command:`diskpart` to determine both the detected disks and
volumes:

.. code-block:: sh

   X:\Windows\System32><input>diskpart</input>
   
   Microsoft DiskPart version 10.0.26100.1
   
   Copyright (C) Microsoft Corporation.
   On computer: MININT-DE9S2KM
   DISKPART> 

.. code-block:: sh
   :caption: use diskpart **list disk** to determine the disks to recover to

   DISKPART> <input>list disk</input>
   
     Disk ###  Status         Size     Free     Dyn  Gpt
     --------  -------------  -------  -------  ---  ---
     Disk 0    Online          256 GB   234 GB        *
     Disk 1    Online          460 GB  1024 KB

Take note what disk number(s) the disks have that you want to recover to.
In this example, we want to recover disk **0**.

.. code-block:: sh
   :caption: use diskpart **list volume** to determine the drive letters of our USB drive

   DISKPART> <input>list volume</input>
   
     Volume ###  Ltr  Label        Fs     Type        Size     Status     Info
     ----------  ---  -----------  -----  ----------  -------  ---------  --------
     Volume 0     D   CESE_X64FRE  UDF    CD-ROM      4875 MB  Healthy
     Volume 1                      NTFS   Partition    256 GB  Healthy
     Volume 2                      FAT32  Partition    100 MB  Healthy    Hidden
     Volume 3     C   Barri        NTFS   Partition    460 GB  Healthy
   DISKPART><input>exit</input>

Now change the directory to the USB drive containing :command:`barri-cli.exe`
and the :file:`.barri` image file.
In our case the drive is :file:`C:`.
To recover a :file:`.barri` image, we call :command:`barri-cli.exe` in the following way:

.. code-block:: sh

   C:\barri\barri-cli.exe --to-disks <disks to write to> < <path_to_barri_file>

Applied to our example this leads to:

.. code-block:: sh

   C:\barri\barri-cli.exe --to-disks 0 < @BARRI\windr-fd.barri

   barri - bareos recovery imager
   Version 25.0.0~pre1152.a0abe56e3 (09 October 2025)
   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
   Get professional support from https://www.bareos.com
   found 3 volumes on the selected disks
    begin disk { size: 256060514304, extent count: 20582 }
   [>                                                 ] 0% [00:00s<00:00s]
   [=>                                                ] 2% [00m:32s<09m:01s] 120.50 MiB/s                                 
   [...]
   [=================================================>] 99% [13m:54s<00m:01s] 152.25 MiB/s                                 
   disk done
   [=================================================>] 99% [13m:55s<00m:01s] 152.25 MiB/s                                 
   [==================================================] 100% [13m:56s<00m:00s] Done                                        
   
   

.. note:: 

   If the target disk is not empty it can be required to remove all volumes from the disk using diskpart before barri can recover the image.

When all disks have been recovered, you can unplug all removable drives and close the Windows PE commandline window.
The computer will restart and the recovered OS will start.


Description of all Plugin Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The windows plugin has the following options:

unknown disks
  If this option is given, the barri plugin will also save **disks** that contain no snapshotted data.
  This could be a linux disk in a multi-boot environment.

unknown partitions
  If this option is given, the barri plugin will also save **partitions** that contain no snapshotted data.
  This could be a linux partition in a multi-boot environment.

unknown extents
  If this option is given, the barri plugin will save **extents** that contain no snapshotted data.
  This usually does not make sense, as it will enlarge the backup size with unneeded data.

ignore disks=<disks to ignore>
  If this option is given, the barri plugin will skip the disks with the given numbers.
  To avoid disk 1 being snapshotted and backed up, add `ignore disks=1` to the plugin options


The linux plugin has the following options:

files=<list of file paths> [*]
  If this option is set to a comma-separated list of files, then bareos will restore
  the disks to the given files.  The first disk from the image will get restored
  to the first given file, the second disk will get restored to the second file,
  and so on.  If the number of files and disks do not match up, then the restore
  will fail.

directory=<path to directory> [*]
  If this option is set to a directory path, then bareos will restore all disks as
  newly created files in that directory.

copy=<path to a file> [*]
  If this option is set to a file, then bareos will write a copy the image to that
  file.  This copy can then be restored manually using **barri-cli**.

.. note::
   Only a single option marked [*] may be given during a single restore.
