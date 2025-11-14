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

The Plugin can do full backups and offers two options for restoring them:

* It can restore the backup into a new proxmox guest (Virtual Machine / Container).
* It can restore the backup into a local vzdump dump file to be recovered later via the proxmox GUI.


.. limitation:: Proxmox Plugin: Only Full Backups are currently supported.

   The Proxmox Plugin currently only supports Full backups.


Requirements
^^^^^^^^^^^^

The Proxmox Plugin (together with the |fd|) needs to be installed on a member of the proxmox cluster
that is running the guests that need to be backed up.

Installation
^^^^^^^^^^^^

Start by adding the matching repository according to the following table to one of
the |proxmox ve| cluster members.

================== ========================
Proxmox VE Version Bareos Repository to use
================== ========================
8.x                Debian_12
9.x                Debian_13
================== ========================

Then install the package **bareos-filedaemon-proxmox-plugin** (using :command:`apt`)
on one of the |proxmox ve| cluster members. This will automatically also
install the |fd| and other required packages:

.. code-block:: sh
   :caption: Install the proxmox plugin on one |proxmox ve| cluster member

   root@proxmox90:~#<input>apt install bareos-filedaemon-proxmox-python-plugin</input>
   Installing:                      
     bareos-filedaemon-proxmox-python-plugin
   
   Installing dependencies:
     bareos-common      bareos-filedaemon-python-plugins-common  gawk      libmpfr6       libsigsegv2
     bareos-filedaemon  bareos-filedaemon-python3-plugin         libfmt10  libpython3.13  python3-dateutil
   
   Suggested packages:
     gawk-doc
   
   Summary:
     Upgrading: 0, Installing: 11, Removing: 0, Not Upgrading: 0
     Download size: 1,381 kB / 5,184 kB
     Space needed: 17.4 MB / 3,892 MB available
   
   Continue? [Y/n] y
   Get:1 https://download.bareos.com/bareos/release/25/Debian_13  bareos-common 25.0.0-65 [988 kB]
   Get:2 https://download.bareos.com/bareos/release/25/Debian_13  bareos-filedaemon 25.0.0-65 [257 kB]
   Get:3 https://download.bareos.com/bareos/release/25/Debian_13  bareos-filedaemon-python-plugins-common 25.0.0-65 [39.2 kB]
   Get:4 https://download.bareos.com/bareos/release/25/Debian_13  bareos-filedaemon-python3-plugin 25.0.0-65 [62.4 kB]
   Get:5 https://download.bareos.com/bareos/release/25/Debian_13  bareos-filedaemon-proxmox-python-plugin 25.0.0-65 [34.5 kB]
   Fetched 1,381 kB in 1s (1,188 kB/s)  
   [...]


Configuration
^^^^^^^^^^^^^


