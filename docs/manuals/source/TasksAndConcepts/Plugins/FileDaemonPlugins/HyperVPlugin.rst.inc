.. _HyperVPlugin:

|hyperv| Plugin
~~~~~~~~~~~~~~~

.. index::
   pair: Plugin; HyperV
   RCT; Resilient Change Tracking

The |hyperv| Plugin allows you to backup hyper-v virtual machines.  It makes use of **RCT** *(Resilient Change Tracking)*
Technology to make extremely fast incremental backups.


.. limitation:: Local Paths

   Currently only disks with a local path can be backed up.

.. limitation:: Currently .vhds disks are unsupported

   Currently :file:`.vhds` disks are unsupported; only :file:`.vhdx` and :file:`.vhd` disks are supported.


Requirements
^^^^^^^^^^^^

The |hyperv| plugin needs to be installed on a Windows Computer with the `Hyper-V` Windows feature installed and running.


Installation
^^^^^^^^^^^^

To install the |hyperv| plugin use run the Bareos Windows installer and install the **File Daemon** and the **Hyper-V File Daemon Plugin**.

Configuration
^^^^^^^^^^^^^

The |hyperv| Plugin expects the Name of the Virtual Machine to be backed up as plugin parameter `vmname`.

Make sure to add or enable the following settings in your |fd| configuration and restart the |fd| service.

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = "C:/Program Files/bareos/plugins"
     Plugin Names = "hyper-v"
     ...
   }


To verify that the plugin was loaded, execute :command:`status client` and look for the Hyper-V plugin information:

.. code-block:: bconsole
   :caption: |hyperv| plugin is loaded
    
   *<input>status client=hyperv-fd</input>
   Connecting to Client hyperv-fd at hyperv:9102
   [...]
   hypev-fd Version: 25.0.0~pre925.de8540c15 (20 October 2025)  VSS Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit
   Daemon started 20-Okt-25 14:56. Jobs: run=0 running=0, self-compiled binary
    Sizeof: boffset_t=8 size_t=8 debug=0 trace=0 bwlimit=0kB/s
   Plugin Info:
    Plugin     : hyper-v-fd.dll
    Description: Bareos Hyper-V Windows File Daemon Plugin
    Version    : 1 (April 2025)
    Author     : Sebastian Sura
    License    : Bareos AGPLv3
    Usage      : 
     hyper-v:config_file=<path>:vmname=<name>
    [...]
   
   
The next step is to configure a fileset for each VM to be backed up. In our case the VM Name is **Alma10**:


.. limitation:: Unique VM Name is required

   The name of the Virtual Machine needs to be unique. If multilple VMs with the same name exist, the backup will be cancelled.
   


.. code-block:: bareosconfig
   :caption:  bareos-dir.d/job/hyperv-alma10.conf and bareos-dir.d/fileset/hyperv-alma10.conf

   Job {
     Name = "hyperv-alma10"
     Client = "hyperv-fd"
     JobDefs = "DefaultJob"
     FileSet = "hyper-v-alma10"
   }

   FileSet {
     Name = "hyper-v-alma10"
     Description = "backup of the hyper-v vm Alma10 via the hyper-v plugin"
     Include {
       Options {
         ...
         Compression = LZ4 # recommended if using dynamic disks
       }
       Plugin = "hyper-v:vmname=Alma10"
     }
   }

.. note::
   Currently the plugin does not support sparse full backups.  That means that the full dynamic disk
   is backed up even if only a small part of it is actually used.
   For this reason we recommend using compression in hyper-v filesets, as this will allow the backed up
   data to be shrunk again to their sparse size.

All parameters can optionally also be given inside a json configuration file.
You can specify the configuration file to use via the **config_file** option.

.. code-block:: bareosconfig
   :caption:  bareos-dir.d/fileset/hyperv-alma10.conf

   FileSet {
     Name = "hyper-v-alma10"
     Description = "backup of the hyper-v vm Alma10 via the hyper-v plugin"
     Include {
       ...
       Plugin = "hyper-v:config_file=C:/hyper-v.conf"
     }
   }

