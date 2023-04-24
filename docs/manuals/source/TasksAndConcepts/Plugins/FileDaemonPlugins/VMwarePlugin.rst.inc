.. _VMwarePlugin:

VMware Plugin
~~~~~~~~~~~~~

.. index::
   single: Plugin; VMware
   single: VMware Plugin

The |vmware| Plugin can be used for agentless backups of virtual machines running on |vsphere|. It makes use of CBT (Changed Block Tracking) to do space efficient full and incremental backups, see below for mandatory requirements.

It is included in Bareos since :sinceVersion:`15.2.0: VMware Plugin`.

Status
^^^^^^

The Plugin can do full, differential and incremental backup and restore of VM disks.

Since :sinceVersion:`22.0.0: VMware Plugin` it also backs up the VM configuration metadata so that it can recreate deleted VMs and then restore the VM disks.

Current limitations amongst others are:

.. limitation:: VMware Plugin: Normal VM disks can not be excluded from the backup.

   It is not yet possible to exclude normal (dependent) VM disks from backups.
   However, independent disks are excluded implicitly because they are not affected
   by snapshots which are required for CBT based backup.



.. limitation:: VMware Plugin: Virtual Disks have to be smaller than 2TB for restore to local VMDK.

   Virtual Disks have to be smaller than 2 TB for being able to restore to local VMDK files, see :mantis:`670`.



.. limitation:: VMware Plugin: Restore not possible on recreated VM when VM was created from template or OVA.

   When creating a VM from template or OVA, the parameter **ddb.adaptertype** in the .vmdk file is changed from **lsilogic** to **buslogic** although SCSI adapter type is VMware Paravirtual or LSI Logic. Restore works to the same still existing VM, but when such a VM was completely removed so that the plugin recreates it via vSphere API, the newly created disk will have **ddb.adaptertype** set to **lsilogic** with different numbers of cylinders and heads which causes the restore to fail due to disk geometry mismatch. Currently there's no known workaround.


.. limitation:: VMware Plugin: Restore to different vCenter Server is unsupported.

   Restore to a different vCenter Server was not tested, it will probably not work, so it is currently unsupported.


Requirements
^^^^^^^^^^^^

As the Plugin is based on the |vsphere| Storage APIs for Data Protection, which requires at least a |vsphere| Essentials License. It is tested against |vsphere| Storage APIs for Data Protection of |vmware| 7.0.1. It does not work with standalone unlicensed |vmware| ESXi\ |trade|.

Since Bareos :sinceVersion:`22.0.0: VMware Plugin: VDDK 8.0.0` the plugin is using the Virtual Disk Development Kit (VDDK) 8.0.0, as of the VDDK 8.0 release notes, it should be compatible with vSphere 8 and the next major release (except new features) and backward compatible with vSphere 6.7 and 7, see VDDK release notes at https://developer.vmware.com/web/sdk/8.0/vddk for details.

This plugin requires the pyVmomi module version 7.0.2 or greater. Since Bareos :sinceVersion:`21.0.0: VMware Plugin: pyVmomi` the package **bareos-vmware-plugin** no longer includes a dependency on a pyVmomi package, because some Linux distributions don't provide current versions. Consequently, pyVmomi must be either installed by using :command:`pip install pyvmomi` or by manually installing a distribution provided pyVmomi package.

Installation
^^^^^^^^^^^^

Install the package **bareos-vmware-plugin** including its requirements by using an appropriate package management tool (eg. :command:`yum`, :command:`zypper`, :command:`apt`)

Configuration
^^^^^^^^^^^^^

First add a user account in vCenter that has full privileges by assigning the account to an administrator role or by adding the account to a group that is assigned to an administrator role. While any user account with full privileges could be used, it is better practice to create a separate user account, so that the actions by this account logged in vSphere are clearly distinguishable. In the future a more detailed set of required role privileges may be defined.

When using the vCenter appliance with embedded SSO, a user account usually has the structure :command:`<username>@vsphere.local`, it may be different when using Active Directory as SSO in vCenter. For the examples here, we will use :command:`bakadm@vsphere.local` with the password :command:`Bak.Adm-1234`.

For more details regarding users and permissions see vSphere documentation at https://docs.vmare.com

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
   :caption: bareos-dir.conf: VMware Plugin Job and FileSet definition

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