Switch the |fd| runtime group to  `root`
''''''''''''''''''''''''''''''''''''''''

The |fd| is usually running as user ``root`` and group ``bareos``.

Unfortunately this does not work on |proxmox ve|, as it prevents the |fd| from
backing up the guests.

Running the |fd| with the default group ``bareos`` leads to the following errors in the job logs:

.. code-block:: bconsole
   :caption: Error when running the |fd| as group ``bareos``

   [...] JobId 1: python3-fd-mod: Executing vzdump 100 --stdout
   [...] JobId 1: python3-fd-mod: <input>ipcc_send_rec[1] failed: Unknown error -1</input>
   [...] JobId 1: python3-fd-mod: <input>ipcc_send_rec[2] failed: Unknown error -1</input>
   [...] JobId 1: python3-fd-mod: <input>ipcc_send_rec[3] failed: Unknown error -1</input>
   [...] JobId 1: python3-fd-mod: Unable to load access control list: Unknown error -1

For the bareos proxmox plugin to work properly, the |fd| needs to run as user ``root`` and group ``root``.

This is achieved by adapting the systemd service to be started as Group ``root`` by using
``systemctl edit bareos-filedaemon``` an adding the following two lines:

.. code-block:: sh

    <input>[Service]</input>
    <input>Group=root</input>

like the following example:

.. code-block:: sh
   :caption: Edit the bareos-filedaemon system service to be run as group ``root``

   <input>systemctl edit bareos-filedaemon</input>

    ### Editing /etc/systemd/system/bareos-filedaemon.service.d/override.conf
    ### Anything between here and the comment below will become the new contents of the file

    <input>[Service]</input>
    <input>Group=root</input>

    ### Lines below this comment will be discarded

This will create the file :file:`/etc/systemd/system/bareos-filedaemon.service.d/override.conf`
containing the lines mentioned above. You can alternatively also create this file directly if desired.


Configure the |fd| to load the python plugin
''''''''''''''''''''''''''''''''''''''''''''

Make sure to add or enable the following settings in your |fd| configuration:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = "/usr/lib/bareos/plugins"
     Plugin Names = "python3"
     ...
   }

Restart the |fd| after the changes with :command:`systemctl restart bareos-fd`.
:qk
After connectng the client to the director as described in :ref:`section-AddAClient`,
we go on with configuring jobs and filesets.


Configure jobs and filesets
'''''''''''''''''''''''''''

To define the backup of a VM in Bareos, a job definition and a fileset resource
must be added to the Bareos director configuration. In |proxmox ve|, guests have
an unique ID (i.e. a number between **100** - **999999999**) that uniquely identifies
each guest. The guest ID is visible in the |proxmox ve| GUI next to the name as can be seen in figure :ref:`proxmox_gui` .

The following example shows how to configure the backup of the *KVM* guest with
the ID **100** and a *Container* guest with the ID **101**:

The |proxmox| plugin automatically detects the different guest types and handles
them accordingly. The fileset only differs in the guestid to be backed up, the
guest type is neither required nor possible to configure. 

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Proxmox Plugin Job and FileSet definitions

   Job {
     Name = "proxmox90-vm"
     Client = "proxmox90-fd"
     FileSet = "ProxmoxTestVM"
     JobDefs = "DefaultJob"
     Level = Full
   }

   Job {
     Name = "proxmox90-ct"
     Client = "proxmox90-fd"
     FileSet = "ProxmoxTestCT"
     JobDefs = "DefaultJob"
     Level = Full
   }

   FileSet {
     Name = "ProxmoxTestVM"
     Description = "Backup a Proxmox Virtual Machine"
     Include {
       options {
         signature = "xxh128"
         hardlinks = No
         AclSupport = Yes
         XattrSupport = Yes
       }
       Plugin = "python"
                ":module_name=bareos-fd-proxmox"
                ":guestid=100"
     }
   }

   FileSet {
     Name = "ProxmoxTestCT"
     Description = "Backup a Proxmox Container"
     Include {
       options {
         signature = "xxh128"
         hardlinks = No
         AclSupport = Yes
         XattrSupport = Yes
       }
       Plugin = "python"
                ":module_name=bareos-fd-proxmox"
                ":guestid=101"
     }
   }



Backup of a KVM guest
^^^^^^^^^^^^^^^^^^^^^

To run a backup of a KVM guest, simply start the corresponding job ``proxmox90-vm``:

