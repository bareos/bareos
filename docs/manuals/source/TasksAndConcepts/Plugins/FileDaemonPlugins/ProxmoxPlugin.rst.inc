.. _ProxmoxPlugin:

Proxmox Plugin
~~~~~~~~~~~~~~

.. index::
   single: Plugin; Proxmox
   single: Proxmox Plugin

The |proxmox| Plugin can be used for agentless backups of virtual machines running on |proxmox ve|.
It is capable of doing full backups of both VM Guests and Container Guests.

It is included in Bareos since :sinceVersion:`25.0.0: Proxmox Plugin`.

Status
^^^^^^

The Plugin can do full backup and can has two options for restoring:

* It can restore the backup into a new proxmox guest (Virtual Machine / Container).
* It can restore the backup into a local vzdump dump file to be recovered later via the proxmox gui.


.. limitation:: Proxmox Plugin: Only Full Backups are currently supported.


Requirements
^^^^^^^^^^^^

The Proxmox Plugin needs to be installed on a member of the proxmox cluster that is running the guests that need to be backed up.

Installation
^^^^^^^^^^^^

Install the package **bareos-filedaemon-proxmox-plugin** (using :command:`apt`)

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

To define the backup of a VM in Bareos, a job definition and a fileset resource must be added to the Bareos director configuration. In proxmox ve, Guests have an unique ID (i.e. a number between 100 - 999999999) that uniquely identifies each guest.

The following example shows how to configure the backup of the VM named with the ID  *100*:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Proxmox Plugin Job and FileSet definition

   Job {
     Name = "vm-websrv1"
     JobDefs = "DefaultJob"
     FileSet = "vm-guest100_fileset"
   }

   FileSet {
     Name = "vm-guest100_fileset"

     Include {
       Options {
            Signature = XXH128
            Compression = LZ4
       }
       Plugin = "python"
                ":module_name=bareos-fd-proxmox"
                ":guestid=100"
     }
   }


Backup
^^^^^^


Restore
^^^^^^^


Restore using Bareos WebUI
''''''''''''''''''''''''''

Since :sinceVersion:`22.0.0: Proxmox Plugin: Restore using WebUI` it is possible to use the Bareos WebUI to restore Proxmox Plugin jobs.

When using the WebUI to restore a Proxmox Plugin job, it is **important** to set *Merge all client file sets* to **no** and *Merge all jobs up to the last full backup together* to **yes**. In the *File selection* all files must be selected. Only restoring selected virtual disks will probably not work and is currently unsupported. The Bareos WebUI will detect if a plugin based jobs is restore and will then show an additional *Plugin options* field, here a plugin options string starting with ``python:`` as described above can be entered.

.. image:: /include/images/bareos-webui-restore-with-pluginoptions.*
   :width: 80.0%

Restore to local .vma dump File
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Proxmox Plugin; vma dump files>`\


For restoring to local vma dump file, the plugin option :strong:`restoretodisk=yes` must be passed. The following example shows how to perform such a restore using :command:`bconsole`:

.. code-block:: bconsole
   :caption: Example restore to local vma dump file

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
   Plugin Options:  python: module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-proxmox: dc=dass5:folder=/: vmname=stephand-test02: vcserver=virtualcenter5.dass-it:vcuser=bakadm@vsphere.local: vcpass=Bak.Adm-1234: localvmdk=yes
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=639

Note: Since Bareos :sinceVersion:`15.2.3: Add additional python plugin options`
it is sufficient to add Python plugin options, e.g. by

.. code-block:: bareosconfig

   python:restoretodisk=yes

Before, all Python plugin must be repeated and the additional be added, like:

.. code-block:: bareosconfig

   "python:localvmdk=yes"

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

Note that all plugin options that have been used at backup time, are passed back on restore.

guestid (mandatory on backup)
   The ID of the VM to be backed up. On restore it is possible to override this option in order to recreate the VM with a different ID.

restoretodisk (optional)
   Restore to local ``.vma`` vzdump file instead of restoreing to guest.. Default is *no*.