For VMs defined in the root-folder, :command:`folder=/` must be specified in the Plugin definition.

Since Bareos :sinceVersion:`17.2.4: bareos-vmware-plugin: module\_path without vmware\_plugin subdirectory` the :strong:`module\_path` is without :file:`vmware_plugin` directory. On upgrades you either adapt your configuration from

.. code-block:: bareosconfig
   :caption: python:module\_path for Bareos < 17.2.0

   Plugin = "python"
            ":module_path=/usr/lib64/bareos/plugins/vmware_plugin"
            ":module_name=bareos-fd-vmware"
            ":..."

to

.. code-block:: bareosconfig
   :caption: python:module\_path for Bareos >= 17.2.0

   Plugin = "python"
            ":module_path=/usr/lib64/bareos/plugins"
            ":module_name=bareos-fd-vmware"
            ":..."

or install the **bareos-vmware-plugin-compat** package which includes compatibility symbolic links.

Since :sinceVersion:`17.2.4: VMware Plugin: vcthumbprint`: The Plugin is using the Virtual Disk Development Kit (VDDK), which requires to pass the thumbprint of the vCenter SSL Certificate since version 6.5. The thumbprint is the SHA1 checksum of the SSL Certificate. The thumbprint can be retrieved like this:

.. code-block:: shell-session
   :caption: Example Retrieving vCenter SSL Certificate Thumbprint

   echo -n | openssl s_client -connect vcenter.example.org:443 2>/dev/null | openssl x509 -noout -fingerprint -sha1  | tr -d ":"

The result would look like this:

.. code-block:: shell-session
   :caption: Example Result Thumbprint

   SHA1 Fingerprint=AABBCCDDEEFF11223344556677889900AABBCCDD

For additional security, there is a now plugin option :command:`vcthumbprint`, that can optionally be added. It must be given without colons like in the following example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin Options with vcthumbprint

       ...
       Plugin = "python"
                ":module_name=bareos-fd-vmware"
                ":dc=mydc1:folder=/webservers"
                ":vmname=websrv1"
                ":vcserver=vcenter.example.org"
                ":vcuser=bakadm@vsphere.local"
                ":vcpass=Bak.Adm-1234"
                ":vcthumbprint=AABBCCDDEEFF11223344556677889900AABBCCDD"
       ...

For ease of use (but less secure) when the :command:`vcthumbprint` is not given, the plugin will retrieve the thumbprint.

Also since :sinceVersion:`17.2.4: VMware Plugin: transport=nbdssl` another optional plugin option has been added that can be used for trying to force a given transport method. Normally, when no transport method is given, VDDK will negotiate available transport methods and select the best one. For a description of transport methods, see

https://developer.vmware.com/docs/16967/virtual-disk-development-kit-programming-guide/GUID-15395099-5300-4D3F-BCC3-E50DCDC954C2.html

When the plugin runs in a VMware virtual machine which has access to datastore where the virtual disks to be backed up reside, VDDK will use the hotadd transport method. On a physical server without SAN access, it will use the NBD transport method, hotadd transport is not available in this case.

To try forcing a given transport method, the plugin option :command:`transport` can be used, for example

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin options with transport

       ...
       Plugin = "python"
                ":module_name=bareos-fd-vmware"
                ":dc=mydc1"
                ":folder=/webservers"
                ":vmname=websrv1"
                ":vcserver=vcenter.example.org"
                ":vcuser=bakadm@vsphere.local"
                ":vcpass=Bak.Adm-1234"
                ":transport=nbdssl"
       ...

Note that the backup will fail when specifying a transport method that is not available.