.. code-block:: bconsole
   :caption: Example backup of a KVM guest with the proxmox plugin

   *<input>run job=proxmox90-vm level=Full yes</input>
   Using Catalog "MyCatalog"
   Job queued. JobId=1
   *You have messages.
   *messages
   [...] bareos-dir JobId 1: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 1: Start Backup JobId 1, Job=proxmox90-vm.2025-11-12_10.21.41_05
   [...] bareos-dir JobId 1: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1: Connected Client: proxmox90-fd at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1:  Handshake: Immediate TLS
   [...] bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 1: Using just in time reservation for job 1
   [...] bareos-dir JobId 1: Using Device "JustInTime Device" to write.
   [...] bareos-sd JobId 1: Connected File Daemon at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] proxmox90-fd JobId 1: Extended attribute support is enabled
   [...] bareos-sd JobId 1: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] proxmox90-fd JobId 1: ACL support is enabled
   [...] proxmox90-fd JobId 1: Version: 25.0.0~pre1059.ff31e25a9 (10 November 2025) Debian GNU/Linux 13 (trixie)
   [...] proxmox90-fd JobId 1: python3-fd-mod: Executing vzdump 100 --stdout
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: starting new backup job: vzdump 100
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: Starting Backup of VM 100 (qemu)
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: Backup started at 2025-11-12 10:21:46
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: status = running
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: include disk 'scsi0' 'local-lvm:vm-100-disk-0' 10G
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: backup mode: snapshot
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: ionice priority: 7
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: sending archive to stdout
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: started backup task 'f8e2c068-679d-43d5-82ef-168887ba4e76'
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: resuming VM again
   [...] proxmox90-fd JobId 1: python3-fd-mod: Guest name not set, defaulting to "VM 100"
   [...] bareos-sd JobId 1: JustInTime Reservation: Finding drive to reserve.
   [...] proxmox90-fd JobId 1: python3-fd-mod: Backing up qemu guest "VM 100" to virtual file PROXMOX/VM 100/vzdump-qemu-100-2025_11_12-10_21_46.vma
   [...] bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
   [...] bareos-sd JobId 1: Using Device "File0001" (/var/lib/bareos/storage) to write.
   [...] bareos-sd JobId 1: Labeled new Volume "Full-0001" on device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 1: Moving to end of data on volume "Full-0001"
   [...] bareos-sd JobId 1: Ready to append to end of Volume "Full-0001" size=223
   [...] bareos-dir JobId 1: Max Volume jobs=1 exceeded. Marking Volume "Full-0001" as Used.
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO:  23% (2.4 GiB of 10.0 GiB) in 3s, read: 812.0 MiB/s, write: 199.9 MiB/s
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO:  40% (4.1 GiB of 10.0 GiB) in 6s, read: 582.4 MiB/s, write: 181.2 MiB/s
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO:  65% (6.5 GiB of 10.0 GiB) in 9s, read: 839.8 MiB/s, write: 259.7 MiB/s
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO:  92% (9.3 GiB of 10.0 GiB) in 12s, read: 924.4 MiB/s, write: 226.2 MiB/s
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: 100% (10.0 GiB of 10.0 GiB) in 13s, read: 764.2 MiB/s, write: 0 B/s
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: backup is sparse: 7.46 GiB (74%) total zero data
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: transferred 10.00 GiB in 13 seconds (787.7 MiB/s)
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: Finished Backup of VM 100 (00:00:13)
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: Backup finished at 2025-11-12 10:21:59
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: Backup job finished successfully
   [...] proxmox90-fd JobId 1: python3-fd-mod: INFO: notified via target `mail-to-root`
   [...] proxmox90-fd JobId 1: python3-fd-mod: waiting for command to finish
   [...] bareos-sd JobId 1: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 1: Elapsed time=00:00:13, Transfer rate=209.9 M Bytes/second
   [...] bareos-dir JobId 1: Insert of attributes batch table with 1 entries start
   [...] bareos-dir JobId 1: Insert of attributes batch table done
   [...] bareos-dir JobId 1: Bareos bareos-dir 25.0.0~pre1327.fd0f53dc3 (31Oct25):
     Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
     JobId:                  1
     Job:                    proxmox90-vm.2025-11-12_10.21.41_05
     Backup Level:           Full
     Client:                 "proxmox90-fd" 25.0.0~pre1059.ff31e25a9 (10Nov25) Debian GNU/Linux 13 (trixie),debian
     FileSet:                "ProxmoxTestVM" 2025-11-12 10:21:41
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         12-Nov-2025 10:21:41
     Start time:             12-Nov-2025 10:21:44
     End time:               12-Nov-2025 10:21:59
     Elapsed time:           15 secs
     Priority:               10
     Allow Mixed Priority:   no
     FD Files Written:       1
     SD Files Written:       1
     FD Bytes Written:       2,728,973,824 (2.728 GB)
     SD Bytes Written:       2,728,974,013 (2.728 GB)
     Rate:                   181931,6 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Full-0001
     Volume Session Id:      1
     Volume Session Time:    1762934122
     Last Volume Bytes:      2,729,193,300 (2.729 GB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK
   



Backup of a Container guest
^^^^^^^^^^^^^^^^^^^^^^^^^^^

To backup a container guest, also start the corresponding job ``proxmox90-ct``:

.. code-block:: bconsole
   :caption: Example backup of a Container guest with the proxmox plugin

   *<input>run job=proxmox90-ct level=Full yes</input>
   Job queued. JobId=2
   *<input>messages</input>
   [...] bareos-dir JobId 2: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 2: Start Backup JobId 2, Job=proxmox90-ct.2025-11-12_12.36.09_04
   [...] bareos-dir JobId 2: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2: Connected Client: proxmox90-fd at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2:  Handshake: Immediate TLS
   [...] bareos-dir JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 2: Using just in time reservation for job 2
   [...] bareos-dir JobId 2: Using Device "JustInTime Device" to write.
   [...] bareos-sd JobId 2: Connected File Daemon at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] proxmox90-fd JobId 2: Extended attribute support is enabled
   [...] bareos-sd JobId 2: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] proxmox90-fd JobId 2: ACL support is enabled
   [...] proxmox90-fd JobId 2: Version: 25.0.0~pre1059.097ef5ba3 (12 November 2025) Debian GNU/Linux 13 (trixie)
   [...] proxmox90-fd JobId 2: python3-fd-mod: Executing vzdump 101 --stdout
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: starting new backup job: vzdump 101
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: Starting Backup of VM 101 (lxc)
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: Backup started at 2025-11-12 12:36:14
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: status = running
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: CT Name: CT101
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: including mount point rootfs ('/') in backup
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: backup mode: snapshot
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: ionice priority: 7
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: create storage snapshot 'vzdump'
   [...] proxmox90-fd JobId 2: python3-fd-mod:   WARNING: You have not turned on protection against thin pools running out of space.
   [...] proxmox90-fd JobId 2: python3-fd-mod:   WARNING: Set activation/thin_pool_autoextend_threshold below 100 to trigger automatic extension of thin pools before they get full.
   [...] proxmox90-fd JobId 2: python3-fd-mod:   Logical volume "snap_vm-101-disk-0_vzdump" created.
   [...] proxmox90-fd JobId 2: python3-fd-mod:   WARNING: Sum of all thin volume sizes (26.00 GiB) exceeds the size of thin pool pve/data and the amount of free space in volume group (<6.13 GiB).
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: sending archive to stdout
   [...] proxmox90-fd JobId 2: python3-fd-mod: Backing up lxc guest "CT101" to virtual file PROXMOX/CT101/vzdump-lxc-101-2025_11_12-12_36_14.vma
   [...] bareos-sd JobId 2: JustInTime Reservation: Finding drive to reserve.
   [...] bareos-dir JobId 2: Created new Volume "Full-0002" in catalog.
   [...] bareos-sd JobId 2: Using Device "File0001" (/var/lib/bareos/storage) to write.
   [...] bareos-sd JobId 2: Labeled new Volume "Full-0002" on device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 2: Moving to end of data on volume "Full-0002"
   [...] bareos-sd JobId 2: Ready to append to end of Volume "Full-0002" size=223
   [...] bareos-dir JobId 2: Max Volume jobs=1 exceeded. Marking Volume "Full-0002" as Used.
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: Total bytes written: 386027520 (369MiB, 30MiB/s)
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: cleanup temporary 'vzdump' snapshot
   [...] proxmox90-fd JobId 2: python3-fd-mod:   Logical volume "snap_vm-101-disk-0_vzdump" successfully removed.
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: Finished Backup of VM 101 (00:00:13)
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: Backup finished at 2025-11-12 12:36:27
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: Backup job finished successfully
   [...] proxmox90-fd JobId 2: python3-fd-mod: INFO: notified via target `mail-to-root`
   [...] proxmox90-fd JobId 2: python3-fd-mod: waiting for command to finish
   [...] bareos-sd JobId 2: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 2: Elapsed time=00:00:15, Transfer rate=25.73 M Bytes/second
   [...] bareos-dir JobId 2: Insert of attributes batch table with 1 entries start
   [...] bareos-dir JobId 2: Insert of attributes batch table done
   [...] bareos-dir JobId 2: Bareos bareos-dir 25.0.0~pre1327.fd0f53dc3 (31Oct25):
     Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
     JobId:                  2
     Job:                    proxmox90-ct.2025-11-12_12.36.09_04
     Backup Level:           Full
     Client:                 "proxmox90-fd" 25.0.0~pre1059.097ef5ba3 (12Nov25) Debian GNU/Linux 13 (trixie),debian
     FileSet:                "ProxmoxTestCT" 2025-11-12 12:36:09
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         12-Nov-2025 12:36:09
     Start time:             12-Nov-2025 12:36:11
     End time:               12-Nov-2025 12:36:28
     Elapsed time:           17 secs
     Priority:               10
     Allow Mixed Priority:   no
     FD Files Written:       1
     SD Files Written:       1
     FD Bytes Written:       386,027,520 (386.0 MB)
     SD Bytes Written:       386,027,707 (386.0 MB)
     Rate:                   22707,5 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Full-0002
     Volume Session Id:      2
     Volume Session Time:    1762947301
     Last Volume Bytes:      386,059,314 (386.0 MB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK





Restoration of a guest to its original guest ID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Restoring a guest to its original guest ID is the easiest way to restore a |proxmox| backup.

This is only possible if the guest ID does not exist on the |proxmox ve|.
If the ID exists, the restore will fail with the message ``unable to restore VM
100 - VM 100 already exists on node 'proxmox90'`` or similar.


To restore, select the jobid of the full backup, mark all files for
restoration and restore the job to the |proxmox| filedaemon:

.. code-block:: bconsole
   :caption: Example restore of a KVM guest to its original guest ID

   *restore jobid=1 all done
   You have selected the following JobId: 1
   [...]
   1 file selected to be restored.

   Defined Clients:
   5: proxmox90-fd
   Select the Client (1-6): 5
   Plugin Options:  *None*
   OK to run? (yes/mod/no): yes
   Job queued. JobId=5
   [...]
   * messages
   [...] bareos-dir JobId 5: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 5: Start Restore Job RestoreFiles.2025-11-12_12.51.05_09
   [...] bareos-dir JobId 5: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 5:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 5: Using Device "File0001" to read.
   [...] bareos-dir JobId 5: Connected Client: proxmox90-fd at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 5:  Handshake: Immediate TLS
   [...] bareos-dir JobId 5:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 5: Connected File Daemon at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 5: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-sd JobId 5: Ready to read from volume "Full-0001" on device "File0001" (/var/lib/bareos/storage).
   [...] proxmox90-fd JobId 5: Version: 25.0.0~pre1059.097ef5ba3 (12 November 2025) Debian GNU/Linux 13 (trixie)
   [...] proxmox90-fd JobId 5: python3-fd-mod: Executing qmrestore - 100
   [...] bareos-sd JobId 5: Forward spacing Volume "Full-0001" to file:block 0:223.
   [...] proxmox90-fd JobId 5: python3-fd-mod: restore vma archive: vma extract -v -r /var/tmp/vzdumptmp60261.fifo - /var/tmp/vzdumptmp60261
   [...] bareos-sd JobId 5: End of Volume at file 0 on device "File0001" (/var/lib/bareos/storage), Volume "Full-0001"
   [...] bareos-sd JobId 5: End of all volumes.
   [...] proxmox90-fd JobId 5: python3-fd-mod: CFG: size: 496 name: qemu-server.conf
   [...] bareos-sd JobId 5: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] proxmox90-fd JobId 5: python3-fd-mod: DEV: dev_id=1 size: 10737418240 devname: drive-scsi0
   [...] proxmox90-fd JobId 5: python3-fd-mod: CTIME: Wed Nov 12 12:35:14 2025
   [...] proxmox90-fd JobId 5: python3-fd-mod:   WARNING: You have not turned on protection against thin pools running out of space.
   [...] proxmox90-fd JobId 5: python3-fd-mod:   WARNING: Set activation/thin_pool_autoextend_threshold below 100 to trigger automatic extension of thin pools before they get full.
   [...] proxmox90-fd JobId 5: python3-fd-mod:   Logical volume "vm-100-disk-0" created.
   [...] proxmox90-fd JobId 5: python3-fd-mod:   WARNING: Sum of all thin volume sizes (18.00 GiB) exceeds the size of thin pool pve/data and the amount of free space in volume group (<6.13 GiB).
   [...] proxmox90-fd JobId 5: python3-fd-mod:   Logical volume pve/vm-100-disk-0 changed.
   [...] proxmox90-fd JobId 5: python3-fd-mod: new volume ID is 'local-lvm:vm-100-disk-0'
   [...] proxmox90-fd JobId 5: python3-fd-mod: map 'drive-scsi0' to '/dev/pve/vm-100-disk-0' (write zeros = 0)
   [...] proxmox90-fd JobId 5: python3-fd-mod: progress 1% (read 107413504 bytes, duration 0 sec)
   [...] proxmox90-fd JobId 5: python3-fd-mod: progress 2% (read 214761472 bytes, duration 1 sec)
   [...]
   [...] proxmox90-fd JobId 5: python3-fd-mod: progress 97% (read 10415308800 bytes, duration 9 sec)
   [...] proxmox90-fd JobId 5: python3-fd-mod: progress 98% (read 10522722304 bytes, duration 9 sec)
   [...] proxmox90-fd JobId 5: python3-fd-mod: progress 99% (read 10630070272 bytes, duration 9 sec)
   [...] proxmox90-fd JobId 5: python3-fd-mod: progress 100% (read 10737418240 bytes, duration 9 sec)
   [...] proxmox90-fd JobId 5: python3-fd-mod: waiting for command to finish
   [...] bareos-dir JobId 5: Bareos bareos-dir 25.0.0~pre1327.fd0f53dc3 (31Oct25):
     Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
     JobId:                  5
     Job:                    RestoreFiles.2025-11-12_12.51.05_09
     Restore Client:         "proxmox90-fd" 25.0.0~pre1059.097ef5ba3 (12Nov25) Debian GNU/Linux 13 (trixie),debian
     Start time:             12-Nov-2025 12:51:07
     End time:               12-Nov-2025 12:51:24
     Elapsed time:           17 secs
     Files Expected:         1
     Files Restored:         1
     Bytes Restored:         2,728,977,920
     Rate:                   160528,1 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Restore OK



