.. _oVirtPlugin:

oVirt Plugin
~~~~~~~~~~~~

.. index::
   pair: Plugin; oVirt

The oVirt Plugin can be used for agentless backups of virtual machines running on oVirt or Red Hat Virtualization (RHV).
It was tested with oVirt/RHV 4.4. There are currently no known technical differences between
RHV and oVirt (which is RHV's upstream project) that are relevant for this plugin, so both
names are equivalent in this documentation if not explicitly mentioned.

For backing up a VM, the plugin performs the following steps:

* Retrieve the VM configuration data from the oVirt API as OVF XML data
* Take a snapshot of the VM
* Retrieve the VM disk image data of the snapshot via oVirt Image I/O
* Remove the snapshot

When using include/exclude options, the snapshot will only contain the resulting disks.

It is included in Bareos since :sinceVersion:`19: oVirt Plugin`.

.. _oVirtPlugin-status:

Status of oVirt Plugin
^^^^^^^^^^^^^^^^^^^^^^

The Plugin can currently only take full backups of VM disks.

In oVirt 4.4 the incremental backup feature was added, in RHV 4.4 this
is still declared as a technology preview feature. The Bareos oVirt
plugin does not yet support this new feature.

When performing restores, the plugin can do one of the following:

* Write local disk image files
* Create a new VM with new disks
* Overwrite existing disks of an existing VM

Additionally it is possible to

* Skip disks by alias names using include/exclude
* Restoring the VM only without any disks is possible by

  * Selecting to restore only the **.ovf** file
  * Excluding all disks by alias

Currently, the access to disk images is implemented only via the oVirt Image I/O Proxy component
of the engine server.

Since :sinceVersion:`20: oVirt Plugin` the plugin works with both Python version 2 or 3 and it
was adapted to the modernized Bareos Python plugin API.

.. _oVirtPlugin-requirements:

Requirements of oVirt Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The plugin is currently only available for Red Hat Enterprise Linux and CentOS 7 and 8. It requires the
Python oVirt Engine SDK version 4, Red Hat Subscriptions customers can find the package
**python-ovirt-engine-sdk4** in the ``rh-common`` repo for RHEL 7, which may not be enabled by default.
For RHEL 8 the package **python3-ovirt-engine-sdk4** can be found in the ``rhv-4-tools-for-rhel-8-x86_64-rpms``
repo.
The oVirt project provides the package at https://resources.ovirt.org/pub/ovirt-4.3/rpm/el7/x86_64/
or https://resources.ovirt.org/pub/ovirt-4.4/rpm/el8/x86_64/.

As mentioned above, since :sinceVersion:`20: oVirt Plugin` the plugin works with Python version 2 or 3,
however, it is recommended to use Python 3 because Python 2 has reached it's end of life.
To allow the user more flexibility for the installation of the Python oVirt Engine SDK, the
dependency on the **python-ovirt-engine-sdk4** has been removed in the Bareos package.
Instead, the plugin code now creates an appropriate job error messages when the SDK is not
installed. The user can now choose to either install the **python3-ovirt-engine-sdk4** package
from the oVirt project or Red Hat repo, or install the SDK by using

.. code-block:: shell

   pip3 install ovirt-engine-sdk-python

Use ``pip`` or ``pip2`` for the Python 2 version.

The system running the |fd| with this plugin must have network access to the oVirt/RHV
engine server on the TCP ports 443 (https for API access) and 54323 (for Image I/O Proxy access).

The QEMU Guest Agent (QEMU GA) should be installed inside VMs to optimize the consistency
of snapshots by filesystem flushing and quiescing. This also allows custom freeze/thaw hook
scripts in Linux VMs to ensure application level consistency of snapshots. On Windows the
QEMU GA provides VSS support thus live snapshots attempt to quiesce whenever possible.

.. _oVirtPlugin-installation:

Installation of oVirt Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The installation is done by installing the package **bareos-filedaemon-ovirt-python-plugin**:

.. code-block:: shell

   yum install bareos-filedaemon-ovirt-python-plugin


.. _oVirtPlugin-configuration:

Configuration of oVirt Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As the Plugin needs access to the oVirt API, an account with appropriate privileges must be used.
The default **admin@internal** user works, as it has all privileges. Using an account with
less privileges should be possible, the plugin needs to be able to do the following:

* Read VM metadata
* Read, create and write disk images via Image I/O Proxy
* Create VMs

The exact required oVirt roles are beyond the scope of this document.

To verify SSL certificates, the plugin must know the CA certificate of the oVirt environment,
it can be downloaded from the oVirt/RHV engine start page manually, or by using the following
command:

.. code-block:: shell

   curl -k -o /etc/bareos/ovirt-ca.cert https://engine.example.com/ovirt-engine/services/pki-resource?resource=ca-certificate&format=X509-PEM-CA

For each VM to be backed up, a **job** and a **fileset** must be configured. For
example to backup the VM **testvm1**, configure the fileset as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/fileset/testvm1_fileset.conf

   FileSet {
      Name = "testvm1_fileset"

      Include {
         Options {
            Signature = XXH128
            Compression = LZ4
         }
         Plugin = "python"
                  ":module_name=bareos-fd-ovirt"
                  ":ca=/etc/bareos/ovirt-ca.cert"
                  ":server=engine.example.com"
                  ":username=admin@internal"
                  ":password=secret"
                  ":vm_name=testvm1"
      }
   }

.. note::

   The Plugin options string can currently not be split over multiple lines in the configuration file.

And the job as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/job/testvm1_job.conf

   Job {
      Name = "testvm1_job"
      JobDefs = "DefaultJob"
      FileSet = "testvm1_fileset"
   }

Optionally, it is possible to use a configuration file on the system running the |fd| for storing the credentials instead of using the plugin options **username** and **password**. Use the plugin option **config_file** to specify the config file name as in the following example:

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/fileset/testvm1_fileset.conf

   FileSet {
      Name = "testvm1_fileset"

      Include {
         Options {
            Signature = XXH128
            Compression = LZ4
         }
         Plugin = "python"
                  ":module_name=bareos-fd-ovirt"
                  ":ca=/etc/bareos/ovirt-ca.cert"
                  ":server=engine.example.com"
                  ":config_file=/etc/bareos/ovirt-plugin.ini"
                  ":vm_name=testvm1"
      }
   }

And the config file as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/ovirt-plugin.ini

   [credentials]
   username = admin@internal
   password = secret

.. note::

   Do not use quotes in the above config file, it is processed by the Python ConfigParser module and the quotes would not be stripped from the string.

Currently the config file can only be used for credentials. If **username** and **password** are also present in the plugin options, the credentials from the config file will override them. In this case, the job log will contain a warning.

Mandatory Plugin Options:

module_name
   Always :file:`bareos-fd-ovirt`

ca
   Path to the oVirt/RHV SSL CA File, the CA File must be downloaded as described above

server
   The FQDN of the oVirt/RHV engine server

username
   The username of an account which has appropriate privileges

password
   The password for the user that is configured with **username**

vm_name
   The name of the VM to be backed up

storage_domain
   The target storage domain name (only for restore)

Optional Plugin Options:

module_path
   Additional search path for the plugin.

uuid
   Instead of specifying the VM to be backed up by name (using option **vm_name**), the VM
   can be specified by its uuid.

include_disk_aliases
   Comma separated list of disk alias names to be included only. If not specified, all disks
   that are attached to the VM are included. Can be used on backup and restore.

exclude_disk_aliases
   Comma separated list of disk alias names to be excluded, if not specified, no disk will
   be excluded. Using ``exclude_disk_aliases=*`` would exclude all disks. Can be used on
   backup and restore. Note that the **include_disk_aliases** options
   is applied first, then **exclude_disk_aliases**, so using both usually makes no sense.
   Also note that disk alias names are not unique, so if two disks of a VM have the same
   alias name, they will be excluded both. Excluded disks will be already excluded from
   the snapshot.
   On *local* restore, both **include_disk_aliases** and **exclude_disk_aliases** are ignored
   and *all* disk that were backed up will be restored.

overwrite
   When restoring disks of an existing VM, the option **overwrite=yes** must be explicitly
   passed to force overwriting. To prevent from accidentally overwriting an existing VM,
   the plugin will return an error message if this option is not passed.

cluster_name
   When restoring, the target cluster name can be specified. Otherwise the default cluster
   will be used.

vm_template
   The VM template to be used when restoring to a new VM. If not specified, the default Blank
   template will be used.

vm_type
   When not using this option, the VM type *Server* will be used when restoring to a new VM. The VM Type
   can be set to *Desktop* or *High Performance* optionally by using **vm_type=desktop**
   or **vm_type=high_performance**.

vm_memory
   When not using this option, the amount of VM memory configured when restoring to a new VM will
   be taken from the VM metadata that have been saved on backup. Optionally, the amount of
   memory for the new VM can be specified in Megabytes here, for example by using
   **vm_memory=4** would create the new vm with 4 MB or RAM.

vm_cpu
   When not using this option, the number of virtual CPU cores/sockets/threads configured when restoring
   to a new VM will be taken from the VM metadata that have been saved on backup. Optionally, the
   amount of a cores/sockets/threads can be specified as a comma separated list
   **vm_cpu=<cores>,<sockets>,<threads>**.

ovirt_sdk_debug_log
   Only useful for debugging purposes, enables writing oVirt SDK debug log to the specified file, for
   example by adding **ovirt_sdk_debug_log=/var/log/bareos/ovirt-sdk-debug.log**.


.. _oVirtPlugin-backup:

Backup with oVirt Plugin
^^^^^^^^^^^^^^^^^^^^^^^^

To manually run a backup, use the following command in |bconsole|:

.. code-block:: bconsole
   :caption: Example: Running a oVirt Plugin backup job

   *<input>run job=testvm1_job level=Full</input>
   Using Catalog "MyCatalog"
   Run Backup job
   JobName:  testvm1_job
   Level:    Full
   Client:   bareos-fd
   Format:   Native
   FileSet:  testvm1_fileset
   Pool:     Full (From Job FullPool override)
   Storage:  File (From Job resource)
   When:     2019-12-16 17:41:13
   Priority: 10
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=1


.. note::

   As the oVirt/RHV API does not yet allow Incremental backups, the plugin will only
   allow to run full level backups to prevent from using the Incremental pool
   accidentally. Please make sure to configure a schedule that always runs
   full level backups for jobs using this plugin.


.. _oVirtPlugin-restore:

Restore with oVirt Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^

An example restore dialogue could look like this:

.. code-block:: bconsole
   :caption: Example: running a oVirt Plugin backup job

   *<input>restore</input>

   First you select one or more JobIds that contain files
   to be restored. You will be presented several methods
   of specifying the JobIds. Then you will be allowed to
   select which files from those JobIds are to be restored.

   To select the JobIds, you have the following choices:
        1: List last 20 Jobs run
        2: List Jobs where a given File is saved
        3: Enter list of comma separated JobIds to select
        4: Enter SQL list command
        5: Select the most recent backup for a client
        6: Select backup for a client before a specified time
        7: Enter a list of files to restore
        8: Enter a list of files to restore before a specified time
        9: Find the JobIds of the most recent backup for a client
       10: Find the JobIds for a backup for a client before a specified time
       11: Enter a list of directories to restore for found JobIds
       12: Select full restore to a specified Job date
       13: Cancel
   Select item:  (1-13): <input>5</input>
   Defined Clients:
        1: bareos1-fd
        2: bareos2-fd
        3: bareos3-fd
        4: bareos4-fd
        5: bareos-fd
   Select the Client (1-5): <input>5</input>
   Automatically selected FileSet: testvm1_fileset
   +-------+-------+----------+-------------+---------------------+------------+
   | jobid | level | jobfiles | jobbytes    | starttime           | volumename |
   +-------+-------+----------+-------------+---------------------+------------+
   |     1 | F     |        9 | 564,999,361 | 2019-12-16 17:41:26 | Full-0001  |
   +-------+-------+----------+-------------+---------------------+------------+
   You have selected the following JobId: 1

   Building directory tree for JobId(s) 1 ...
   5 files inserted into the tree.

   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.

   cwd is: /
   $ <input>mark *</input>
   5 files marked.
   $ <input>done</input>
   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.3.bsr

   The job will require the following
      Volume(s)                 Storage(s)                SD Device(s)
   ===========================================================================

       Full-0001                 File                      FileStorage

   Volumes marked with "*" are online.


   5 files selected to be restored.

   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.3.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         LinuxAll
   Backup Client:   bareos-fd
   Restore Client:  bareos-fd
   Format:          Native
   Storage:         File
   When:            2019-12-16 20:58:31
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
   Please enter Plugin Options string: python:storage_domain=hosted_storage:vm_name=testvm1restore
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.3.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         LinuxAll
   Backup Client:   bareos-fd
   Restore Client:  bareos-fd
   Format:          Native
   Storage:         File
   When:            2019-12-16 20:58:31
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  <input>python:storage_domain=hosted_storage:vm_name=testvm1restore</input>
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=2

By using the above Plugin Options, the new VM **testvm1restore** is created and the disks
are created in the storage domain **hosted_storage** with the same cpu and memory parameters
as the backed up VM.

When omitting the **vm_name** Parameter, the VM name will be taken from the backed up metadata
and the plugin will restore to the same VM if it still exists.


When restoring disks of an existing VM, the option **overwrite=yes** must be explictly
passed to force overwriting. To prevent from accidentally overwriting an existing VM,
the plugin will return an error message if this option is not passed.

.. _oVirtPlugin-restore-to-local-image:

Restore to local disk image
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Instead of restoring to an existing or new VM, it is possible to restore the disk image
as image files on the system running the Bareos FD. To perform such a restore, the
following Plugin Option must be entered:

.. code-block:: bconsole
   :caption: Example: running a oVirt Plugin backup job

   *<input>restore</input>

   First you select one or more JobIds that contain files
   to be restored. You will be presented several methods
   ...
   Plugin Options:  <input>python:local=yes</input>
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=2

Anything else from the restore dialogue is the same.

This will create disk image files that could be examined for example by using
the **guestfish** tool (see https://libguestfs.org/guestfish.1.html). This tool
can also be used to extract single files from the disk image.
