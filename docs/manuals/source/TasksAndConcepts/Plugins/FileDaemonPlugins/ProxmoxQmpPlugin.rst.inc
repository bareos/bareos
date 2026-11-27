.. _ProxmoxQmpPlugin:

Proxmox QMP Plugin
~~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Proxmox QMP
   single: Proxmox QMP Plugin

The Proxmox QMP Plugin backs up the configuration and disks of a running
|proxmox ve| virtual machine using the QEMU Machine Protocol (QMP).
It is a separate plugin from the :ref:`Proxmox Plugin <ProxmoxPlugin>`.

Status
^^^^^^

The plugin supports Full and Incremental backups of QEMU virtual machines.
Differential backups and container guests are not supported.
Restores operate on whole disks; individual files inside a guest cannot be
selected for restore.

The plugin backs up the VM configuration and supported disk devices, but does
not back up the data volume configured as ``tpmstate0``. If the VM relies on
TPM-sealed keys or other virtual TPM state, preserve that state separately;
this plugin's backup alone is not sufficient to recover it.

The plugin uses QEMU dirty bitmaps to track changes for incremental backups.
It requests persistent bitmaps, which QEMU stores in qcow2 images, and falls
back to transient bitmaps when the disk format does not support persistence.
Raw disks, including typical LVM-backed VM disks, use transient bitmaps. These
include VM disks on Proxmox ``zfspool`` storage (raw ZFS zvols) and standard
``rbd`` storage (raw Ceph RBD images). These bitmaps exist only in the QEMU
process and are lost when that process exits. The storage backend alone does
not determine bitmap persistence: for example, a qcow2 file on a
ZFS-backed directory storage can support persistent bitmaps. An in-guest
reboot that leaves QEMU running does not by itself lose a transient bitmap;
stopping and restarting the VM, or otherwise restarting its QEMU process,
does.

If an Incremental backup finds a bitmap missing or inconsistent, the plugin
recreates it and reads that disk in full during the Incremental job. Other
disks can still be backed up incrementally. This establishes a new bitmap
baseline for the affected disk. The job log warns when a disk is backed up in
full this way. If you suspect an earlier Incremental was created after bitmap
loss by a plugin version that did not do this full read, run a new Full backup
before relying on that backup chain. After migration or storage rollback,
verify that the bitmaps remain present and consistent. Keep the Full backup and
all subsequent Incremental backups needed for a restore.
See the `QEMU dirty bitmap documentation
<https://www.qemu.org/docs/master/interop/bitmaps.html>`_ for details on
persistent and transient bitmaps.

To inspect bitmap state, run ``bareos-qmp-tool.py`` on the Proxmox node as
``root`` while the VM is running and no backup is in progress. Replace ``100``
with the VM ID:

.. code-block:: sh

   bareos-qmp-tool.py bitmap-status --vmid 100

The tool automatically checks the disk nodes supported by the plugin. It
reports whether each ``bqmp-bitmap`` is present, consistent, recording, and
persistent. A missing or inconsistent bitmap makes the command return a
nonzero status; ``persistent: false`` is informational and means the bitmap
will be lost when the QEMU process exits. Use ``--socket`` for a non-default
QMP socket path, or ``--json`` for machine-readable output. If a bitmap was
lost when QEMU restarted, the next Incremental reads that disk in full and
creates a new bitmap baseline.

Requirements
^^^^^^^^^^^^

Install the |fd| and plugin on the |proxmox ve| node running the VM.
The VM must be running during backup, and the |fd| must be able to access
its QMP socket and Proxmox configuration. The plugin uses ``qm``,
``qemu-img`` and ``qemu-storage-daemon`` on the node.

For restore destinations, the plugin supports file-backed storage with
``raw`` or ``qcow2`` images, LVM/LVM-thin, ZFS zvols, and Ceph RBD using
QEMU's librbd driver. Ceph RBD storage must have ``krbd`` disabled. The QSD
process uses the Proxmox Ceph configuration and per-storage credentials, so
ensure its RBD driver is available and the |fd| can access the configuration
and keyring files and reach the Ceph monitors.