The guest has been recovered and can be now be started via commandline or |proxmox ve| GUI.

While the above example shows the restore of a KVM guest, the procedure to
recover a container guest is slightly different.
When recovering a container backup, the target storage and size need to be
passed via the plugin paramter ``pctstorage`` to the plugin in the format

``pctstorage=<storagename>\:<size-in-GiB>``

.. note ::
   This requirement will probably be removed in the next version of the plugin.
   It is planned to record the original storage and size during the backup and
   restore to the same recorded values with the option to alter the values
   during restore.

In the following example, we use ``local-lvm`` as storage and ``8`` as size
which results in the following plugin options:

.. code-block:: bareosconfig
   :caption: pctstorage setting for container restores

   python:pctstorage=local-lvm\:8

.. code-block:: bconsole
   :caption: Example restore of a container guest to its original guest ID

   python:pctstorage=local-lvm\:8

   *<input>restore jobid=2 all done</input>
   Automatically selected Catalog: MyCatalog
   [...]
   1 file selected to be restored.
   [...]
   6: proxmox90-fd
   [...]
   Select the Client (1-7): 6
   Using Catalog "MyCatalog"
   Run Restore job
   Priority:        10
   [...]
   Plugin Options:  *None*
   OK to run? (yes/mod/no): mod
   Parameters to modify:
   14: Plugin Options
   Select parameter to modify (1-14): <input>14</input>
   Please enter Plugin Options string: <input>python:pctstorage=local-lvm\:8</input>
   Run Restore job
   JobName:         RestoreFiles
   Priority:        10
   Plugin Options:  python:pctstorage=local-lvm\:8
   [...]
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=4
   [...] bareos-dir JobId 4: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-dir JobId 4: Start Restore Job RestoreFiles.2025-11-14_18.12.11_08
   [...] bareos-dir JobId 4: Connected Storage daemon at barri-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 4:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 4: Using Device "File0001" to read.
   [...] bareos-dir JobId 4: Connected Client: proxmox90-fd at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 4:  Handshake: Immediate TLS 
   [...] bareos-dir JobId 4:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 4: Connected File Daemon at proxmox90:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 4: Version: 25.0.0~pre1327.fd0f53dc3 (31 October 2025) AlmaLinux Kitten release 10 (Lion Cub)
   [...] bareos-sd JobId 4: Ready to read from volume "Full-0002" on device "File0001" (/var/lib/bareos/storage).
   [...] proxmox90-fd JobId 4: Version: 25.0.0~pre1092.5574206a7 (14 November 2025) Debian GNU/Linux 13 (trixie)
   [...] bareos-sd JobId 4: Forward spacing Volume "Full-0002" to file:block 0:223.
   [...] proxmox90-fd JobId 4: python3-fd-mod: Executing pct restore 101 - --rootfs local-lvm:8
   [...] proxmox90-fd JobId 4: python3-fd-mod:   WARNING: You have not turned on protection against thin pools running out of space.
   [...] proxmox90-fd JobId 4: python3-fd-mod:   WARNING: Set activation/thin_pool_autoextend_threshold below 100 to trigger automatic extension of thin pools before they get full.
   [...] proxmox90-fd JobId 4: python3-fd-mod:   Logical volume "vm-101-disk-0" created.
   [...] proxmox90-fd JobId 4: python3-fd-mod:   WARNING: Sum of all thin volume sizes (18.00 GiB) exceeds the size of thin pool pve/data and the amount of free space in volume group (<6.13 GiB).
   [...] proxmox90-fd JobId 4: python3-fd-mod:   Logical volume pve/vm-101-disk-0 changed.
   [...] bareos-sd JobId 4: End of Volume at file 0 on device "File0001" (/var/lib/bareos/storage), Volume "Full-0002"
   [...] bareos-sd JobId 4: End of all volumes.
   [...] bareos-sd JobId 4: Releasing device "File0001" (/var/lib/bareos/storage).
   [...] proxmox90-fd JobId 4: python3-fd-mod: Creating filesystem with 2097152 4k blocks and 524288 inodes
   [...] proxmox90-fd JobId 4: python3-fd-mod: Filesystem UUID: 6ab7d7e1-0886-40f5-8d66-8fdddeaae664
   [...] proxmox90-fd JobId 4: python3-fd-mod: Superblock backups stored on blocks: 
   [...] proxmox90-fd JobId 4: python3-fd-mod:      32768, 98304, 163840, 229376, 294912, 819200, 884736, 1605632
   [...] proxmox90-fd JobId 4: python3-fd-mod: extracting archive from STDIN
   [...] proxmox90-fd JobId 4: python3-fd-mod: Total bytes read: 386048000 (369MiB, 243MiB/s)
   [...] proxmox90-fd JobId 4: python3-fd-mod: merging backed-up and given configuration..
   [...] proxmox90-fd JobId 4: python3-fd-mod: waiting for command to finish
   [...] bareos-dir JobId 4: Bareos bareos-dir 25.0.0~pre1327.fd0f53dc3 (31Oct25):
     Build OS:               AlmaLinux Kitten release 10 (Lion Cub)
     JobId:                  4
     Job:                    RestoreFiles.2025-11-14_18.12.11_08
     Restore Client:         "proxmox90-fd" 25.0.0~pre1092.5574206a7 (14Nov25) Debian GNU/Linux 13 (trixie),debian
     Start time:             14-Nov-2025 18:12:13
     End time:               14-Nov-2025 18:12:21
     Elapsed time:           8 secs
     Files Expected:         1
     Files Restored:         1
     Bytes Restored:         386,048,000
     Rate:                   48256,0 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Restore OK
   

