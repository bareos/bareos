.. _IncusPlugin:

Incus Plugin
~~~~~~~~~~~~~~

.. index::
   single: Plugin; Incus
   single: Incus Plugin

The |incus| Plugin allows to backup |incus| instances. Is can perform full and incremental backups of
both VM (acting at the block level) and container (acting at the file level) instances. Restores can
be done either to a new |incus| instance or to a local tarball ready for import to |incus|.

.. limitation:: Partial restores are not supported

   As VM backups happen at the block level, they cannot be partially restored. Restoring individual
   files to a container is not currently supported, but is planned to be in a future version of the
   plugin.


Requirements
^^^^^^^^^^^^

The |incus| Plugin (together with the |fd|) needs to be installed on a machine with an |incus|
client installed (not necessarily an |incus| server). The plugin requires at least Python 3.13; for
a Debian installation, it corresponds to at least Debian 13 (Trixie).

Installation
^^^^^^^^^^^^

Install the package **bareos-filedaemon-incus-plugin** (using :command:`apt`) on your machine. This
will automatically also install the |fd| and other required packages.

Configuration
^^^^^^^^^^^^^


Configure the |fd| to load the Python plugin
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
After connecting the client to the director as described in :ref:`section-AddAClient`,
we go on with configuring jobs and filesets.