The plugin creates temporary QCOW2 snapshot files in ``snapshotpath``.
The directory must exist, be writable by the |fd| and have enough free space
for changes made to the VM disks during a backup.
The plugin also uses local NBD connections on ports starting at 10809.

If the QEMU guest agent is available, the plugin requests a filesystem freeze
when creating the backup snapshot and then requests a thaw.
Without a guest agent, it does not quiesce the guest filesystems.
After a failed backup, check that the guest filesystems are not left frozen.

Installation
^^^^^^^^^^^^

The package is currently built for Proxmox VE 9.x (Debian 13).
Add the matching Bareos Debian 13 repository.

.. warning::

   If installation reports that ``python3-qemu-qmp`` is not installable,
   Debian 13 backports may not be enabled. For example:

   .. code-block:: text

      Error: Unable to correct problems, you have held broken packages.
      Error: The following information from --solver 3.0 may provide additional context:
         Unable to satisfy dependencies. Reached two conflicting decisions:
         1. bareos-filedaemon-proxmox-qmp-python-plugin:amd64=26.0.0~pre1643.bb96f906a-4 is selected for install
         2. bareos-filedaemon-proxmox-qmp-python-plugin:amd64 Depends python3-qemu-qmp
            but none of the choices are installable:
            [no choices]

   Enable Debian 13 backports as described below, then update the package
   lists and retry installation. The plugin also requires ``python3-libnbd``,
   which is available from the standard Debian 13 repository.

The plugin requires
``python3-qemu-qmp`` from Debian 13 backports. If backports is not already
configured, add the following line to a new file such as
:file:`/etc/apt/sources.list.d/trixie-backports.list`:

.. code-block:: text

   deb http://deb.debian.org/debian trixie-backports main

Then update the package lists and install the plugin on the Proxmox node:

.. code-block:: sh

   apt update
   apt install bareos-filedaemon-proxmox-qmp-python-plugin

The package also depends on ``python3-libnbd`` and the Bareos Python
File Daemon plugin. See the `Debian backports instructions
<https://backports.debian.org/Instructions/>`_ for enabling backports.

Configuration
^^^^^^^^^^^^^

Configure the |fd|
''''''''''''''''''

Run the |fd| as user and group ``root`` on the Proxmox node so that it can
access the Proxmox resources. The default group is usually ``bareos``.
Follow the shared instructions to
:ref:`switch the File Daemon runtime group to root <ProxmoxFileDaemonRuntimeGroup>`.
If the service is configured to run as a user other than ``root``, also add
``User=root`` to the systemd override.

.. warning::

   If the |fd| runs as user ``root`` but group ``bareos``, Proxmox can reject
   its cluster filesystem IPC connection with a message such as
   ``connection from bad user 0! - rejected``. Check both the service user
   and group, then apply the override above and restart the |fd| after any
   running jobs have finished.

Enable the Python plugin in the |fd| configuration:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = "/usr/lib/bareos/plugins"
     Plugin Names = "python3"
     ...
   }

Restart the |fd| after changing its configuration or service settings.
Connect the client to the Director as described in :ref:`section-AddAClient`.