Overwriting an existing guest with the same ID
''''''''''''''''''''''''''''''''''''''''''''''
If you want to overwrite an existing guest with the same ID, it is required that:
 * The existing guest ist **powered off**
 * The restore is started with the plugin option **force=yes**

.. code-block:: bareosconfig
   :caption: allow to overwrite an exisiting guest

   python:force=yes

The options string can be set during the restore in the commandline in option `14: Plugin Options` or in the webui
in the plugin options field.

Restore to a different guest ID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
To restore to a different guest ID, the new guest ID needs to be specified in
the parameter ``guestid`` in the "Plugin Options String" during restore:

.. code-block:: bareosconfig
   :caption: changing the guest ID during restore
 
   python:guestid=<newguestid>



Restore to ``.vma`` dump File
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Proxmox Plugin; vma dump files>`\


For restoring to local ``.vma`` dump file, the **Plugin Option** :strong:`restoretodisk=yes` must be passed.
The restore will write the dump file into the standard directory for backup
dump files on the proxmox server which is :file:`/var/lib/vz/dump`

.. code-block:: bareosconfig
   :caption: restore the backup into the local filesystem of the proxmox server

   python:restoretodisk=yes

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



After the backup as been recovered into the default vzdump directory, the backup shows up in the |proxmox| GUI and
can be recovered just like any other proxmox local backup:

.. _proxmox_gui:

.. figure:: /include/images/proxmox-gui-shows-recovered-backup.*
   :width: 90.0%
   :alt: |proxmox ve| GUI showing the recovered backup

   |proxmox ve| GUI showing the recovered backup

Restore using Bareos WebUI
''''''''''''''''''''''''''

When using the WebUI to restore a Proxmox Plugin job.
The Bareos WebUI will detect if a plugin based jobs is about to be restored and will
show an additional *Plugin options* field. Add the required plugin options string here
starting with ``python:`` as described above.


Description of all Plugin Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Note that all plugin options that have been used at backup time, are passed
back on restore. Options given on restore will override the values originating
from the backup.


guestid (mandatory on backup)
   The ID of the guest to be backed up. On restore it is possible to override this
   option in order to recreate the guest with a different ID.

   example: ``python:guestid=999``

force (optional)
   This allows to overwrite existing guests with the same ID during restore.

   example: ``python:force=yes``

restoretodisk (optional)
   Restore to local ``.vma`` vzdump file instead of restoring a guest directly. Default is *no*.

   example: ``python:restoretodisk=yes``

restorepath (optional)
   Path where the local ``.vma`` vzdump file will be restored. Default is :file:`/var/lib/vzdump`.

   example: ``python:restorepath=/var/lib/vzdump/bareos-restores``

pctstorage (required during restore of container guests)
   Storage and storage size in GiB where the container image will be restored in the format

   ``pctstorage=<storage-name>\:<size-in-GiB``
   
   Please not that the colon needs to be escaped with a backslash.

   example: ``python:pctstorage=local-lvm\:8``


.. warning::

   As the colon (**:**) needs to be escaped with a backslash (**\\**) when passing it as plugin parameter during container restore.
   This is required because the colon is the delimiter between plugin parameters.