Since :sinceVersion:`17.2.8: VMware Plugin: non-ascii characters` it is possible to use non-ascii characters and blanks in the configuration for :strong:`folder` and :strong:`vmname`. Also virtual disk file names or paths containing non-ascii characters are handled correctly now. For backing up VMs that are contained in vApps, it is now possible to use the vApp name like a folder component. For example, if we have the vApp named
:command:`Test vApp` in the folder :file:`/Test/Test Folder` and the vApp contains the two VMs :command:`Test VM 01` and :command:`Test VM 02`, then the configuration of the filesets should look like this:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin FileSet definition for vApp

   FileSet {
     Name = "vApp_Test_vm_Test_VM_01_fileset"

     Include {
       Options {
            Signature = XXH128
            Compression = LZ4
       }
       Plugin = "python"
                ":module_name=bareos-fd-vmware"
                ":dc=mydc1"
                ":folder=/Test/Test Folder/Test vApp"
                ":vmname=Test VM 01"
                ":vcserver=vcenter.example.org"
                ":vcuser=bakadm@vsphere.local"
                ":vcpass=Bak.Adm-1234"
     }
   }

   FileSet {
     Name = "vApp_Test_vm_Test_VM_02_fileset"

     Include {
       Options {
            Signature = XXH128
            Compression = LZ4
       }
       Plugin = "python"
                ":module_name=bareos-fd-vmware"
                ":dc=mydc1"
                ":folder=/Test/Test Folder/Test vApp"
                ":vmname=Test VM 02"
                ":vcserver=vcenter.example.org"
                ":vcuser=bakadm@vsphere.local"
                ":vcpass=Bak.Adm-1234"
     }
   }

However, it is important to know that it is not possible to use non-ascii characters as an argument for the :strong:`Name`\  of a job or fileset resource.

Since :sinceVersion:`20: VMware Plugin: config file` it is optionally possible to use a configuration file on the system running the Bareos File Daemon. This can be useful to specify common plugin options instead of having to repeat them in every Fileset. Options which are specifed in the config file will override options from the Fileset, if the same option is given there, too. A warning will be issued in that case. Use the plugin option **config_file** to specify the config file name as in the following example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin Job and FileSet definition with config_file

   FileSet {
     Name = "vm-websrv1_fileset"

     Include {
       Options {
            Signature = XXH128
            Compression = LZ4
       }
       Plugin = "python"
                ":module_name=bareos-fd-vmware"
                ":dc=mydc1"
                ":folder=/webservers"
                ":vmname=websrv1"
                ":config_file=/etc/bareos/vmware-plugin.ini"
     }
   }

And the config file as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/vmware-plugin.ini

   [vmware_plugin_options]
   vcserver=vcenter.example.org
   vcuser=bakadm@vsphere.local
   vcpass=Bak.Adm-1234

.. note::

   Do not use quotes in the above config file, it is processed by the Python ConfigParser module and the quotes would not be stripped from the string.

Since :sinceVersion:`20: VMware Plugin: quiesce` To allow backing up VMs which do not support quiesced snapshots, it is now possible to use the plugin option **quiesce**. By default quiescing when not explicitly using this option, quiescing is enabled to create backups that are as consistent as possible. When setting **quiesce=no** it is more likely to backup an inconsistent state. In this case, the backup job log will contain an appropriate warning and the the job termination will be **Backup OK -- with warnings**.

Backup
^^^^^^

Before running the first backup, CBT (Changed Block Tracking) must be enabled for the VMs to be backed up.

As of https://kb.vmware.com/s/article/2075984 manually enabling CBT is currently not working properly. The API however works properly. To enable CBT use the Script :command:`vmware_cbt_tool.py`, it is packaged in the bareos-vmware-plugin package:

.. code-block:: shell-session
   :caption: usage of vmware\_cbt\_tool.py

   user@host:~$ vmware_cbt_tool.py --help
   usage: vmware_cbt_tool.py [-h] -s HOST [-o PORT] -u USER [-p PASSWORD] -d
                             DATACENTER [-f FOLDER] [-v VMNAME]
                             [--vm-uuid VM_UUID] [--enablecbt] [--disablecbt]
                             [--resetcbt] [--info] [--listall]

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

Note: the options :command:`--vm-uuid` and :command:`--listall` have been added in version :sinceVersion:`17.2.8: VMware Plugin: new options in vmware\_cbt\_tool.py`, the tool is also able now to process non-ascii character arguments for the :command:`--folder` and :command:`--vmname` arguments and vApp names can be used like folder name components. With :command:`--listall` all VMs in the given datacenter are reported
in a tabular output including instance UUID and containing Folder/vApp name.

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