Configure a job and fileset
'''''''''''''''''''''''''''

Configure one FileSet for the VM, identified by its Proxmox guest ID.
For example, to back up VM 100:

.. code-block:: bareosconfig
   :caption: Proxmox QMP job and FileSet

   Job {
     Name = "proxmox-qmp-vm"
     Client = "proxmox93-fd"
     FileSet = "ProxmoxQmpVM"
     JobDefs = "DefaultJob"
   }

   FileSet {
     Name = "ProxmoxQmpVM"
     Include {
       Options {
         Signature = XXH128
       }
       Plugin = "python"
                ":module_name=bareos-fd-proxmox-qmp"
                ":guestid=100"
     }
   }

Use the name of the configured Proxmox |fd| for ``Client``.
The FileSet plugin definition is separate from the one used by the
existing Proxmox Plugin.

Backup
^^^^^^

Run an initial Full backup, then run Incremental backups as needed:

.. code-block:: bconsole

   *run job=proxmox-qmp-vm level=Full yes
   *run job=proxmox-qmp-vm level=Incremental yes

The following example shows a successful Full backup that was automatically
upgraded from Incremental because no prior Full backup was found:

.. code-block:: bconsole
   :caption: Example Full backup job log

   *list joblog  jobid=1
   Using Catalog "MyCatalog"
   [...] bareos-dir JobId 1: No prior Full backup Job record found.
   [...] bareos-dir JobId 1: No prior or suitable Full backup found in catalog. Doing FULL backup.
   [...] bareos-dir JobId 1: Version: 26.0.0~pre1626.5bf9f13b1 (24 November 2026) AlmaLinux 10.3 (Mauve Lion)
   [...] bareos-dir JobId 1: Start Backup JobId 1, Job=proxmox-qmp-vm.2026-11-26_18.26.24_03
   [...] bareos-dir JobId 1: Connected Storage daemon at bareos-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1: Connected Client: proxmox93-fd at proxmox93:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1:  Handshake: Immediate TLS
   [...] bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 1: Using just in time reservation for job 1
   [...] bareos-dir JobId 1: Using Device "JustInTime Device" to write.
   [...] bareos-sd JobId 1: Connected File Daemon at proxmox93:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] proxmox93-fd JobId 1: Extended attribute support is enabled
   [...] bareos-sd JobId 1: Version: 26.0.0~pre1626.5bf9f13b1 (24 November 2026) AlmaLinux 10.3 (Mauve Lion)
   [...] proxmox93-fd JobId 1: ACL support is enabled
   [...] proxmox93-fd JobId 1: Version: 26.0.0~pre1643.bb96f906a (26 November 2026) Debian GNU/Linux 13 (trixie)
   [...] bareos-sd JobId 1: JustInTime Reservation: Finding drive to reserve.
   [...] bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
   [...] bareos-sd JobId 1: Using Device "FileStorage0001" (/var/lib/bareos/storage) to write.
   [...] bareos-sd JobId 1: Labeled new Volume "Full-0001" on device "FileStorage0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 1: Moving to end of data on volume "Full-0001"
   [...] bareos-sd JobId 1: Ready to append to end of Volume "Full-0001" size=225
   [...] proxmox93-fd JobId 1: python3-fd-mod: Executing /usr/lib/bareos/plugins/bareos-proxmox-qmp.py backup /var/run/qemu-server/100.qmp none scsi0 - 10810 /var/lib/bareos
   [...] proxmox93-fd JobId 1: python3-fd-mod: Backing up VM 100 disks with level F to virtual file @BQMP/100/disks-100-20261126_182629-F.bchunks
   [...] proxmox93-fd JobId 1: python3-fd-mod: waiting for command to finish
   [...] bareos-sd JobId 1: Releasing device "FileStorage0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 1: Elapsed time=00:00:48, Transfer rate=178.9 M Bytes/second
   [...] bareos-dir JobId 1: Insert of attributes batch table with 2 entries start
   [...] bareos-dir JobId 1: Insert of attributes batch table done
   [...] bareos-dir JobId 1: Bareos bareos-dir 26.0.0~pre1626.5bf9f13b1 (24Nov26):
     OS Information:         AlmaLinux 10.3 (Mauve Lion)
     JobId:                  1
     Job:                    proxmox-qmp-vm.2026-11-26_18.26.24_03
     Backup Level:           Full (upgraded from Incremental)
     Client:                 "proxmox93-fd" 26.0.0~pre1643.bb96f906a (26Nov26) Debian GNU/Linux 13 (trixie),debian
     FileSet:                "ProxmoxQmpVM" 2026-11-26 18:26:24
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         26-Nov-2026 18:26:24
     Start time:             26-Nov-2026 18:26:26
     End time:               26-Nov-2026 18:27:16
     Elapsed time:           50 secs
     Priority:               10
     Allow Mixed Priority:   yes
     FD Files Written:       3
     SD Files Written:       3
     FD Bytes Read:          8,589,941,249 (8.589 GB)
     FD Bytes Written:       8,589,941,249 (8.589 GB)
     SD Bytes Written:       8,589,942,259 (8.589 GB)
     Rate:                   171798.8 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Full-0001
     Volume Session Id:      1
     Volume Session Time:    1795713972
     Last Volume Bytes:      8,590,631,186 (8.590 GB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK

The following Incremental backup uses the Full backup above:

.. code-block:: bconsole
   :caption: Example Incremental backup job log

   *list joblog  jobid=2
   [...] bareos-dir JobId 2: Version: 26.0.0~pre1626.5bf9f13b1 (24 November 2026) AlmaLinux 10.3 (Mauve Lion)
   [...] bareos-dir JobId 2: Start Backup JobId 2, Job=proxmox-qmp-vm.2026-11-26_18.27.48_04
   [...] bareos-dir JobId 2: Connected Storage daemon at bareos-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2: Connected Client: proxmox93-fd at proxmox93:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2:  Handshake: Immediate TLS
   [...] bareos-dir JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 2: Using just in time reservation for job 2
   [...] bareos-dir JobId 2: Using Device "JustInTime Device" to write.
   [...] bareos-sd JobId 2: Connected File Daemon at proxmox93:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] proxmox93-fd JobId 2: Extended attribute support is enabled
   [...] bareos-sd JobId 2: Version: 26.0.0~pre1626.5bf9f13b1 (24 November 2026) AlmaLinux 10.3 (Mauve Lion)
   [...] proxmox93-fd JobId 2: ACL support is enabled
   [...] proxmox93-fd JobId 2: Version: 26.0.0~pre1643.bb96f906a (26 November 2026) Debian GNU/Linux 13 (trixie)
   [...] bareos-sd JobId 2: JustInTime Reservation: Finding drive to reserve.
   [...] bareos-dir JobId 2: Created new Volume "Incremental-0002" in catalog.
   [...] bareos-sd JobId 2: Using Device "FileStorage0001" (/var/lib/bareos/storage) to write.
   [...] bareos-sd JobId 2: Labeled new Volume "Incremental-0002" on device "FileStorage0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 2: Moving to end of data on volume "Incremental-0002"
   [...] bareos-sd JobId 2: Ready to append to end of Volume "Incremental-0002" size=239
   [...] proxmox93-fd JobId 2: python3-fd-mod: Executing /usr/lib/bareos/plugins/bareos-proxmox-qmp.py incr /var/run/qemu-server/100.qmp none scsi0 - 10810 /var/lib/bareos
   [...] proxmox93-fd JobId 2: python3-fd-mod: Backing up VM 100 disks with level I to virtual file @BQMP/100/disks-100-20261126_182753-I.bchunks
   [...] proxmox93-fd JobId 2: python3-fd-mod: waiting for command to finish
   [...] bareos-sd JobId 2: Releasing device "FileStorage0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 2: Elapsed time=00:00:10, Transfer rate=58.30 M Bytes/second
   [...] bareos-dir JobId 2: Insert of attributes batch table with 1 entries start
   [...] bareos-dir JobId 2: Insert of attributes batch table done
   [...] bareos-dir JobId 2: Bareos bareos-dir 26.0.0~pre1626.5bf9f13b1 (24Nov26):
     OS Information:         AlmaLinux 10.3 (Mauve Lion)
     JobId:                  2
     Job:                    proxmox-qmp-vm.2026-11-26_18.27.48_04
     Backup Level:           Incremental, since=2026-11-26 18:26:26
     Client:                 "proxmox93-fd" 26.0.0~pre1643.bb96f906a (26Nov26) Debian GNU/Linux 13 (trixie),debian
     FileSet:                "ProxmoxQmpVM" 2026-11-26 18:26:24
     Pool:                   "Incremental" (From Job IncPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         26-Nov-2026 18:27:48
     Start time:             26-Nov-2026 18:27:50
     End time:               26-Nov-2026 18:28:02
     Elapsed time:           12 secs
     Priority:               10
     Allow Mixed Priority:   yes
     FD Files Written:       2
     SD Files Written:       2
     FD Bytes Read:          583,010,464 (583.0 MB)
     FD Bytes Written:       583,010,464 (583.0 MB)
     SD Bytes Written:       583,011,316 (583.0 MB)
     Rate:                   48584.2 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Incremental-0002
     Volume Session Id:      2
     Volume Session Time:    1795713972
     Last Volume Bytes:      583,058,787 (583.0 MB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK

Configure a schedule appropriate for the retention of the Full and
Incremental backup chain. If an Incremental job reports that the VM
configuration changed, run a new Full backup.

Restore
^^^^^^^

Select the VM configuration and all disk data from the Full backup and the
required Incremental backups. In the interactive file selection, mark all
files in the selected backup chain, including the VM configuration and
every Full and Incremental disk file. For example:

.. code-block:: bconsole
   :caption: Select and mark the complete VM backup

   *<input>restore client=proxmox93-fd</input>
   [...]
    5: Select the most recent backup for a client
   [...]
   Select item:  (1-13): <input>5</input>
   [...]
   You have selected the following JobIds: 1,2
   [...]
   Building directory tree for JobId(s) 1,2 ...
   3 files inserted into the tree.
   [...]
   cwd is: /
   $ <input>mark *</input>
   4 files newly marked.
   $ <input>lsmark</input>
   *@BQMP/
     *100/
       *100.conf
       *disks-100-20261126_182629-F.bchunks
       *disks-100-20261126_182753-I.bchunks

After checking the marked files, enter ``done`` to leave file selection and
continue with the restore dialog.

Restore jobs can either restore to a VM on the Proxmox node or write raw disk
images and the VM configuration to a directory. The |fd| performing the restore
must have the QMP plugin installed.

Restore to a VM
'''''''''''''''

