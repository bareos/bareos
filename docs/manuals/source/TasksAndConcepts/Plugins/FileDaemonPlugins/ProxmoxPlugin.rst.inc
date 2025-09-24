.. _ProxmoxPlugin:

Proxmox Plugin
~~~~~~~~~~~~~~

.. index::
   single: Plugin; Proxmox
   single: Proxmox Plugin

The |proxmox| Plugin can be used for agentless backups of virtual machines running on |proxmox ve|.
It is capable of doing full backups of both VM Guests and Container Guests.

It is available since Bareos :sinceVersion:`25.0.0: Proxmox Plugin`.

Status
^^^^^^

The Plugin can do full backup and can has two options for restoring:

* It can restore the backup into a new proxmox guest (Virtual Machine / Container).
* It can restore the backup into a local vzdump dump file to be recovered later via the proxmox gui.


.. limitation:: Proxmox Plugin: Only Full Backups are currently supported.

   The Proxmox Plugin currently only supports Full backups.


Requirements
^^^^^^^^^^^^

The Proxmox Plugin needs to be installed on a member of the proxmox cluster
that is running the guests that need to be backed up.

Installation
^^^^^^^^^^^^

Install the package **bareos-filedaemon-proxmox-plugin** (using :command:`apt`)
on one of the |proxmox ve| cluster members.

Configuration
^^^^^^^^^^^^^

Make sure to add or enable the following settings in your |fd| configuration:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = /usr/lib/bareos/plugins
     Plugin Names = python3
     ...
   }

Note: Depending on the platform, the Plugin Directory may also be :file:`/usr/lib64/bareos/plugins`

To define the backup of a VM in Bareos, a job definition and a fileset resource
must be added to the Bareos director configuration. In |proxmox ve|, guests have
an unique ID (i.e. a number between *100* - *999999999*) that uniquely identifies
each guest.

The following example shows how to configure the backup of the *KVM* guest with
the ID **999001** and a *Container* guest with the ID **999002**:

The |proxmox| plugin automatically detects the different guest types and handles
them accordingly. The fileset only differs in the guestid to be backed up, the
guest type is neither required nor possible to configure. 

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Proxmox Plugin Job and FileSet definitions

   Job {
     Name = "backup-proxmox-vm"
     Client = "bareos-fd"
     FileSet = "ProxmoxTestVM"
     JobDefs = "DefaultJob"
   }

   FileSet {
     Name = "ProxmoxTestVM"
     Description = "Test the Plugin functionality of the Proxmox Plugin."
     Include {
       options {
         signature = "xxh128"
         hardlinks = No
         AclSupport = Yes
         XattrSupport = Yes
       }
       Plugin = "python"
                ":module_name=bareos-fd-proxmox"
                ":guestid=999001"
     }
   }

   FileSet {
     Name = "ProxmoxTestCT"
     Description = "Test the Plugin functionality of the Proxmox Plugin."
     Include {
       Options {
         Signature = "XXH128"
         HardLinks = No
         AclSupport = Yes
         XattrSupport = Yes
       }
       Plugin = "python"
                ":module_name=bareos-fd-proxmox"
                ":guestid=999002"
     }
   }


