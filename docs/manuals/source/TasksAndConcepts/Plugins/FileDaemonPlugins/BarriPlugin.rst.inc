.. _BarriPlugin:

Barri Plugin
~~~~~~~~~~~~

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

TODO

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

To define the backup of a VM in Bareos, a job definition and a fileset resource must be added to the Bareos director configuration. In vCenter, VMs are usually organized in datacenters and folders. The following example shows how to configure the backup of the VM named *websrv1* in the datacenter *mydc1* folder *webservers* on the vCenter server :command:`vcenter.example.org`:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Barri Plugin Job and FileSet definition

   Job {
     Name = "vm-websrv1"
     JobDefs = "DefaultJob"
     FileSet = "vm-websrv1_fileset"
   }

   FileSet {
     Name = "vm-websrv1_fileset"

     Include {
       Options {
            Signature = XXH128
            Compression = LZ4
       }
       Plugin = "python"
                ":module_name=bareos-fd-vmware"
                ":dc=mydc1:folder=/webservers"
                ":vmname=websrv1"
                ":vcserver=vcenter.example.org"
                ":vcuser=bakadm@vsphere.local"
                ":vcpass=Bak.Adm-1234"
     }
   }


Backup
^^^^^^

Before running the first backup, CBT (Changed Block Tracking) must be enabled for the VMs to be backed up.

Since :sinceVersion:`22.1.1: Barri Plugin: enable_cbt` the plugin will try to
enable CBT automatically when the plugin option **enable_cbt=yes** is set (see below).
Since :sinceVersion:`23.0.0: Barri Plugin: enable_cbt` this option is set to **yes** by default.

As of https://knowledge.broadcom.com/external/article?legacyId=2075984 manually enabling CBT is currently not working properly. The API however works properly. To enable CBT use the Script :command:`vmware_cbt_tool.py`, it is packaged in the bareos-vmware-plugin package:

.. code-block:: shell-session
   :caption: usage of vmware\_cbt\_tool.py

   user@host:~$ vmware_cbt_tool.py --help
   usage: vmware_cbt_tool.py [-h] -s HOST [-o PORT] -u USER [-p PASSWORD] -d
                             DATACENTER [-f FOLDER] [-v VMNAME]
                             [--vm-uuid VM_UUID] [--enablecbt] [--disablecbt]
                             [--resetcbt] [--info] [--listall] [--sslverify]
                             [--dumpvmconfig]

   Process args for enabling/disabling/resetting CBT

   optional arguments:
     -h, --help            show this help message and exit
     -s HOST, --host HOST  Remote host to connect to
     -o PORT, --port PORT  Port to connect on
     -u USER, --user USER  User name to use when connecting to host
     -p PASSWORD, --password PASSWORD
                           Password to use when connecting to host
     -d DATACENTER, --datacenter DATACENTER
                           DataCenter Name
     -f FOLDER, --folder FOLDER
                           Folder Name (must start with /, use / for root folder
     -v VMNAME, --vmname VMNAME
                           Names of the Virtual Machines
     --vm-uuid VM_UUID     Instance UUIDs of the Virtual Machines
     --enablecbt           Enable CBT
     --disablecbt          Disable CBT
     --resetcbt            Reset CBT (disable, then enable)
     --info                Show information (CBT supported and enabled or
                           disabled)
     --listall             List all VMs in the given datacenter with UUID and
                           containing folder
     --sslverify           Force SSL certificate verification
     --dumpvmconfig        Dump VM config metadata to JSON file

.. note::

   The options :command:`--vm-uuid` and :command:`--listall` have been added in version :sinceVersion:`17.2.8: Barri Plugin: new options in vmware\_cbt\_tool.py`, the tool is also able now to process non-ascii character arguments for the :command:`--folder` and :command:`--vmname` arguments and vApp names can be used like folder name components.

With :command:`--listall` all VMs in the given datacenter are reported in a tabular output including instance UUID and containing Folder/vApp name.

Without the option :command:`--sslverify` also self-signed SSL certificates will
be accepted, but a warning message will be emitted in this case.

The option :command:`--dumpvmconfig` is helpful to debug issues with the transformation
of VM config metadata for recreating virtual machines.
The JSON file will be written to the current working directory when this option is used.

For the above configuration example, the command to enable CBT would be

.. code-block:: shell-session
   :caption: Example using vmware\_cbt\_tool.py

   user@host:~$ vmware_cbt_tool.py -s vcenter.example.org -u bakadm@vsphere.local -p Bak.Adm-1234 -d mydc1 -f /webservers -v websrv1 --enablecbt

Note: CBT does not work if the virtual hardware version is 6 or earlier.

After enabling CBT, Backup Jobs can be run or scheduled as usual, for example in :command:`bconsole`:

:bcommand:`run job=vm-websrv1 level=Full`

Restore
^^^^^^^

For restoring to the same still exising VM from which the backup has been taken, the VM must be **powered off** and **no snapshot** must exist. In :command:`bconsole` use the restore menu 5, select the correct FileSet and enter :bcommand:`mark *`, then :bcommand:`done`. After restore has finished, the VM will be set to its previous powerstate. So if it was powered on at backup time, it will be powered on after restore. This can be changed by using the plugin option :command:`restore_powerstate` (see below).

Since :sinceVersion:`22.0.0: Barri Plugin: recreate VMs` the plugin will recreate the VM if it does not exist. By passing plugin options, with this version it is also possible to recreate the VM in a different folder, datacenter, host, cluster, resource pool or datastore, see below for details. The MAC address and the UUID of the VM will be restored, too. Restoring to a different VM location, eg, by passing a different folder, will **create a new VM** even **if the VM** which was backed up **still exists**. In this case, the new VM will get a new generated MAC address and UUID.

.. note::

   When restoring a VM to a different location while the backed up VM still exists and a static IP is configured within the VM: To avoid IP address conflicts, make sure to also add the plugin option :command:`restore_powerstate=off` and disable or change the network adapter configuration of the VM before powering it on.

To restore to a different folder, datacenter, host, cluster, resource pool or datastore, the corresponding plugin options must be passed. All plugin options which have been effective at backup time will be passed on restore and each individual option can be overridden by passing an options string at restore time. For example, to restore to a different VM name and different datastore, pass the following plugin option string:

.. code-block:: bconsole
   :caption: Example restore plugin options string

   python:datastore=datastore2:vmname=testvm1restored

All other plugin options which are not passed explicitily on restore will be the same as at backup time.

Note that most plugin options are used for backup and restore, but there are some which can be only used on restore, for example to prevent from powering on the VM automatically after restore if it was powered on at backup time, use this plugin options string:

.. code-block:: bconsole
   :caption: Example restore plugin options string with powerstate

   python:restore_datastore=datastore2:vmname=testvm1restored:restore_powerstate=off

See below for a complete restore example and description of all plugin options.


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