By default, the plugin restores to the original guest ID. It creates a VM and
its disks if that ID does not exist. To use another guest ID, provide it as a
restore plugin option, for example:

.. code-block:: bareosconfig

   python:guestid=999

Set plugin options in the restore dialog or with ``pluginoptions`` in
|bconsole|. Use a target node whose storage configuration can access or
create the VM disks. Direct restore currently resolves disk paths using
``path`` or ``vgname`` entries in :file:`/etc/pve/storage.cfg`; not all
Proxmox storage backends and disk formats are supported.

.. warning::

   If the target guest ID already exists, the plugin stops the VM if it is
   running and writes to disk paths from the backed-up VM configuration,
   which may not be the target VM's current disks. This happens without
   requiring ``force=yes``. Check the target and disk paths carefully before
   restoring; other VMs' data may also be at risk.

The following example restores the Full and Incremental backups above to
their original guest ID, 100:

.. code-block:: bconsole
   :caption: Example restore to the original guest ID

   *list joblog  jobid=3
   [...] bareos-dir JobId 3: Version: 26.0.0~pre1626.5bf9f13b1 (24 November 2026) AlmaLinux 10.3 (Mauve Lion)
   [...] bareos-dir JobId 3: Start Restore Job RestoreFiles.2026-11-26_19.01.03_13
   [...] bareos-dir JobId 3: Connected Storage daemon at bareos-server.bareos:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 3: Using Device "FileStorage0001" to read.
   [...] bareos-dir JobId 3: Connected Client: proxmox93-fd at proxmox93:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 3:  Handshake: Immediate TLS
   [...] bareos-dir JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 3: Connected File Daemon at proxmox93:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 3: Version: 26.0.0~pre1626.5bf9f13b1 (24 November 2026) AlmaLinux 10.3 (Mauve Lion)
   [...] bareos-sd JobId 3: Ready to read from volume "Full-0001" on device "FileStorage0001" (/var/lib/bareos/storage).
   [...] proxmox93-fd JobId 3: Version: 26.0.0~pre1643.bb96f906a (26 November 2026) Debian GNU/Linux 13 (trixie)
   [...] bareos-sd JobId 3: Forward spacing Volume "Full-0001" to file:block 0:225.
   [...] proxmox93-fd JobId 3: python3-fd-mod: Restoring 100.conf
   [...] proxmox93-fd JobId 3: python3-fd-mod: Restoring disks-100-20261126_182629-F.bchunks
   [...] proxmox93-fd JobId 3: python3-fd-mod: vm 100 does not exist, creating it
   [...] proxmox93-fd JobId 3: python3-fd-mod: Started qemu-storage-daemon at NBD port 10813 pid 2131
   [...] proxmox93-fd JobId 3: python3-fd-mod: Executing /usr/lib/bareos/plugins/bareos-proxmox-qmp.py restore /var/run/qemu-server/100.qmp none scsi0:8589934592 - 10813 /var/lib/bareos
   [...] bareos-sd JobId 3: End of Volume at file 2 on device "FileStorage0001" (/var/lib/bareos/storage), Volume "Full-0001"
   [...] bareos-sd JobId 3: Ready to read from volume "Incremental-0002" on device "FileStorage0001" (/var/lib/bareos/storage).
   [...] bareos-sd JobId 3: Forward spacing Volume "Incremental-0002" to file:block 0:239.
   [...] proxmox93-fd JobId 3: python3-fd-mod: Restoring disks-100-20261126_182753-I.bchunks
   [...] bareos-sd JobId 3: End of Volume at file 0 on device "FileStorage0001" (/var/lib/bareos/storage), Volume "Incremental-0002"
   [...] bareos-sd JobId 3: End of all volumes.
   [...] bareos-sd JobId 3: Releasing device "FileStorage0001" (/var/lib/bareos/storage).
   [...] proxmox93-fd JobId 3: python3-fd-mod: waiting for command to finish
   [...] bareos-dir JobId 3: Bareos bareos-dir 26.0.0~pre1626.5bf9f13b1 (24Nov26):
     OS Information:         AlmaLinux 10.3 (Mauve Lion)
     JobId:                  3
     Job:                    RestoreFiles.2026-11-26_19.01.03_13
     Restore Client:         "proxmox93-fd" 26.0.0~pre1643.bb96f906a (26Nov26) Debian GNU/Linux 13 (trixie),debian
     Start time:             26-Nov-2026 19:01:05
     End time:               26-Nov-2026 19:01:55
     Elapsed time:           50 secs
     Files Expected:         3
     Files Restored:         3
     Bytes Restored:         9,172,951,713
     Rate:                   183459.0 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos pre-release (UNSUPPORTED): Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Restore OK