Configure jobs and filesets
'''''''''''''''''''''''''''

To define the backup of an |incus| instance in Bareos, a job definition and a fileset resource
must be added to the Bareos director configuration.
The following example shows how to configure the backup of the instance named ``my-instance``:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Incus Plugin Job and FileSet definitions

   Job {
     Name = "incus-instance"
     Client = "incus-fd"
     FileSet = "IncusTestInstance"
     JobDefs = "DefaultJob"
     Level = Full
     Accurate = yes
   }

   FileSet {
     Name = "IncusTestInstance"
     Description = "Backup an Incus intance"
     Include {
       Options {
         Signature = XXH128
         Accurate = ipnugsamc
       }
       Plugin = "python"
                ":module_name=bareos-fd-incus"
                ":instance=my-instance"
                ":compression=lzma"
     }
   }


Note that for incremental backups to work, :config:option:`dir/job/accurate` needs to be set to
``yes``. Performing an incremental backup over a full backup that didn’t define this option is not
recommended. We also highly recommend setting :config:option:`dir/fileset/include/options/accurate`
to ``ipnugsamc``. For more details about this, please look at the plugin options below.


Backup
^^^^^^

To run a backup of an |incus| instance, simply start the corresponding job ``incus-instance``:

.. code-block:: bconsole
   :caption: Example backup of an Incus instance with the Incus plugin

   *<input>run job=incus-instance level=Full yes</input>

   Using Catalog "MyCatalog"
   Job queued. JobId=1
   *You have messages.
   *messages
   [...] bareos-dir JobId 1: Version: 26.0.0~pre28.b49f19339 (15 December 2025) Debian GNU/Linux 13 (trixie)
   [...] bareos-dir JobId 1: Start Backup JobId 1, Job=backup-incus-vm.2026-01-23_14.22.01_03
   [...] bareos-dir JobId 1: Connected Storage daemon at bareos:30473, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1: Connected Client: bareos-fd at bareos:30472, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 1:  Handshake: Immediate TLS
   [...] bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 1: Using just in time reservation for job 1
   [...] bareos-dir JobId 1: Using Device "JustInTime Device" to write.
   [...] py3plug-fd-incus-fd JobId 1: Connected Storage daemon at bareos:30473, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] py3plug-fd-incus-fd JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] py3plug-fd-incus-fd JobId 1: Extended attribute support is enabled
   [...] py3plug-fd-incus-fd JobId 1: ACL support is enabled
   [...] py3plug-fd-incus-fd JobId 1: python3-fd-mod: Executing incus export my-instance - --instance-only --quiet --compression lzma
   [...] bareos-sd JobId 1: Version: 26.0.0~pre28.b49f19339 (15 December 2025) Debian GNU/Linux 13 (trixie)
   [...] py3plug-fd-incus-fd JobId 1: Version: 26.0.0~pre28.b49f19339 (15 December 2025) Debian GNU/Linux 13 (trixie)
   [...] bareos-sd JobId 1: JustInTime Reservation: Finding drive to reserve.
   [...] bareos-sd JobId 1: Using Device "FileStorage" (storage) to write.
   [...] bareos-sd JobId 1: Moving to end of data on volume "Full-0001"
   [...] bareos-sd JobId 1: Ready to append to end of Volume "Full-0001" size=213
   [...] bareos-sd JobId 1: Releasing device "FileStorage" (storage).
   [...] bareos-sd JobId 1: Elapsed time=00:00:43, Transfer rate=7.804 M Bytes/second
   [...] bareos-dir JobId 1: Insert of attributes batch table with 59 entries start
   [...] bareos-dir JobId 1: Insert of attributes batch table done
   [...] bareos-dir JobId 1: Bareos bareos-dir 26.0.0~pre28.b49f19339 (15Dec25):
     OS Information:         Debian GNU/Linux 13 (trixie)
     JobId:                  1
     Job:                    backup-incus-vm.2026-01-23_14.22.01_03
     Backup Level:           Full
     Client:                 "bareos-fd" 26.0.0~pre28.b49f19339 (15Dec25) Debian GNU/Linux 13 (trixie),debian
     FileSet:                "IncusTestInstance" 2026-01-23 14:22:01
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         23-Jan-2026 14:22:01
     Start time:             23-Jan-2026 14:22:03
     End time:               23-Jan-2026 14:22:46
     Elapsed time:           43 secs
     Priority:               10
     Allow Mixed Priority:   no
     FD Files Written:       59
     SD Files Written:       59
     FD Bytes Read:          335,561,581 (335.5 MB)
     FD Bytes Written:       335,561,581 (335.5 MB)
     SD Bytes Written:       335,581,420 (335.5 MB)
     Rate:                   7803.8 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               yes
     Volume name(s):         Full-0001
     Volume Session Id:      1
     Volume Session Time:    1769178121
     Last Volume Bytes:      335,612,023 (335.6 MB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Self-compiled: Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Backup OK


Virtual machine backups cut the root disk into small chunks. When performing incremental backups,
only the chunks that have changed will be saved.


Restore
^^^^^^^

The next step is to restore the instance. currently, only total restores are supported. Therefore,
run the restore command and mark all the files available.

Restoring to an |incus| server that already has an instance with the same name as the one you are
restoring will cause an error, so you either need to move or delete the existing one or to restore
to another instance name or server.

.. code-block:: bconsole
   :caption: Example restore of an Incus instance with the Incus plugin

   *restore client=bareos-fd fileset=IncusTestInstance where=/ select all done
   *messages
   [...] bareos-dir JobId 2: Version: 26.0.0~pre28.b49f19339 (15 December 2025) Debian GNU/Linux 13 (trixie)
   [...] bareos-dir JobId 2: Start Restore Job RestoreFiles.2026-01-23_14.55.50_06
   [...] bareos-dir JobId 2: Connected Storage daemon at bareos:30473, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2: Using Device "FileStorage" to read.
   [...] bareos-dir JobId 2: Connected Client: bareos-fd at bareos:30472, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-dir JobId 2:  Handshake: Immediate TLS
   [...] bareos-dir JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] py3plug-fd-incus-fd JobId 2: Connected Storage daemon at bareos:30473, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 2: Version: 26.0.0~pre28.b49f19339 (15 December 2025) Debian GNU/Linux 13 (trixie)
   [...] py3plug-fd-incus-fd JobId 2:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   [...] bareos-sd JobId 2: Ready to read from volume "Full-0001" on device "FileStorage" (storage).
   [...] py3plug-fd-incus-fd JobId 2: Version: 26.0.0~pre28.b49f19339 (15 December 2025) Debian GNU/Linux 13 (trixie)
   [...] py3plug-fd-incus-fd JobId 2: python3-fd-mod: Executing incus import - my-instance
   [...] bareos-sd JobId 2: Forward spacing Volume "Full-0001" to file:block 0:213.
   [...] bareos-sd JobId 2: End of Volume at file 0 on device "FileStorage" (storage), Volume "Full-0001"
   [...] bareos-sd JobId 2: End of all volumes.
   [...] bareos-sd JobId 2: Releasing device "FileStorage" (storage).
   [...] bareos-dir JobId 2: Bareos bareos-dir 26.0.0~pre28.b49f19339 (15Dec25):
     OS Information:         Debian GNU/Linux 13 (trixie)
     JobId:                  2
     Job:                    RestoreFiles.2026-01-23_14.55.50_06
     Restore Client:         "bareos-fd" 26.0.0~pre28.b49f19339 (15Dec25) Debian GNU/Linux 13 (trixie),debian
     Start time:             23-Jan-2026 14:55:52
     End time:               23-Jan-2026 14:56:33
     Elapsed time:           41 secs
     Files Expected:         59
     Files Restored:         59
     Bytes Restored:         335,561,577
     Rate:                   8184.4 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Self-compiled: Get professional support from https://www.bareos.com
     Job triggered by:       User
     Termination:            Restore OK


The instance has been recovered and can be now be started as usual.


Restore to a different instance
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To restore to a different instance name, the new name needs to be specified in either the
``instance`` or the ``restore_instance` parameters in the plugin options string during restore:

.. code-block:: bareosconfig
   :caption: Changing the instance name during restore
 
   python:restore_instance=<new name>


Restoring to another remote can be similarly done using either ``remote`` or ``restore_remote``.

Note that if your workflow often restores instances to a specific name or to a specific remote,
you can set the ``restore_instance`` or the ``restore_remote`` parameters in
:config:option:`dir/fileset/plugin`.


Restore to an Incus-ready tarball
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Setting a ``restore_path`` parameter to a valid path within your filesystem will alter the restore
logic and write a tarball importable into |incus| directly to disk:

.. code-block:: bareosconfig
   :caption: Restore the backup into a local tarball

   python:restore_path=/tmp/restore.tgz

Importing the tarball to |incus| is simply a matter of:

.. code-block:: sh
   :caption: Restore a tarball to Incus

   root@incus:~#<input>incus import /tmp/restore.tgz</input>

            'restore_storage': self.make_opt(_str, ''),
            'temp_dir': self.make_opt(_str, ''),

Description of all Plugin Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Note that all plugin options that have been used at backup time are passed back on restore. Options
given on restore will override the values originating from the backup.


instance (mandatory on backup)
   The name of the instance to be backed up. On restore it is possible to override this option in
   order to recreate the instance with a different name.

project (optional)
   The |incus| project from which to fetch the instance. Defaults to the default configured project
   on the |fd|’s |incus| client.

remote (optional)
   The |incus| remote from which to fetch the instance. Defaults to the default configured remote on
   the |fd|’s |incus| client.

allow_disk_resize (optional)
   This option must stay set to ``no`` (or unset). If this gains enough interest, this option could
   allow in the future to resize virtual machines without breaking the incremental mechanisms we
   have in place. Currently, resizing a virtual machine and running an incremental backup job will
   backup again all the disk chunks, essentially performing a full backup.

chunk_size (optional)
   The size of virtual machine disk chunks. Needs to be a multiple of ``256kiB``. Defaults to
   ``64MiB``.

chunk_id_length (optional)
   The number of digits used to store chunk identifiers. You should not need to change this value.
   Defaults to ``8``.

queue_depth (optional)
   The maximum number of chunks to buffer to RAM during backup. This should be properly sized so it
   doesn’t fill up all your available memory. Expect the memory usage of the backup buffer to be
   around ``queue_depth * chunk_size``. Defaults to ``8``.

compression (optional)
   The compression algorithm to use with |incus| when streaming tarballs. This should only matter if
   you are using a remote |incus| server. ``zstd`` is only supported on Python 3.14+. Defaults to
   ``none``.

backup_poll_timeout (optional)
   The delay after which, if there is no data in our processing queue, we should stop the export.
   Currently, |incus| doesn’t directly stream its backup tarballs, but rather dumps them to disk
   before exporting them; this leads to a potentially significant latency between the export request
   and the actual export stream. Defaults to ``1h`` (will be decreased to ``1m`` in the future).

hash (optional)
   The hash algorithm to use to detect changes in each chunk. Digests are stored in the chunk’s
   metadata as described by the ``hijacked_stat_fields`` parameter. You should use a hash algorithm
   that fits in your hijacked fields (64 bits per field). Defaults to ``sha256``.

hijacked_stat_fields (optional)
   A comma-separated list of stat fields (among ``st_ino``, ``st_atime``, ``st_mtime`` and
   ``st_ctime``) to use to store chunk digests. To compare these fields before sending any data to
   the Bareos core, :config:option:`dir/job/accurate` needs to be set to ``yes``. This is not enough
   for the core to compare the fields we need, so you should set
   :config:option:`dir/fileset/include/options/accurate` to ``ipnugsamc`` (or, if you understand
   what you are doing, to at least ``ips`` + the fields used in ``hijacked_stat_fields``). Defaults
   to ``st_ino,st_atime,st_mtime,st_ctime``

buffer_depth (optional)
   The maximum number of chunks to buffer to RAM during restore before writing temporary data to
   disk. This should be properly sized so it doesn’t fill up all your available memory. Expect the
   memory usage of the restore buffer to be around ``(buffer_depth + 2) * chunk_size``. Defaults to
   ``8``.

restore_instance (optional)
   The name of the instance in which the restore will be done. Defaults to using the instance
   defined in ``instance``.

restore_path (optional)
   The path on disk to which the tarball will be restored. This option is incompatible with
   ``restore_instance``, ``restore_project`` and ``restore_remote``. Defaults to not restoring to
   disk.

restore_project (optional)
   The name of the project in which the restore will be done. Defaults to using the project defined
   in ``project``.

restore_remote (optional)
   The name of the remote in which the restore will be done. Defaults to using the remote defined in
   ``remote``.

restore_storage (optional)
   The name of the storage pool in which the restore will be done. Defaults to the name of the pool
   from which the instance originates.

temp_dir (optional)
   The temporary directory used on restore in which to write temporary image chunks. Defaults to
   using a platform-dependent temporary directory chosen by Python.

.. warning::

   Colons (**:**) need to be escaped with a backslash (**\\**) when passing them as plugin
   parameters. This is required because the colon is the delimiter between plugin parameters.