Since :sinceVersion:`22.0.0: VMware Plugin: recreate VMs` the plugin will recreate the VM if it does not exist. By passing plugin options, with this version it is also possible to recreate the VM in a different folder, datacenter, host, cluster, resource pool or datastore, see below for details. The MAC address and the UUID of the VM will be restored, too. Restoring to a different VM location, eg, by passing a different folder, will **create a new VM** even **if the VM** which was backed up **still exists**. In this case, the new VM will get a new generated MAC address and UUID.

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

Since :sinceVersion:`22.0.0: VMware Plugin: Restore using WebUI` it is possible to use the Bareos WebUI to restore VMware Plugin jobs.

When using the WebUI to restore a VMware Plugin job, it is **important** to set *Merge all client file sets* to **no** and *Merge all jobs up to the last full backup together* to **yes**. In the *File selection* all files must be selected. Only restoring selected virtual disks will probably not work and is currently unsupported. The Bareos WebUI will detect if a plugin based jobs is restore and will then show an additional *Plugin options* field, here a plugin options string starting with ``python:`` as described above can be entered.

.. image:: /include/images/bareos-webui-restore-with-pluginoptions.*
   :width: 80.0%

Restore to local VMDK File
^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: VMware Plugin; VMDK files>`\

Since :sinceVersion:`15.2.3: VMware Plugin: restore to VMDK files` it is possible to restore to local VMDK files. That means, instead of directly restoring a disk that belongs to the VM, the restore creates VMDK disk image files on the filesystem of the system that runs the |fd|. As the VM that the backup was taken from is not affected by this, it can remain switched on while restoring to local VMDK. Such a restored VMDK file can then be uploaded to a
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
   The backed up VM will be triggered to quiesce its filesystems before creating a snapshot by default to increase the data consistency. This can fail or take very long on a VM which runs heavy I/O workload. When setting ``quiesce=no`` the quiescing will be skipped, but the snapshot may be inconsistent. It is not recommended to use this option, instead try stop heavy I/O load before snapshot. This could be possible by running pre-freeze and post-thaw actions which can be configured in VMware tools, see VMware documentation for details.

cleanup_tmpfiles (optional)
   By default, temporary files created by the plugin will be cleaned up after backup or restore. When setting ``cleanup_tmpfiles=no`` they will be left over, this can be helpful for debugging purposes. Since :sinceVersion:`22.0.0: VMware Plugin`

restore_esxhost (optional)
   By default, if a VM to be restored does not exist, it will be recreated on the same host that it has been running on at backup time. Use this option to restore on the given ESX host. Since :sinceVersion:`22.0.0: VMware Plugin`

restore_cluster (optional)
   Instead of specifying *restore_esxhost*, it is also possible to specify a cluster name using this option, the ESX host will be autoselected in that case, if DRS is configured properly. Since :sinceVersion:`22.0.0: VMware Plugin`

restore_datastore (optional)
   By default, if a VM to be restored does not exist, it will be recreated in the same datastore where it was stored at backup time. Use this option to restore on the given datastore. Since :sinceVersion:`22.0.0: VMware Plugin`

restore_resourcepool (optional)
   By default, if a VM to be restored does not exist, it will be recreated in the same resource pool which it has in been in at backup time. Using this option allows to override this and specify a different resource pool. Since :sinceVersion:`22.0.0: VMware Plugin`

restore_powerstate (optional)
   By **default**, after restore a VM will be set to its **previous powerstate** which means the powerstate at backup time. When specifying ``restore_powerstate=off`` the VM will stay powered off after restore. Also can be forced to on with ``restore_powerstate=on``. Note that this will only work if DRS is configured to **fully automated**, otherwise the API request to power on a VM will be ignored. Since :sinceVersion:`22.0.0: VMware Plugin`

poweron_timeout (optional)
   Timeout in seconds to wait for a VM to be powered on after restore, default 15s. When a VM is powered on after restore (see also the option *restore_powerstate* above), the plugin will check if it succeeded by checking the power state. If it is not powered on within this timeout, the restore job will issue a warning message.

uuid (deprecated)
   The uuid option could be used instead of *dc*, *folder* and *vmname* to uniquely address a VM for backup. As the plugin since :sinceVersion:`22.0.0: VMware Plugin` is able recreate VMs in a different datacenter, folder or datastore, this option has become useless. When using uuid, restoring is only possible to the same still existing VM. It is recommended to change the configuration, as the uuid option will be dropped in the next version.