Restore to disk images
''''''''''''''''''''''

Set ``restoretodisk=yes`` to restore the configuration and raw disk images
instead of writing to a VM. Set ``restorepath`` to an existing writable
directory with enough space for the restored disks. The plugin reports an error
if the directory does not exist; it does not create it. For example, create
the directory on the Proxmox node:

.. code-block:: sh

   mkdir -p /var/lib/vz/dump/bareos-restores

Then set the restore plugin options:

.. code-block:: bareosconfig

   python:restoretodisk=yes:restorepath=/var/lib/vz/dump/bareos-restores

The plugin writes the VM configuration as ``<guestid>.conf`` and each disk
as ``<device>.raw``, for example ``scsi0.raw``. These are raw images, not a
Proxmox ``.vma`` archive. ``restorepath`` controls the image destination;
the Bareos restore ``where`` setting does not replace it.

.. warning::

   Existing files with the same names in ``restorepath`` are overwritten.
   Use an empty directory or ensure that its existing files can be replaced.

.. warning::

   Image restores currently ignore zeroed ranges in Incremental backups.
   Restoring an Incremental chain to disk images can therefore leave stale
   data in the images; do not rely on it for recovery.

Plugin options
^^^^^^^^^^^^^^

Options are separated by colons. Options supplied at restore time override
those saved with the backup.

``guestid``
   Mandatory integer ID of the VM to back up. On restore, it selects the
   target guest ID.

``snapshotpath``
   Directory for temporary snapshot files during backup. The default is
   :file:`/var/lib/bareos`.

``restoretodisk``
   Restore to raw disk images instead of a VM. The default is ``no``.

``restorepath``
   Destination directory for image restores. The default is
   :file:`/var/lib/vz/dump`.

``force``
   Accepted as a boolean option, but currently does not control whether an
   existing VM or image file is overwritten. See the restore warnings above.

Boolean options accept ``yes``, ``on``, ``true`` or ``1`` for true, and
``no``, ``off``, ``false`` or ``0`` for false.