Backup of a KVM guest
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bconsole
   :caption: Example backup of a KVM guest with the proxmox plugin

   * <input>run job=backup-proxmox-vm level=Full yes</input>
    You have messages.
   * <input>messages</input>
   25-Sep 09:15 bareos-dir JobId 1: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
   25-Sep 09:15 bareos-dir JobId 1: Start Backup JobId 1, Job=backup-proxmox-vm.2025-09-25_09.15.07_03
   25-Sep 09:15 bareos-dir JobId 1: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   25-Sep 09:15 bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   25-Sep 09:15 bareos-dir JobId 1: Probing client protocol... (result will be saved until config reload)
   25-Sep 09:15 bareos-dir JobId 1: Connected Client: bareos-fd at proxmox1.bareos.com:30542, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   25-Sep 09:15 bareos-dir JobId 1:    Handshake: Immediate TLS 
   25-Sep 09:15 bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   25-Sep 09:15 bareos-sd JobId 1: Using just in time reservation for job 1
   25-Sep 09:15 bareos-dir JobId 1: Using Device "JustInTime Device" to write.
   25-Sep 09:15 proxmox-fd JobId 1: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   25-Sep 09:15 proxmox-fd JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   25-Sep 09:15 bareos-sd JobId 1: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
   25-Sep 09:15 proxmox-fd JobId 1: Extended attribute support is enabled
   25-Sep 09:15 proxmox-fd JobId 1: ACL support is enabled
   25-Sep 09:15 proxmox-fd JobId 1: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: Executing ['/usr/bin/vzdump', '999001', '--stdout']
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: sending archive to stdout
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: Backing up qemu guest "plugtestvm" to virtual file /var/lib/vz/dump/vzdump-qemu-999001-2025_09_25-09_15_10.vma
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: starting new backup job: vzdump 999001
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: Starting Backup of VM 999001 (qemu)
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: Backup started at 2025-09-25 09:15:10
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: status = running
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: VM Name: plugtestvm
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: include disk 'scsi0' 'ZFS:vm-999001-disk-0' 2G
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: backup mode: snapshot
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: ionice priority: 7
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: sending archive to stdout
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: started backup task '68b0bc16-7bcb-4d00-a1f9-08893c9a7b02'
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: resuming VM again
   25-Sep 09:15 bareos-sd JobId 1: JustInTime Reservation: Finding drive to reserve.
   25-Sep 09:15 bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
   25-Sep 09:15 bareos-sd JobId 1: Using Device "FileStorage" (storage) to write.
   25-Sep 09:15 bareos-sd JobId 1: Labeled new Volume "Full-0001" on device "FileStorage" (storage).
   25-Sep 09:15 bareos-sd JobId 1: Moving to end of data on volume "Full-0001"
   25-Sep 09:15 bareos-sd JobId 1: Ready to append to end of Volume "Full-0001" size=227
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: 100% (2.0 GiB of 2.0 GiB) in 1s, read: 2.0 GiB/s, write: 0 B/s
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: backup is sparse: 2.00 GiB (100%) total zero data
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: transferred 2.00 GiB in 1 seconds (2.0 GiB/s)
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: Finished Backup of VM 999001 (00:00:01)
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: Backup finished at 2025-09-25 09:15:11
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: Backup job finished successfully
   25-Sep 09:15 proxmox-fd JobId 1: python3-fd-mod: INFO: skipping disabled target 'mail-to-root'
   25-Sep 09:15 bareos-sd JobId 1: Releasing device "FileStorage" (storage).
   25-Sep 09:15 bareos-sd JobId 1: Elapsed time=00:00:03, Transfer rate=100.7 K Bytes/second
   25-Sep 09:15 bareos-dir JobId 1: Insert of attributes batch table with 1 entries start
   25-Sep 09:15 bareos-dir JobId 1: Insert of attributes batch table done
   25-Sep 09:15 bareos-dir JobId 1: Bareos bareos-dir 23.0.0~pre3596.820496dab.dirty (23Sep25):
     Build OS:               Debian GNU/Linux 12 (bookworm)
     JobId:                  1
     Job:                    backup-proxmox-vm.2025-09-25_09.15.07_03
     Backup Level:           Full
     Client:                 "bareos-fd" 23.0.0~pre3596.820496dab.dirty (23Sep25) Debian GNU/Linux 12 (bookworm),debian
     FileSet:                "ProxmoxTestVM" 2025-09-25 09:15:07
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         25-Sep-2025 09:15:05
     Start time:             25-Sep-2025 09:15:09
     End time:               25-Sep-2025 09:15:12
     Elapsed time:           3 secs
     Priority:               10
     Allow Mixed Priority:   no
     FD Files Written:       1
     SD Files Written:       1
     FD Bytes Written:       302,080 (302.0 KB)
     SD Bytes Written:       302,370 (302.3 KB)
     Rate:                   100.7 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Full-0001
     Volume Session Id:      1
     Volume Session Time:    1758784489
     Last Volume Bytes:      303,095 (303.0 KB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Self-compiled: Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK
   
Backup of a Container guest
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bconsole
   :caption: Example backup of a Container guest with the proxmox plugin

    *<input>run job=backup-proxmox-vm level=Full yes</input>
    You have messages.
    *<input>messages</input>
    25-Sep 09:25 bareos-dir JobId 1: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:25 bareos-dir JobId 1: Start Backup JobId 1, Job=backup-proxmox-vm.2025-09-25_09.25.35_03
    25-Sep 09:25 bareos-dir JobId 1: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:25 bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:25 bareos-dir JobId 1: Probing client protocol... (result will be saved until config reload)
    25-Sep 09:25 bareos-dir JobId 1: Connected Client: bareos-fd at proxmox1.bareos.com:30542, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:25 bareos-dir JobId 1:    Handshake: Immediate TLS 
    25-Sep 09:25 bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:25 bareos-sd JobId 1: Using just in time reservation for job 1
    25-Sep 09:25 bareos-dir JobId 1: Using Device "JustInTime Device" to write.
    25-Sep 09:25 proxmox-fd JobId 1: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:25 proxmox-fd JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:25 proxmox-fd JobId 1: Extended attribute support is enabled
    25-Sep 09:25 proxmox-fd JobId 1: ACL support is enabled
    25-Sep 09:25 bareos-sd JobId 1: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:25 proxmox-fd JobId 1: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: Executing ['/usr/bin/vzdump', '999001', '--stdout']
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: sending archive to stdout
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: Backing up qemu guest "plugtestvm" to virtual file /var/lib/vz/dump/vzdump-qemu-999001-2025_09_25-09_25_38.vma
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: starting new backup job: vzdump 999001
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: Starting Backup of VM 999001 (qemu)
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: Backup started at 2025-09-25 09:25:38
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: status = running
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: VM Name: plugtestvm
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: include disk 'scsi0' 'ZFS:vm-999001-disk-0' 2G
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: backup mode: snapshot
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: ionice priority: 7
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: sending archive to stdout
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: started backup task 'd41f6879-b15e-451c-a42d-d40f21360f16'
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: resuming VM again
    25-Sep 09:25 bareos-sd JobId 1: JustInTime Reservation: Finding drive to reserve.
    25-Sep 09:25 bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
    25-Sep 09:25 bareos-sd JobId 1: Using Device "FileStorage" (storage) to write.
    25-Sep 09:25 bareos-sd JobId 1: Labeled new Volume "Full-0001" on device "FileStorage" (storage).
    25-Sep 09:25 bareos-sd JobId 1: Moving to end of data on volume "Full-0001"
    25-Sep 09:25 bareos-sd JobId 1: Ready to append to end of Volume "Full-0001" size=227
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: 100% (2.0 GiB of 2.0 GiB) in 1s, read: 2.0 GiB/s, write: 0 B/s
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: backup is sparse: 2.00 GiB (100%) total zero data
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: transferred 2.00 GiB in 1 seconds (2.0 GiB/s)
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: Finished Backup of VM 999001 (00:00:01)
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: Backup finished at 2025-09-25 09:25:39
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: Backup job finished successfully
    25-Sep 09:25 proxmox-fd JobId 1: python3-fd-mod: INFO: skipping disabled target 'mail-to-root'
    25-Sep 09:25 bareos-sd JobId 1: Releasing device "FileStorage" (storage).
    25-Sep 09:25 bareos-sd JobId 1: Elapsed time=00:00:02, Transfer rate=151.1 K Bytes/second
    25-Sep 09:25 bareos-dir JobId 1: Insert of attributes batch table with 1 entries start
    25-Sep 09:25 bareos-dir JobId 1: Insert of attributes batch table done
    25-Sep 09:25 bareos-dir JobId 1: Bareos bareos-dir 23.0.0~pre3596.820496dab.dirty (23Sep25):
      Build OS:               Debian GNU/Linux 12 (bookworm)
      JobId:                  1
      Job:                    backup-proxmox-vm.2025-09-25_09.25.35_03
      Backup Level:           Full
      Client:                 "bareos-fd" 23.0.0~pre3596.820496dab.dirty (23Sep25) Debian GNU/Linux 12 (bookworm),debian
      FileSet:                "ProxmoxTestVM" 2025-09-25 09:25:35
      Pool:                   "Full" (From Job FullPool override)
      Catalog:                "MyCatalog" (From Client resource)
      Storage:                "File" (From Job resource)
      Scheduled time:         25-Sep-2025 09:25:35
      Start time:             25-Sep-2025 09:25:37
      End time:               25-Sep-2025 09:25:39
      Elapsed time:           2 secs
      Priority:               10
      Allow Mixed Priority:   no
      FD Files Written:       1
      SD Files Written:       1
      FD Bytes Written:       302,080 (302.0 KB)
      SD Bytes Written:       302,370 (302.3 KB)
      Rate:                   151.0 KB/s
      Software Compression:   None
      VSS:                    no
      Encryption:             no
      Accurate:               no
      Volume name(s):         Full-0001
      Volume Session Id:      1
      Volume Session Time:    1758784831
      Last Volume Bytes:      303,095 (303.0 KB)
      Non-fatal FD errors:    0
      SD Errors:              0
      FD termination status:  OK
      SD termination status:  OK
      Bareos binary info:     Self-compiled: Get professional support from https://www.bareos.com
      Job triggered by:       User
      Termination:            Backup OK
    


Restoration of a guest to its original guest ID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Restoring a guest to its original guest ID is the easiest way to restore a |proxmox| backup.
This is only possible if the guest ID does not exist on the |proxmox ve|. If the ID exists, the restore will fail.

To restore, select the jobid of the full backup and mark the file that is stored in the backup for restoration and restore
the job to the proxmox filedaemon:

.. code-block:: bconsole
   :caption: Example restore of a KVM guest to its original guest ID

    *<input>restore jobid=1 all done</input>
    You have selected the following JobId: 1
    
    Building directory tree for JobId(s) 1 ...  
    1 files inserted into the tree and marked for extraction.
    [...]
    Run Restore job
    JobName:         RestoreFiles
    [...]
    Priority:        10
    Plugin Options:  *None*
    OK to run? (yes/mod/no): <input>yes</input>
    Job queued. JobId=4
    *
    You have messages.
    *<input>messages</input>
    25-Sep 09:48 bareos-dir JobId 4: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:48 bareos-dir JobId 4: Start Restore Job RestoreFiles.2025-09-25_09.48.28_12
    25-Sep 09:48 bareos-dir JobId 4: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:48 bareos-dir JobId 4:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:48 bareos-dir JobId 4: Using Device "FileStorage" to read.
    25-Sep 09:48 bareos-dir JobId 4: Connected Client: bareos-fd at proxmox1.bareos.com:30542, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:48 bareos-dir JobId 4:  Handshake: Immediate TLS 
    25-Sep 09:48 bareos-dir JobId 4:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:48 proxmox-fd JobId 4: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:48 bareos-sd JobId 4: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:48 proxmox-fd JobId 4:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:48 bareos-sd JobId 4: Ready to read from volume "TestVolume001" on device "FileStorage" (storage).
    25-Sep 09:48 proxmox-fd JobId 4: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: Executing ['/usr/sbin/qmrestore', '-', '999001', '--force', 'yes']
    25-Sep 09:48 bareos-sd JobId 4: Forward spacing Volume "TestVolume001" to file:block 0:231.
    25-Sep 09:48 bareos-sd JobId 4: End of Volume at file 0 on device "FileStorage" (storage), Volume "TestVolume001"
    25-Sep 09:48 bareos-sd JobId 4: End of all volumes.
    25-Sep 09:48 bareos-sd JobId 4: Releasing device "FileStorage" (storage).
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: restore vma archive: vma extract -v -r /var/tmp/vzdumptmp2202285.fifo - /var/tmp/vzdumptmp2202285
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: CFG: size: 274 name: qemu-server.conf
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: DEV: dev_id=1 size: 2147483648 devname: drive-scsi0
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: CTIME: Thu Sep 25 09:37:45 2025
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: new volume ID is 'ZFS:vm-999001-disk-0'
    25-Sep 09:48 proxmox-fd JobId 4: python3-fd-mod: map 'drive-scsi0' to '/dev/zvol/ZFS/vm-999001-disk-0' (write zeros = 0)
    25-Sep 09:48 bareos-dir JobId 4: Bareos bareos-dir 23.0.0~pre3596.820496dab.dirty (23Sep25):
      Build OS:               Debian GNU/Linux 12 (bookworm)
      JobId:                  4
      Job:                    RestoreFiles.2025-09-25_09.48.28_12
      Restore Client:         "bareos-fd" 23.0.0~pre3596.820496dab.dirty (23Sep25) Debian GNU/Linux 12 (bookworm),debian
      Start time:             25-Sep-2025 09:48:30
      End time:               25-Sep-2025 09:48:34
      Elapsed time:           4 secs
      Files Expected:         1
      Files Restored:         1
      Bytes Restored:         302,080
      Rate:                   75.5 KB/s
      FD Errors:              0
      FD termination status:  OK
      SD termination status:  OK
      Bareos binary info:     Self-compiled: Get professional support from https://www.bareos.com
      Job triggered by:       User
      Termination:            Restore OK

The guest has been recovered and can be now be started via commandline or |proxmox ve| gui.

While the above example shows the restore of a KVM guest, the procedure is
exactly the same to recover a Container guest:


.. code-block:: bconsole
   :caption: Example restore of a Container guest to its original guest ID

    *<input>restore jobid=1 all done</input>
    Automatically selected Catalog: MyCatalog
    Using Catalog "MyCatalog"
    You have selected the following JobId: 1
    
    Building directory tree for JobId(s) 1 ...  
    [...]
    When:            2025-09-25 09:57:51
    Catalog:         MyCatalog
    Priority:        10
    Plugin Options:  *None*
    OK to run? (yes/mod/no): <input>yes</input>
    Job queued. JobId=3
    *
    You have messages.
    *<input>messages</input>
    25-Sep 09:57 bareos-dir JobId 3: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:57 bareos-dir JobId 3: Start Restore Job RestoreFiles.2025-09-25_09.57.52_08
    25-Sep 09:57 bareos-dir JobId 3: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:57 bareos-dir JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:57 bareos-dir JobId 3: Using Device "FileStorage" to read.
    25-Sep 09:57 bareos-dir JobId 3: Connected Client: bareos-fd at proxmox1.bareos.com:30542, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:57 bareos-dir JobId 3:  Handshake: Immediate TLS 
    25-Sep 09:57 bareos-dir JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:57 proxmox-fd JobId 3: Connected Storage daemon at proxmox1.bareos.com:30543, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:57 bareos-sd JobId 3: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:57 proxmox-fd JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
    25-Sep 09:57 bareos-sd JobId 3: Ready to read from volume "TestVolume001" on device "FileStorage" (storage).
    25-Sep 09:57 proxmox-fd JobId 3: Version: 23.0.0~pre3596.820496dab.dirty (23 September 2025) Debian GNU/Linux 12 (bookworm)
    25-Sep 09:57 proxmox-fd JobId 3: python3-fd-mod: Executing ['/usr/sbin/pct', 'restore', '999002', '-', '--rootfs', '/', '--force', 'yes']
    25-Sep 09:57 bareos-sd JobId 3: Forward spacing Volume "TestVolume001" to file:block 0:231.
    25-Sep 09:57 bareos-sd JobId 3: End of Volume at file 0 on device "FileStorage" (storage), Volume "TestVolume001"
    25-Sep 09:57 bareos-sd JobId 3: End of all volumes.
    25-Sep 09:57 bareos-sd JobId 3: Releasing device "FileStorage" (storage).
    25-Sep 09:57 proxmox-fd JobId 3: python3-fd-mod: extracting archive from STDIN
    25-Sep 09:57 bareos-dir JobId 3: Bareos bareos-dir 23.0.0~pre3596.820496dab.dirty (23Sep25):
      Build OS:               Debian GNU/Linux 12 (bookworm)
      JobId:                  3
      Job:                    RestoreFiles.2025-09-25_09.57.52_08
      Restore Client:         "bareos-fd" 23.0.0~pre3596.820496dab.dirty (23Sep25) Debian GNU/Linux 12 (bookworm),debian
      Start time:             25-Sep-2025 09:57:54
      End time:               25-Sep-2025 09:57:58
      Elapsed time:           4 secs
      Files Expected:         1
      Files Restored:         1
      Bytes Restored:         569,466,880
      Rate:                   142366.7 KB/s
      FD Errors:              0
      FD termination status:  OK
      SD termination status:  OK
      Bareos binary info:     Self-compiled: Get professional support from https://www.bareos.com
      Job triggered by:       User
      Termination:            Restore OK



Restore to a different guest ID
'''''''''''''''''''''''''''''''
To restore to a different guest ID, the new guest ID needs to be specified in
the parameter ``guestid`` in the "Plugin Options String" during restore:

.. code-block:: bareosconfig
 
   python:guestid=<newguestid>

The next bconsole session shows how to restore an existing backup to the new guestid **8888** by passing the desired guestid
to the restore process via *plugin options*:

.. code-block:: bconsole
   :caption: Example restore to a new guest ID

    *<input>restore jobid=1 all done</input>
    Automatically selected Catalog: MyCatalog
    Using Catalog "MyCatalog"
    You have selected the following JobId: 1
    
    Building directory tree for JobId(s) 1 ...  
    [...]
    When:            2025-09-25 11:56:58
    Catalog:         MyCatalog
    Priority:        10
    Plugin Options:  *None*
    OK to run? (yes/mod/no): <input>mod</input>
    Parameters to modify:
    [...]
    13: JobId
    14: Plugin Options
    Select parameter to modify (1-14): 14
    Please enter Plugin Options string: <input>python:guestid=8888</input>
    Run Restore job
    [...]
    When:            2025-09-25 11:56:58
    Catalog:         MyCatalog
    Priority:        10
    Plugin Options:  python:guestid=8888
    OK to run? (yes/mod/no): <input>yes</input>
    Job queued. JobId=4



Restore using Bareos WebUI
''''''''''''''''''''''''''

TODO: Verify that this is true!

When using the WebUI to restore a Proxmox Plugin job.
The Bareos WebUI will detect if a plugin based jobs is restore and will then
show an additional *Plugin options* field, here a plugin options string
starting with ``python:`` as described above can be entered.

Restore to ``.vma`` dump File
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Proxmox Plugin; vma dump files>`\


For restoring to local ``.vma`` dump file, the **Plugin Option** :strong:`restoretodisk=yes` must be passed.
Also, the **where** parameter needs to be set to :file:`/` to make
The restore will write the dump file into the standard directory for backup
dump files on the proxmox server which is :file:`/var/lib/vz/dump`

The following example shows how to
perform such a restore using :command:`bconsole`:

.. code-block:: bconsole
   :caption: Example restore to local vma dump file


    *<input>restore jobid=1 all done</input>
    Automatically selected Catalog: MyCatalog
    Using Catalog "MyCatalog"
    You have selected the following JobId: 1
    
    Building directory tree for JobId(s) 1 ...  
    [...]
    Priority:        10
    Plugin Options:  *None*
    OK to run? (yes/mod/no): <input>mod</input>
    Parameters to modify:
    [...]
     9: Bootstrap
    10: Where
    11: File Relocation
    [...]
    Select parameter to modify (1-14): <input>10</input>
    Please enter the full path prefix for restore (/ for none): <input>/</input>
    [...]
    Catalog:         MyCatalog
    Priority:        10
    Plugin Options:  *None*
    OK to run? (yes/mod/no): <input>mod</input>
    Parameters to modify:
    [...]
    13: JobId
    14: Plugin Options
    Select parameter to modify (1-14): <input>14</input>
    Please enter Plugin Options string: <input>python:restoretodisk=yes</input>
    Run Restore job
    JobName:         RestoreFiles
    Bootstrap:       /home/pstorz/git/bareos/root/systemtests/tests/proxmox/working/bareos-dir.restore.1.bsr
    Where:           
    Replace:         Always
    FileSet:         SelfTest
    Backup Client:   bareos-fd
    Restore Client:  bareos-fd
    Format:          Native
    Storage:         File
    When:            2025-09-25 12:12:23
    Catalog:         MyCatalog
    Priority:        10
    Plugin Options:  python:restoretodisk=yes
    OK to run? (yes/mod/no): <input>yes</input>
    Job queued. JobId=2



After the backup as been recovered into the default vzdump directory, the backup shows up in the |proxmox| gui and 
can be recovered just like any other proxmox local backup:

.. image:: /include/images/proxmox-gui-shows-recovered-backup.*
   :width: 90.0%




Description of all Plugin Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Note that all plugin options that have been used at backup time, are passed back on restore.

guestid (mandatory on backup)
   The ID of the VM to be backed up. On restore it is possible to override this option in order to recreate the VM with a different ID.

restoretodisk (optional)
   Restore to local ``.vma`` vzdump file instead of restoreing to guest.. Default is *no*.