.. code-block:: json
   :caption: C:\\hyper-v.conf

   {
     "vmname" : "My Virtual Machine"
   }

You can freely mix a config_file with other parameters specified directly in the fileset.
Parameters from the fileset always take precedence.



Backup
^^^^^^

To create a backup of your virtual machine, just run the job `hyper-v-alma10` from the :command:`bconsole` with the Backup Level Full.

The Bareos Plugin will create a checkpoint and backup the virtual machine:

.. code-block:: bconsole
   :caption: create a full vm backup with the hyper-v plugin
      
   <input>run job=hyper-v-alma10 level=Full yes</input>
   [...] bareos-dir JobId 118: Version: 25.0.0~pre1093.c2d6db38b (06 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 118: Start Backup JobId 118, Job=hyper-v-alma10.2025-10-23_15.46.41_08
   [...] bareos-dir JobId 118: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 118:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 118: Connected Client: hyperv-fd at hyperv:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 118:  Handshake: Immediate TLS 
   [...] bareos-dir JobId 118:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] hyperv-fd JobId 118: Successfully connected to 'ROOT\VIRTUALIZATION\V2'
   [...] bareos-sd JobId 118: Using just in time reservation for job 118
   [...] bareos-dir JobId 118: Using Device "JustInTime Device" to write.
   [...] bareos-sd JobId 118: Connected File Daemon at hyperv:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 118: Version: 25.0.0~pre1093.c2d6db38b (06 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] hyperv-fd JobId 118: Fileset has no "File=" directives, ignoring FilesNotToBackup Registry key
   [...] hyperv-fd JobId 118: Version: 25.0.0~pre965.f8bad1c5a (23 October 2025) Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit
   [...] hyperv-fd JobId 118: preparing backup of VM 'Alma10'
   [...] hyperv-fd JobId 118: creating snapshot of VM 'Alma10' ...
   [...] hyperv-fd JobId 118: created snapshot with name 'Bareos JobId 118'
   [...] hyperv-fd JobId 118: exporting system definition for VM 'Alma10' to C:\WINDOWS\TEMP\Bareos-118
   [...] hyperv-fd JobId 118: retrieving information from snapshot ...
   [...] hyperv-fd JobId 118: finished preparing backup
   [...] hyperv-fd JobId 118: Starting backup of metadata C:\WINDOWS\TEMP\Bareos-118\Virtual Machines\7802CE25-83F1-48E0-9637-941262284282.VMRS
   [...] hyperv-fd JobId 118: Starting backup of metadata C:\WINDOWS\TEMP\Bareos-118\Virtual Machines\7802CE25-83F1-48E0-9637-941262284282.vmgs
   [...] bareos-sd JobId 118: JustInTime Reservation: Finding drive to reserve.
   [...] bareos-dir JobId 118: Created new Volume "Full-0008" in catalog.
   [...] hyperv-fd JobId 118: Starting backup of metadata C:\WINDOWS\TEMP\Bareos-118\Virtual Machines\7802CE25-83F1-48E0-9637-941262284282.vmcx
   [...] bareos-sd JobId 118: Using Device "File0001" (/var/lib/bareos/storage) to write.
   [...] hyperv-fd JobId 118: Starting backup of disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{76F49D96-6EB4-441E-A406-B9C47E0523F5}_D08F5D3B-71D8-4C1D-8909-EECDD1675E76.avhdx'
   [...] hyperv-fd JobId 118: Backing up disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{76F49D96-6EB4-441E-A406-B9C47E0523F5}_D08F5D3B-71D8-4C1D-8909-EECDD1675E76.avhdx' as '{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx'
   [...] bareos-sd JobId 118: Labeled new Volume "Full-0008" on device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 118: Moving to end of data on volume "Full-0008"
   [...] bareos-sd JobId 118: Ready to append to end of Volume "Full-0008" size=223
   [...] bareos-dir JobId 118: Max Volume jobs=1 exceeded. Marking Volume "Full-0008" as Used.
   [...] hyperv-fd JobId 118: Starting backup of disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{6882FDF0-4924-4CFD-96CE-096235890E15}_0706D94C-D2FA-4F17-89CC-869ECC021208.avhdx'
   [...] hyperv-fd JobId 118: Backing up disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{6882FDF0-4924-4CFD-96CE-096235890E15}_0706D94C-D2FA-4F17-89CC-869ECC021208.avhdx' as '{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx'
   [...] bareos-sd JobId 118: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 118: Elapsed time=00:01:31, Transfer rate=44.68 M Bytes/second
   [...] bareos-dir JobId 118: Insert of attributes batch table with 5 entries start
   [...] bareos-dir JobId 118: Insert of attributes batch table done
   [...] bareos-dir JobId 118: Bareos bareos-dir 25.0.0~pre1093.c2d6db38b (06Oct25):
    Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
    JobId:                  118
    Job:                    hyper-v-alma10.[...]_08
    Backup Level:           Full
    Client:                 "hyperv-fd" 25.0.0~pre965.f8bad1c5a (23Oct25) Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit,Windows-x64
    FileSet:                "hyperv-alma10" [...]
    Pool:                   "Full" (From Job FullPool override)
    Catalog:                "MyCatalog" (From Client resource)
    Storage:                "File" (From Job resource)
    Scheduled time:         23-Okt-2025 15:46:41
    Start time:             23-Okt-2025 15:46:43
    End time:               23-Okt-2025 15:48:16
    Elapsed time:           1 min 33 secs
    Priority:               10
    Allow Mixed Priority:   no
    FD Files Written:       9
    SD Files Written:       9
    FD Bytes Written:       4,066,217,365 (4.066 GB)
    SD Bytes Written:       4,066,219,893 (4.066 GB)
    Rate:                   43722,8 KB/s
    Software Compression:   65,6 % (lz4)
    VSS:                    no
    Encryption:             no
    Accurate:               no
    Volume name(s):         Full-0008
    Volume Session Id:      3
    Volume Session Time:    1761227133
    Last Volume Bytes:      4,066,901,446 (4.066 GB)
    Non-fatal FD errors:    0
    SD Errors:              0
    FD termination status:  OK
    SD termination status:  OK
    Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
    Job triggered by:       User
    Termination:            Backup OK


Now we let the VM run a bit which will alter the VM Disk contents. The data being saved in the following incremental backup
depends on the amount of disk data that has been changed since the last backup.
The next step is to run an incremental backup of the VM:

.. code-block:: bconsole
   :caption: create an incremental vm backup with the hyper-v plugin

   *<input>run job=hyper-v-alma10 level=Incremental yes</input>
   [...] bareos-dir JobId 123: Version: 25.0.0~pre1093.c2d6db38b (06 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 123: Start Backup JobId 123, Job=hyper-v-alma10.2025-10-23_15.58.18_18
   [...] bareos-dir JobId 123: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 123:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 123: Connected Client: hyperv-fd at hyperv:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 123:  Handshake: Immediate TLS 
   [...] bareos-dir JobId 123:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] hyperv-fd JobId 123: Successfully connected to 'ROOT\VIRTUALIZATION\V2'
   [...] bareos-sd JobId 123: Using just in time reservation for job 123
   [...] bareos-dir JobId 123: Using Device "JustInTime Device" to write.
   [...] bareos-sd JobId 123: Connected File Daemon at hyperv:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 123: Version: 25.0.0~pre1093.c2d6db38b (06 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] hyperv-fd JobId 123: Fileset has no "File=" directives, ignoring FilesNotToBackup Registry key
   [...] hyperv-fd JobId 123: Version: 25.0.0~pre965.f8bad1c5a (23 October 2025) Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit
   [...] hyperv-fd JobId 123: preparing backup of VM 'Alma10'
   [...] hyperv-fd JobId 123: creating snapshot of VM 'Alma10' ...
   [...] hyperv-fd JobId 123: created snapshot with name 'Bareos JobId 123'
   [...] hyperv-fd JobId 123: exporting system definition for VM 'Alma10' to C:\WINDOWS\TEMP\Bareos-123
   [...] hyperv-fd JobId 123: retrieving information from snapshot ...
   [...] hyperv-fd JobId 123: finished preparing backup
   [...] hyperv-fd JobId 123: Starting backup of metadata C:\WINDOWS\TEMP\Bareos-123\Virtual Machines\7802CE25-83F1-48E0-9637-941262284282.VMRS
   [...] bareos-sd JobId 123: JustInTime Reservation: Finding drive to reserve.
   [...] bareos-dir JobId 123: Created new Volume "Incremental-0010" in catalog.
   [...] hyperv-fd JobId 123: Starting backup of metadata C:\WINDOWS\TEMP\Bareos-123\Virtual Machines\7802CE25-83F1-48E0-9637-941262284282.vmgs
   [...] hyperv-fd JobId 123: Starting backup of metadata C:\WINDOWS\TEMP\Bareos-123\Virtual Machines\7802CE25-83F1-48E0-9637-941262284282.vmcx
   [...] bareos-sd JobId 123: Using Device "File0001" (/var/lib/bareos/storage) to write.
   [...] hyperv-fd JobId 123: Starting backup of disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{76F49D96-6EB4-441E-A406-B9C47E0523F5}.vhdx'
   [...] hyperv-fd JobId 123: Backing up disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{76F49D96-6EB4-441E-A406-B9C47E0523F5}.vhdx' as '{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx'
   [...] hyperv-fd JobId 123: Starting backup of disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{6882FDF0-4924-4CFD-96CE-096235890E15}.vhdx'
   [...] hyperv-fd JobId 123: Backing up disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{6882FDF0-4924-4CFD-96CE-096235890E15}.vhdx' as '{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx'
   [...] bareos-sd JobId 123: Labeled new Volume "Incremental-0010" on device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 123: Moving to end of data on volume "Incremental-0010"
   [...] bareos-sd JobId 123: Ready to append to end of Volume "Incremental-0010" size=237
   [...] bareos-dir JobId 123: Max Volume jobs=1 exceeded. Marking Volume "Incremental-0010" as Used.
   [...] bareos-sd JobId 123: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 123: Elapsed time=00:00:06, Transfer rate=438.9 K Bytes/second
   [...] bareos-dir JobId 123: Insert of attributes batch table with 5 entries start
   [...] bareos-dir JobId 123: Insert of attributes batch table done
   [...] bareos-dir JobId 123: Bareos bareos-dir 25.0.0~pre1093.c2d6db38b (06Oct25):
    Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
    JobId:                  123
    Job:                    hyper-v-alma10.2025-10-23_15.58.18_18
    Backup Level:           Incremental, since=2025-10-23 15:46:43
    Client:                 "hyperv-fd" 25.0.0~pre965.f8bad1c5a (23Oct25) Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit,Windows-x64
    FileSet:                "hyperv-alma10" 2025-10-21 15:03:45
    Pool:                   "Incremental" (From Job IncPool override)
    Catalog:                "MyCatalog" (From Client resource)
    Storage:                "File" (From Job resource)
    Scheduled time:         23-Okt-2025 15:58:18
    Start time:             23-Okt-2025 15:58:20
    End time:               23-Okt-2025 15:58:28
    Elapsed time:           8 secs
    Priority:               10
    Allow Mixed Priority:   no
    FD Files Written:       9
    SD Files Written:       9
    FD Bytes Written:       2,631,072 (2.631 MB)
    SD Bytes Written:       2,633,524 (2.633 MB)
    Rate:                   328,9 KB/s
    Software Compression:   88,2 % (lz4)
    VSS:                    no
    Encryption:             no
    Accurate:               no
    Volume name(s):         Incremental-0010
    Volume Session Id:      7
    Volume Session Time:    1761227133
    Last Volume Bytes:      2,646,849 (2.646 MB)
    Non-fatal FD errors:    0
    SD Errors:              0
    FD termination status:  OK
    SD termination status:  OK
    Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
    Job triggered by:       User
    Termination:            Backup OK

   

We now have a full Backup of the VM and also an incremental Backup that only contains the changed blocks of the virtual disks.



Restore
^^^^^^^

The next step is to restore the Virtual Machine. Therefore run the restore command and mark all files that are available for restore:

.. code-block:: bconsole
   :caption: restore a backed up vm with the hyper-v plugin

   *<input>restore client=hyperv-fd fileset=hyperv-alma10</input>
   
   First you select one or more JobIds that contain files
   [...]
   5: Select the most recent backup for a client
   [...]  
   Select item:  (1-13): <input>5</input>
   +-------+-------+----------+---------------+---------------------+------------------+
   | jobid | level | jobfiles | jobbytes      | starttime           | volumename       |
   +-------+-------+----------+---------------+---------------------+------------------+
   |   105 | F     |        9 | 4,066,290,590 | 2025-10-22 14:54:38 | Full-0003        |
   |   106 | I     |        9 |       741,463 | 2025-10-22 15:17:46 | Incremental-0005 |
   +-------+-------+----------+---------------+---------------------+------------------+
   You have selected the following JobIds: 105,106
   
   Building directory tree for JobId(s) 105,106 ...  
   7 files inserted into the tree.
   
   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.
   
   cwd is: /
   $ <input>mark *</input>
   6 files newly marked.
   $ done
   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.1.bsr
   [...] 
   Using Catalog "MyCatalog"
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.1.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         LinuxAll
   Backup Client:   hyperv-fd
   Restore Client:  hyperv-fd
   Format:          Native
   Storage:         File
   When:            2025-10-23 09:15:03
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no): <input>yes</input>
   [...] bareos-dir JobId 124: Version: 25.0.0~pre1093.c2d6db38b (06 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 124: Start Restore Job RestoreFiles.2025-10-23_16.06.06_21
   [...] bareos-dir JobId 124: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 124:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 124: Using Device "File0001" to read.
   [...] bareos-dir JobId 124: Connected Client: hyperv-fd at hyperv:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 124:  Handshake: Immediate TLS 
   [...] bareos-dir JobId 124:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] hyperv-fd JobId 124: Successfully connected to 'ROOT\VIRTUALIZATION\V2'
   [...] bareos-sd JobId 124: Connected File Daemon at hyperv:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 124: Version: 25.0.0~pre1093.c2d6db38b (06 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-sd JobId 124: Ready to read from volume "Full-0008" on device "File0001" (/var/lib/bareos/storage).
   [...] hyperv-fd JobId 124: using 'C:\WINDOWS\TEMP\Bareos-124' as temporary hyper-v import directory
   [...] hyperv-fd JobId 124: Version: 25.0.0~pre965.f8bad1c5a (23 October 2025) Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit
   [...] bareos-sd JobId 124: Forward spacing Volume "Full-0008" to file:block 0:223.
   [...] hyperv-fd JobId 124: restoring disk  {AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx ('@hyper-v@/Alma10/disks/{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx') to 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx'
   [...] hyperv-fd JobId 124: restoring disk  {E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx ('@hyper-v@/Alma10/disks/{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx') to 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx'
   [...] bareos-sd JobId 124: End of Volume at file 0 on device "File0001" (/var/lib/bareos/storage), Volume "Full-0008"
   [...] bareos-sd JobId 124: Ready to read from volume "Incremental-0010" on device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 124: Forward spacing Volume "Incremental-0010" to file:block 0:237.
   [...] bareos-sd JobId 124: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] hyperv-fd JobId 124: restoring metadata file '@hyper-v@/Alma10/config/7802CE25-83F1-48E0-9637-941262284282.VMRS' as 'C:\WINDOWS\TEMP\Bareos-124\Alma10\config\7802CE25-83F1-48E0-9637-941262284282.VMRS'
   [...] hyperv-fd JobId 124: restoring metadata file '@hyper-v@/Alma10/config/7802CE25-83F1-48E0-9637-941262284282.vmgs' as 'C:\WINDOWS\TEMP\Bareos-124\Alma10\config\7802CE25-83F1-48E0-9637-941262284282.vmgs'
   [...] hyperv-fd JobId 124: restoring metadata file '@hyper-v@/Alma10/config/7802CE25-83F1-48E0-9637-941262284282.vmcx' as 'C:\WINDOWS\TEMP\Bareos-124\Alma10\config\7802CE25-83F1-48E0-9637-941262284282.vmcx'
   [...] hyperv-fd JobId 124: restoring incremental image of '{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx' (level 1) to 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx'
   [...] hyperv-fd JobId 124: restoring incremental image of '{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx' (level 1) to 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx'
   [...] hyperv-fd JobId 124: creating a new vm from the restored configuration file 'C:\WINDOWS\TEMP\Bareos-124\Alma10\config\7802CE25-83F1-48E0-9637-941262284282.vmcx'
   [...] hyperv-fd JobId 124: updating the configuration of the created vm
   [...] hyperv-fd JobId 124:  - changing disk path 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{6882FDF0-4924-4CFD-96CE-096235890E15}.vhdx' to restored disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx'
   [...] hyperv-fd JobId 124:  - changing disk path 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\{76F49D96-6EB4-441E-A406-B9C47E0523F5}.vhdx' to restored disk 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx'
   [...] hyperv-fd JobId 124: validating created vm
   [...] hyperv-fd JobId 124: realizing created vm
   [...] bareos-dir JobId 124: Bareos bareos-dir 25.0.0~pre1093.c2d6db38b (06Oct25):
    Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
    JobId:                  124
    Job:                    RestoreFiles.2025-10-23_16.06.06_21
    Restore Client:         "hyperv-fd" 25.0.0~pre965.f8bad1c5a (23Oct25) Microsoft Windows Server 2012 Standard Edition (build 9200), 64-bit,Windows-x64
    Start time:             23-Okt-2025 16:06:08
    End time:               23-Okt-2025 16:07:56
    Elapsed time:           1 min 48 secs
    Files Expected:         7
    Files Restored:         7
    Bytes Restored:         11,833,549,454
    Rate:                   109569,9 KB/s
    FD Errors:              0
    FD termination status:  OK
    SD termination status:  OK
    Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
    Job triggered by:       User
    Termination:            Restore OK




After the successful restore, the Virtual Machine shows up in the Hyper-V Manager and can be started.

During recovery, the |hyperv| plugin tries to recover the virtual harddisks to the original place.

In case a harddisk with the same name already exists (usually because the original VM still exists), the plugin will
alter the harddisks' filename with a prefix and store it into the original folder.

The restore job's termination will be **Restore OK -- with warnings**

.. code-block:: bconsole
   :caption: Store the Virtual Disks to different name if original disks exist

   [...] hyperv-fd JobId 125: Warning: 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx' already exists,⤶
                                 using 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\bareos-125-0-{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx' for restore instead
   [...] hyperv-fd JobId 125: restoring disk  {AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx ('@hyper-v@/Alma10/disks/{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx') to⤶
                                                'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\bareos-125-0-{AEE72202-4637-4C3C-8A3A-03806C36F641}.vhdx'
   [...] hyperv-fd JobId 125: Warning: 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx' already exists,⤶
                                using 'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\bareos-125-0-{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx' for restore instead
   [...] hyperv-fd JobId 125: restoring disk  {E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx ('@hyper-v@/Alma10/disks/{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx') to⤶
                                                'C:\ProgramData\Microsoft\Windows\Virtual Hard Disks\\bareos-125-0-{E4289419-1C8F-4D6E-A195-1BC59C6443EA}.vhdx'
   [...] bareos-dir JobId 125: Bareos bareos-dir 25.0.0~pre1093.c2d6db38b (06Oct25):
   [...]
    FD termination status:  OK -- with warnings
    SD termination status:  OK
    Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
    Job triggered by:       User
    Termination:            Restore OK -- with warnings




Description of all Plugin Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


config_file (optional)
   Path to a json file containing values to use for the other options.

vmname (mandatory)
   Name of the VM
