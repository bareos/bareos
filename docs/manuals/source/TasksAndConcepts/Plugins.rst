.. _section-plugins:

Plugins
=======

:index:`\ <single: Plugin>`\

The functionality of Bareos can be extended by plugins. They do exists plugins for the different daemons (Director, Storage- and File-Daemon).

To use plugins, they must be enabled in the configuration (:strong:`Plugin Directory`\  and optionally :strong:`Plugin Names`\ ).

If a :strong:`Plugin Directory`\  is specified :strong:`Plugin Names`\  defines, which plugins get loaded.

If :strong:`Plugin Names`\  is not defined, all plugins get loaded.

.. _fdPlugins:

File Daemon Plugins
-------------------

File Daemon plugins are configured by the :strong:`Plugin`\  directive of a :ref:`File Set <directive-fileset-plugin>`.



   .. warning::

      Currently the plugin command is being stored as part of the backup. The restore command in your directive should be flexible enough if things might change in future, otherwise you could run into trouble.

.. _bpipe:

bpipe Plugin
~~~~~~~~~~~~

:index:`\ <single: Plugin; bpipe>`\

The bpipe plugin is a generic pipe program, that simply transmits the data from a specified program to Bareos for backup, and from Bareos to a specified program for restore. The purpose of the plugin is to provide an interface to any system program for backup and restore. That allows you, for example, to do database backups without a local dump. By using different command lines to bpipe, you can backup any kind of data (ASCII or binary) depending on the program called.

On Linux, the Bareos bpipe plugin is part of the **bareos-filedaemon** package and is therefore installed on any system running the filedaemon.

The bpipe plugin is so simple and flexible, you may call it the "Swiss Army Knife" of the current existing plugins for Bareos.

The bpipe plugin is specified in the Include section of your Job’s FileSet resource in your :file:`bareos-dir.conf`.

.. code-block:: bareosconfig
   :caption: bpipe fileset

   FileSet {
     Name = "MyFileSet"
     Include {
       Options {
         signature = MD5
         compression = gzip
       }
       Plugin = "bpipe:file=<filepath>:reader=<readprogram>:writer=<writeprogram>
     }
   }

The syntax and semantics of the Plugin directive require the first part of the string up to the colon to be the name of the plugin. Everything after the first colon is ignored by the File daemon but is passed to the plugin. Thus the plugin writer may define the meaning of the rest of the string as he wishes. The full syntax of the plugin directive as interpreted by the bpipe plugin is:

.. code-block:: bareosconfig
   :caption: bpipe directive

   Plugin = "<plugin>:file=<filepath>:reader=<readprogram>:writer=<writeprogram>"

plugin
   is the name of the plugin with the trailing -fd.so stripped off, so in this case, we would put bpipe in the field.

filepath
   specifies the namespace, which for bpipe is the pseudo path and filename under which the backup will be saved. This pseudo path and filename will be seen by the user in the restore file tree. For example, if the value is :strong:`/MySQL/mydump.sql`, the data backed up by the plugin will be put under that "pseudo" path and filename. You must be careful to choose a naming convention that is unique to avoid a conflict with a path and filename that actually
   exists on your system.

readprogram
   for the bpipe plugin specifies the "reader" program that is called by the plugin during backup to read the data. bpipe will call this program by doing a popen on it.

writeprogram
   for the bpipe plugin specifies the "writer" program that is called by the plugin during restore to write the data back to the filesystem.

Please note that the two items above describing the "reader" and "writer", these programs are "executed" by Bareos, which means there is no shell interpretation of any command line arguments you might use. If you want to use shell characters (redirection of input or output, ...), then we recommend that you put your command or commands in a shell script and execute the script. In addition if you backup a file with reader program, when running the writer program during the restore, Bareos will not
automatically create the path to the file. Either the path must exist, or you must explicitly do so with your command or in a shell script.

See the examples about :ref:`backup-postgresql` and :ref:`backup-mysql`.

PGSQL Plugin
~~~~~~~~~~~~

See chapter :ref:`backup-postgresql-plugin`.

MySQL Plugin
~~~~~~~~~~~~

See the chapters :ref:`backup-mysql-XtraBackup` and :ref:`backup-mysql-python`.

MSSQL Plugin
~~~~~~~~~~~~

See chapter :ref:`MSSQL`.

LDAP Plugin
~~~~~~~~~~~

:index:`\ <single: Plugin; ldap>`\

This plugin is intended to backup (and restore) the contents of a LDAP server. It uses normal LDAP operation for this. The package **bareos-filedaemon-ldap-python-plugin** (:sinceVersion:`15.2.0: LDAP Plugin`) contains an example configuration file, that must be adapted to your envirnoment.

Cephfs Plugin
~~~~~~~~~~~~~

:index:`\ <single: Plugin; ceph; cephfs>`\  :index:`\ <single: Ceph; Cephfs Plugin>`\

Opposite to the :ref:`Rados Backend <SdBackendRados>` that is used to store data on a CEPH Object Store, this plugin is intended to backup a CEPH Object Store via the Cephfs interface to other media. The package **bareos-filedaemon-ceph-plugin** (:sinceVersion:`15.2.0: Cephfs Plugin`) contains an example configuration file, that must be adapted to your envirnoment.

Rados Plugin
~~~~~~~~~~~~

:index:`\ <single: Plugin; ceph; rados>`\  :index:`\ <single: Ceph; Rados Plugin>`\

Opposite to the :ref:`Rados Backend <SdBackendRados>` that is used to store data on a CEPH Object Store, this plugin is intended to backup a CEPH Object Store via the Rados interface to other media. The package **bareos-filedaemon-ceph-plugin** (:sinceVersion:`15.2.0: CEPH Rados Plugin`) contains an example configuration file, that must be adapted to your envirnoment.

GlusterFS Plugin
~~~~~~~~~~~~~~~~

:index:`\ <single: Plugin; glusterfs>`\  :index:`\ <single: GlusterFS; Plugin>`\

Opposite to the :ref:`GFAPI Backend <SdBackendGfapi>` that is used to store data on a Gluster system, this plugin is intended to backup data from a Gluster system to other media. The package **bareos-filedaemon-glusterfs-plugin** (:sinceVersion:`15.2.0: GlusterFS Plugin`) contains an example configuration file, that must be adapted to your envirnoment.

python-fd Plugin
~~~~~~~~~~~~~~~~

:index:`\ <single: Plugin; Python; File Daemon>`\

The **python-fd** plugin behaves similar to the :ref:`director-python-plugin`. Base plugins and an example get installed via the package bareos-filedaemon-python-plugin. Configuration is done in the :ref:`DirectorResourceFileSet` on the director.

We basically distinguish between command-plugin and option-plugins.

Command Plugins
^^^^^^^^^^^^^^^

Command plugins are used to replace or extend the FileSet definition in the File Section. If you have a command-plugin, you can use it like in this example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Python FD command plugins

   FileSet {
     Name = "mysql"
     Include {
       Options {
         Signature = MD5 # calculate md5 checksum per file
       }
       File = "/etc"
       Plugin = "python:module_path=/usr/lib/bareos/plugins:module_name=bareos-fd-mysql"
     }
   }

:index:`\ <single: MySQL; Backup>`\  This example uses the :ref:`MySQL plugin <backup-mysql-python>` to backup MySQL dumps in addition to :file:`/etc`.

Option Plugins
^^^^^^^^^^^^^^

Option plugins are activated in the Options resource of a FileSet definition.

Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Python FD option plugins

   FileSet {
     Name = "option"
     Include {
       Options {
         Signature = MD5 # calculate md5 checksum per file
         Plugin = "python:module_path=/usr/lib/bareos/plugins:module_name=bareos-fd-file-interact"
       }
       File = "/etc"
       File = "/usr/lib/bareos/plugins"
     }
   }

This plugin bareos-fd-file-interact from https://github.com/bareos/bareos-contrib/tree/master/fd-plugins/options-plugin-sample has a method that is called before and after each file that goes into the backup, it can be used as a template for whatever plugin wants to interact with files before or after backup.

.. _VMwarePlugin:

VMware Plugin
~~~~~~~~~~~~~

:index:`\ <single: Plugin; VMware>`\  :index:`\ <single: VMware Plugin>`\

The |vmware| Plugin can be used for agentless backups of virtual machines running on |vsphere|. It makes use of CBT (Changed Block Tracking) to do space efficient full and incremental backups, see below for mandatory requirements.

It is included in Bareos since :sinceVersion:`15.2.0: VMware Plugin`.

Status
^^^^^^

The Plugin can do full, differential and incremental backup and restore of VM disks.

Current limitations amongst others are:

.. limitation:: VMware Plugin: Normal VM disks can not be excluded from the backup.

       It is not yet possible to exclude normal (dependent) VM disks from backups.
       However, independent disks are excluded implicitly because they are not affected
       by snapshots which are required for CBT based backup.



.. limitation:: VMware Plugin: VM configuration is not backed up.

       The VM configuration is not backed up, so that it is not yet possible to recreate a completely deleted VM.



.. limitation:: VMware Plugin: Virtual Disks have to be smaller than 2TB.

       Virtual Disks have to be smaller than 2 TB, see :mantis:`670`.



.. limitation:: VMware Plugin: Restore can only be done to the same VM or to local VMDK files.

       Until Bareos Version 15.2.2, the restore has only be possible to the same existing VM with existing virtual disks.
       Since :sinceVersion:`15.2.3: VMware Plugin: restore to VMDK files`
       %**bareos-vadp-dumper** :sinceVersion:`15.2.2-15: bareos-vadp-dumper` and
       %**bareos-vmware-plugin** :sinceVersion:`15.2.2-27: bareos-vmware-plugin`
       it is also possible to restore to local VMDK files, see below for more details.



Requirements
^^^^^^^^^^^^

As the Plugin is based on the |vsphere| Storage APIs for Data Protection, which requires at least a |vsphere| Essentials License. It is tested against |vsphere| Storage APIs for Data Protection of |vmware| 5.x. It does not work with standalone unlicensed |vmware| ESXi\ |trade|.

Since Bareos :sinceVersion:`17.2.4: VMware Plugin: VDDK 6.5.2` the plugin is using the Virtual Disk Development Kit (VDDK) 6.5.2, as of the VDDK 6.5 release notes, it should be compatible with vSphere 6.5 and the next major release (except new features) and backward compatible with vSphere 5.5 and 6.0, see VDDK release notes at https://code.vmware.com/web/sdk/65/vddk for details.

Installation
^^^^^^^^^^^^

Install the package **bareos-vmware-plugin** including its requirments by using an appropriate package management tool (eg. :command:`yum`, :command:`zypper`, :command:`apt`)

The `FAQ <http://www.bareos.org/en/faq.html>`_ may have additional useful information.

Configuration
^^^^^^^^^^^^^

First add a user account in vCenter that has full privileges by assigning the account to an administrator role or by adding the account to a group that is assigned to an administrator role. While any user account with full privileges could be used, it is better practice to create a separate user account, so that the actions by this account logged in vSphere are clearly distinguishable. In the future a more detailed set of required role privilges may be defined.

When using the vCenter appliance with embedded SSO, a user account usually has the structure :command:`<username>@vsphere.local`, it may be different when using Active Directory as SSO in vCenter. For the examples here, we will use :command:`bakadm@vsphere.local` with the password :command:`Bak.Adm-1234`.

For more details regarding users and permissions in vSphere see

-  http://pubs.vmware.com/vsphere-55/topic/com.vmware.vsphere.security.doc/GUID-72BFF98C-C530-4C50-BF31-B5779D2A4BBB.html and

-  http://pubs.vmware.com/vsphere-55/topic/com.vmware.vsphere.security.doc/GUID-5372F580-5C23-4E9C-8A4E-EF1B4DD9033E.html

Make sure to add or enable the following settings in your |fd| configuration:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = /usr/lib/bareos/plugins
     Plugin Names = python
     ...
   }

Note: Depending on Platform, the Plugin Directory may also be :file:`/usr/lib64/bareos/plugins`

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
            signature = MD5
            Compression = GZIP
       }
       Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=mydc1:folder=/webservers:vmname=websrv1:vcserver=vcenter.example.org:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234"
     }
   }

For VMs defined in the root-folder, :command:`folder=/` must be specified in the Plugin definition.

Since Bareos :sinceVersion:`17.2.4: bareos-vmware-plugin: module\_path without vmware\_plugin subdirectory` the :strong:`module\_path` is without :file:`vmware_plugin` directory. On upgrades you either adapt your configuration from

.. code-block:: bareosconfig
   :caption: python:module\_path for Bareos < 17.2.0

   Plugin = "python:module_path=/usr/lib64/bareos/plugins/vmware_plugin:module_name=bareos-fd-vmware:...

to

.. code-block:: bareosconfig
   :caption: python:module\_path for Bareos >= 17.2.0

   Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:...

or install the **bareos-vmware-plugin-compat** package which includes compatibility symbolic links.

Since :sinceVersion:`17.2.4: VMware Plugin: vcthumbprint`: as the Plugin is using the Virtual Disk Development Kit (VDDK) 6.5, it is required to pass the thumbprint of the vCenter SSL Certificate, which is the SHA1 checksum of the SSL Certificate. The thumbprint can be retrieved like this:

.. code-block:: shell-session
   :caption: Example Retrieving vCenter SSL Certificate Thumbprint

   echo -n | openssl s_client -connect vcenter.example.org:443 2>/dev/null | openssl x509 -noout -fingerprint -sha1

The result would look like this:

.. code-block:: shell-session
   :caption: Example Result Thumbprint

   SHA1 Fingerprint=CC:81:81:84:A3:CF:53:ED:63:B1:46:EF:97:13:4A:DF:A5:9F:37:89

For additional security, there is a now plugin option :command:`vcthumbprint`, that can optionally be added. It must be given without colons like in the following example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin Options with vcthumbprint

       ...
       Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=mydc1:folder=/webservers:vmname=websrv1:vcserver=vcenter.example.org:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234:vcthumbprint=56F597FE60521773D073A2ED47CE07282CE6FE9C"
       ...

For ease of use (but less secure) when the :command:`vcthumbprint` is not given, the plugin will retrieve the thumbprint.

Also since :sinceVersion:`17.2.4: VMware Plugin: transport=nbdssl` another optional plugin option has been added that can be used for trying to force a given transport method. Normally, when no transport method is given, VDDK will negotiate available transport methods and select the best one. For a description of transport methods, see

https://code.vmware.com/doc/preview?id=4076#/doc/vddkDataStruct.5.5.html

When the plugin runs in a VMware virtual machine which has access to datastore where the virtual disks to be backed up reside, VDDK will use the hotadd transport method. On a physical server without SAN access, it will use the NBD transport method, hotadd transport is not available in this case.

To try forcing a given transport method, the plugin option :command:`transport` can be used, for example

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin options with transport

       ...
       Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=mydc1:folder=/webservers:vmname=websrv1:vcserver=vcenter.example.org:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234:transport=nbdssl"
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
            signature = MD5
            Compression = GZIP
       }
       Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=mydc1:folder=/Test/Test Folder/Test vApp:vmname=Test VM 01:vcserver=vcenter.example.org:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234"
     }
   }

   FileSet {
     Name = "vApp_Test_vm_Test_VM_02_fileset"

     Include {
       Options {
            signature = MD5
            Compression = GZIP
       }
       Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=mydc1:folder=/Test/Test Folder/Test vApp:vmname=Test VM 02:vcserver=vcenter.example.org:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234"
     }
   }

However, it is important to know that it is not possible to use non-ascii characters as an argument for the :strong:`Name`\  of a job or fileset resource.

Before this, it was only possible specify VMs contained in vApps by using the instance UUID with the :strong:`uuid` instead of :strong:`folder` and :strong:`vmname` like this:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: VMware Plugin FileSet definition for vApp

   FileSet {
     Name = "vApp_Test_vm_Test_VM_01_fileset"
       ...

       Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=mydc1:uuid=502b112f-3954-d761-be08-5570c8a780e2:vcserver=vcenter.example.org:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234"
     }
   }

Note that it must be the so called vSphere instance UUID, not the BIOS UUID which is shown inside a VM when using for example :command:`dmidecode`. The :command:`vmware_cbt_tool.py` utility was adapted accordingly (see below for details).

Backup
^^^^^^

Before running the first backup, CBT (Changed Block Tracking) must be enabled for the VMs to be backed up.

As of http://kb.vmware.com/kb/2075984 manually enabling CBT is currently not working properly. The API however works properly. To enable CBT use the Script :command:`vmware_cbt_tool.py`, it is packaged in the bareos-vmware-plugin package:

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

For restore, the VM must be powered off and no snapshot must exist. In :command:`bconsole` use the restore menu 5, select the correct FileSet and enter :bcommand:`mark *`, then :bcommand:`done`. After restore has finished, the VM can be powered on.

Restore to local VMDK File
^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: VMware Plugin; VMDK files>`\

Since :sinceVersion:`15.2.3: VMware Plugin: restore to VMDK files` it is possible to restore to local VMDK files. That means, instead of directly restoring a disk that belongs to the VM, the restore creates VMDK disk image files on the filesystem of the system that runs the |fd|. As the VM that the backup was taken from is not affected by this, it can remain switched on while restoring to local VMDK. Such a restored VMDK file can then be uploaded to a
|vsphere| datastore or accessed by tools like `guestfish <http://libguestfs.org/guestfish.1.html>`_ to extract single files.

For restoring to local VMDK, the plugin option :strong:`localvmdk=yes` must be passed. The following example shows how to perform such a restore using :command:`bconsole`:

.. code-block:: shell-session
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

Note: Since Bareos :sinceVersion:`15.2.3: Add additional python plugin options` it is sufficient to add Python plugin options, e.g. by

:strong:`python:localvmdk=yes`

Before, all Python plugin must be repeated and the additional be added, like: :file:`python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-vmware:dc=dass5:folder=/:vmname=stephand-test02:vcserver=virtualcenter5.dass-it:vcuser=bakadm@vsphere.local:vcpass=Bak.Adm-1234:localvmdk=yes`

After the restore process has finished, the restored VMDK files can be found under \path{/tmp/bareos-restores/}:

.. code-block:: shell-session
   :caption: Example result of restore to local VMDK

   # <input>ls -laR /tmp/bareos-restores</input>
   /tmp/bareos-restores:
   total 28
   drwxr-x--x.  3 root root  4096 Feb 25 15:47 .
   drwxrwxrwt. 17 root root 20480 Feb 25 15:44 ..
   drwxr-xr-x.  2 root root  4096 Feb 25 15:19 [ESX5-PS100] stephand-test02

   /tmp/bareos-restores/[ESX5-PS100] stephand-test02:
   total 7898292
   drwxr-xr-x. 2 root root       4096 Feb 25 15:19 .
   drwxr-x--x. 3 root root       4096 Feb 25 15:47 ..
   -rw-------. 1 root root 2075197440 Feb 25 15:19 stephand-test02_1.vmdk
   -rw-------. 1 root root 6012731392 Feb 25 15:19 stephand-test02.vmdk

.. _oVirtPlugin:

oVirt Plugin
~~~~~~~~~~~~

:index:`\ <single: Plugin; oVirt>`\  :index:`\ <single: oVirt Plugin>`\

The oVirt Plugin can be used for agentless backups of virtual machines running on oVirt or Red Hat Virtualization (RHV).
It was tested with oVirt/RHV 4.3. There are currently no known technical differences between
RHV and oVirt (which is RHV's upstream project) that are relevant for this plugin, so both
names are equivalent in this documentation if not explicitly mentioned.

For backing up a VM, the plugin performs the following steps:

* Retrieve the VM configuration data from the oVirt API as OVF XML data
* Take a snapshot of the VM
* Retrieve the VM disk image data of the snapshot via oVirt Image I/O
* Remove the snapshot


It is included in Bareos since :sinceVersion:`19: oVirt Plugin`.

.. _oVirtPlugin-status:

Status
^^^^^^

The Plugin can currently only take full backups of VM disks.

When performing restores, the plugin can do one of the following:

* Write local disk image files
* Create a new VM with new disks
* Overwrite existing disks of an existing VM

Currently, the access to disk images is implemented only via the oVirt Image I/O Proxy component
of the engine server.

.. _oVirtPlugin-requirements:

Requirements
^^^^^^^^^^^^

The plugin is currently only available for Red Hat Enterprise Linux 7 and CentOS 7. It requires the
Python oVirt Engine SDK version 4, Red Hat Subscriptions customers can find the package
**python-ovirt-engine-sdk4** in the ``rh-common`` repo, which may not be enabled by default.
The oVirt project provides the package at https://resources.ovirt.org/pub/ovirt-4.3/rpm/el7/x86_64/.

The system running the |fd| with this plugin must have network access to the oVirt/RHV
engine server on the TCP ports 443 (https for API access) and 54323 (for Image I/O Proxy access).

The QEMU Guest Agent (QEMU GA) should be installed inside VMs to optimize the consistency
of snapshots by filesystem flushing and quiescing. This also allows custom freeze/thaw hook
scripts in Linux VMs to ensure application level consistency of snapshots. On Windows the
QEMU GA provides VSS support thus live snapshots attempt to quiesce whenever possible.

.. _oVirtPlugin-installation:

Installation
^^^^^^^^^^^^

The installation is done by installing the package **bareos-filedaemon-ovirt-python-plugin**:

:command:`yum install bareos-filedaemon-ovirt-python-plugin`

.. _oVirtPlugin-configuration:

Configuration
^^^^^^^^^^^^^

As the Plugin needs access to the oVirt API, an account with appropriate privileges must be used.
The default **admin@internal** user works, as it has all privileges. Using an account with
less privileges should be possible, the plugin needs to be able to do the following:

* Read VM metadata
* Read, create and write disk images via Image I/O Proxy
* Create VMs

The exact required oVirt roles are beyond the scope of this document.

To verify SSL certificates, the plugin must know the CA certificate of the oVirt enviroment,
it can be downloaded from the oVirt/RHV engine start page manually, or by using the following
command:

:command:`curl -k -o /etc/bareos/ovirt-ca.cert https://engine.example.com/ovirt-engine/services/pki-resource?resource=ca-certificate&format=X509-PEM-CA`

For each VM to be backed up, a **job** and a **fileset** must be configured. For
example to backup the VM **testvm1**, configure the fileset as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/fileset/testvm1_fileset.conf

   FileSet {
      Name = "testvm1_fileset"

      Include {
         Options {
            signature = MD5
            Compression = LZ4
         }
         Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-ovirt:ca=/etc/bareos/ovirt-ca.cert:server=engine.example.com:username=admin@internal:password=secret:vm_name=testvm1"
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

Mandatory Plugin Options:

module_path
   Path to the plugin, when installed from Bareos packages, this is always
   :file:`/usr/lib64/bareos/plugins`

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

uuid
   Instead of specifying the VM to be backed up by name (using option **vm_name**), the VM
   can be specified by its uuid.

include_disk_aliases
   Comma separated list of disk alias names to be included only. If not specified, all disks
   that are attached to the VM are included. Currently only used on backup.

exclude_disk_aliases
   Comma separated list of disk alias names to be excluded, if not specified, no disk will
   be excluded. Currently only used on backup. Note that the **include_disk_aliases** options
   is applied first, then **exclude_disk_aliases**, so using both usually makes no sense.
   Also note that disk alias names are not unique, so if two disks of a VM have the same
   alias name, they will be excluded both.

overwrite
   When restoring disks of an existing VM, the option **overwrite=yes** must be explictly
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


.. _oVirtPlugin-backup

Backup
^^^^^^

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
   OK to run? (yes/mod/no): *<input>yes</input>
   Job queued. JobId=1


.. note::

   As the oVirt/RHV API does not yet allow Incremental backups, the plugin will only
   allow to run full level backups to prevent from using the Incremental pool
   accidentally. Please make sure to configure a schedule that always runs
   full level backups for jobs using this plugin.


.. _oVirtPlugin-restore

Restore
^^^^^^^

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

.. _oVirtPlugin-restore-to-local-image

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
the **guestfish** tool (see http://libguestfs.org/guestfish.1.html). This tool
can also be used to extract single files from the disk image.


.. _PerconaXtrabackupPlugin:
.. _backup-mysql-XtraBackup:

Backup of MySQL Databases using the Bareos MySQL Percona XtraBackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Plugin; MySQL Backup>`
:index:`\ <single: Percona XtraBackup>`
:index:`\ <single: XtraBackup>`

This plugin uses Perconas XtraBackup tool, to make full and incremental backups of Mysql / MariaDB databases.

The key features of XtraBackup are:

- Incremental backups
- Backups that complete quickly and reliably
- Uninterrupted transaction processing during backups
- Savings on disk space and network bandwidth
- Higher uptime due to faster restore time

Incremental backups only work for INNODB tables, when using MYISAM, only full backups can be created.


Prerequisites
'''''''''''''

Install the XtraBackup tool from Percona. Documentation and packages are available here: https://www.percona.com/software/mysql-database/percona-XtraBackup. The plugin was successfully tested with XtraBackup versions 2.3.5 and 2.4.4.

As it is a Python plugin, it will also require to have the package **bareos-filedaemon-python-plugin** installed on the |fd|, where you run it.

For authentication the :file:`.mycnf` file of the user running the |fd| is used. Before proceeding, make sure that XtraBackup can connect to the database and create backups.


Installation
''''''''''''

Make sure you have met the prerequisites, after that install the package **bareos-filedaemon-percona_XtraBackup-python-plugin**.

Configuration
'''''''''''''

Activate your plugin directory in the |fd| configuration. See :ref:`fdPlugins` for more about plugins in general.

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = /usr/lib64/bareos/plugins
     Plugin Names = "python"
   }

Now include the plugin as command-plugin in the Fileset resource:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/mysql.conf

   FileSet {
       Name = "mysql"
       Include  {
           Options {
               compression=GZIP
               signature = MD5
           }
           File = /etc
           #...
           Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-percona_xtrabackup:mycnf=/root/.my.cnf"
       }
   }

If used this way, the plugin will call XtraBackup to create a backup of all databases in the xbstream format. This stream will be processed by Bareos. If job level is incremental, XtraBackup will perform an incremental backup since the last backup – for InnoDB tables. If you have MyISAM tables, you will get a full backup of those.

You can append options to the plugin call as key=value pairs, separated by ’:’. The following options are available:

-  With :strong:`mycnf` you can make XtraBackup use a special mycnf-file with login credentials.

-  :strong:`dumpbinary` lets you modify the default command XtraBackup.

-  :strong:`dumpoptions` to modify the options for XtraBackup. Default setting is: :command:`--backup --datadir=/var/lib/mysql/ --stream=xbstream --extra-lsndir=/tmp/individual_tempdir`

-  :strong:`restorecommand` to modify the command for restore. Default setting is: :command:`xbstream -x -C`

-  :strong:`strictIncremental`: By default (false), an incremental backup will create data, even if the Log Sequence Number (LSN) wasn’t increased since last backup. This is to ensure, that eventual changes to MYISAM tables get into the backup. MYISAM does not support incremental backups, you will always get a full bakcup of these tables. If set to true, no data will be written into backup, if the LSN wasn’t changed.

Restore
'''''''

With the usual Bareos restore mechanism a file-hierarchy will be created on the restore client under the default restore location:

:file:`/tmp/bareos-restores/_percona/`

Each restore job gets an own subdirectory, because Percona expects an empty directory. In that subdirectory, a new directory is created for every backup job that was part of the Full-Incremental sequence.

The naming scheme is: :file:`fromLSN_toLSN_jobid`

Example:

::

   /tmp/bareos-restores/_percona/351/
   |-- 00000000000000000000_00000000000010129154_0000000334
   |-- 00000000000010129154_00000000000010142295_0000000335
   |-- 00000000000010142295_00000000000010201260_0000000338

This example shows the restore tree for restore job with ID 351. First subdirectory has all files from the first full backup job with ID 334. It starts at LSN 0 and goes until LSN 10129154.

Next line is the first incremental job with ID 335, starting at LSN 10129154 until 10142295. The third line is the 2nd incremental job with ID 338.

To further prepare the restored files, use the :command:`XtraBackup --prepare` command. Read https://www.percona.com/doc/percona-xtrabackup/2.4/backup_scenarios/incremental_backup.html for more information.


Troubleshooting
'''''''''''''''
If things don't work as expected, make sure that

- the |fd| (FD) works in general, so that you can make simple file backups and restores
  - the Bareos FD Python plugins work in general, try one of
  the shipped simple sample plugins
- Make sure *XtraBackup* works as user root, MySQL access needs to be
  configured properly


Support is available here: https://www.bareos.com


.. _sdPlugins:

Storage Daemon Plugins
----------------------

.. _plugin-autoxflate-sd:

autoxflate-sd
~~~~~~~~~~~~~

:index:`\ <single: Plugin; autoxflate-sd>`\

This plugin is part of the **bareos-storage** package.

The autoxflate-sd plugin can inflate (decompress) and deflate (compress) the data being written to or read from a device. It can also do both.

.. image:: /include/images/autoxflate-functionblocks.*
   :width: 80.0%




Therefore the autoxflate plugin inserts a inflate and a deflate function block into the stream going to the device (called OUT) and coming from the device (called IN).

Each stream passes first the inflate function block, then the deflate function block.

The inflate blocks are controlled by the setting of the :config:option:`sd/device/AutoInflate`\  directive.

The deflate blocks are controlled by the setting of the :config:option:`sd/device/AutoDeflate`\ , :config:option:`sd/device/AutoDeflateAlgorithm`\  and :config:option:`sd/device/AutoDeflateLevel`\  directives.

The inflate blocks, if enabled, will uncompress data if it is compressed using the algorithm that was used during compression.

The deflate blocks, if enabled, will compress uncompressed data with the algorithm and level configured in the according directives.

The series connection of the inflate and deflate function blocks makes the plugin very flexible.

Szenarios where this plugin can be used are for example:

-  client computers with weak cpus can do backups without compression and let the sd do the compression when writing to disk

-  compressed backups can be recompressed to a different compression format (e.g. gzip |rarr| lzo) using migration jobs

-  client backups can be compressed with compression algorithms that the client itself does not support

Multi-core cpus will be utilized when using parallel jobs as the compression is done in each jobs’ thread.

When the autoxflate plugin is configured, it will write some status information into the joblog.

.. code-block:: bareosmessage
   :caption: used compression algorithm

   autodeflation: compressor on device FileStorage is FZ4H

.. code-block:: bareosmessage
   :caption: configured inflation and deflation blocks

   autoxflate-sd.c: FileStorage OUT:[SD->inflate=yes->deflate=yes->DEV] IN:[DEV->inflate=yes->deflate=yes->SD]

.. code-block:: bareosmessage
   :caption: overall deflation/inflation ratio

   autoxflate-sd.c: deflate ratio: 50.59%

Additional :config:option:`sd/storage/AutoXflateOnReplication`\  can be configured at the Storage resource.

scsicrypto-sd
~~~~~~~~~~~~~

:index:`\ <single: Plugin; scsicrypto-sd>`\

This plugin is part of the **bareos-storage-tape** package.

General
^^^^^^^

.. _LTOHardwareEncryptionGeneral:

LTO Hardware Encryption
'''''''''''''''''''''''

Modern tape-drives, for example LTO (from LTO4 onwards) support hardware encryption. There are several ways of using encryption with these drives. The following three types of key management are available for encrypting drives. The transmission of the keys to the volumes is accomplished by either of the three:

-  A backup application that supports Application Managed Encryption (AME)

-  A tape library that supports Library Managed Encryption (LME)

-  A Key Management Appliance (KMA)

We added support for Application Managed Encryption (AME) scheme, where on labeling a crypto key is generated for a volume and when the volume is mounted, the crypto key is loaded. When finally the volume is unmounted, the key is cleared from the memory of the Tape Drive using the SCSI SPOUT command set.

If you have implemented Library Managed Encryption (LME) or a Key Management Appliance (KMA), there is no need to have support from Bareos on loading and clearing the encryption keys, as either the Library knows the per volume encryption keys itself, or it will ask the KMA for the encryption key when it needs it. For big installations you might consider using a KMA, but the Application Managed Encryption implemented in Bareos should also scale rather well and have a low overhead as the keys are
only loaded and cleared when needed.

The scsicrypto-sd plugin
''''''''''''''''''''''''

The :command:`scsicrypto-sd` hooks into the :strong:`unload`, :strong:`label read`, :strong:`label write` and :strong:`label verified` events for loading and clearing the key. It checks whether it it needs to clear the drive by either using an internal state (if it loaded a key before) or by checking the state of a special option that first issues an encrytion status query. If there is a connection to the director
and the volume information is not available, it will ask the director for the data on the currently loaded volume. If no connection is available, a cache will be used which should contain the most recently mounted volumes. If an encryption key is available, it will be loaded into the drive’s memory.

Changes in the director
'''''''''''''''''''''''

The director has been extended with additional code for handling hardware data encryption. The extra keyword **encrypt** on the label of a volume will force the director to generate a new semi-random passphrase for the volume, which will be stored in the database as part of the media information.

A passphrase is always stored in the database base64-encoded. When a so called **Key Encryption Key** is set in the config of the director, the passphrase is first wrapped using RFC3394 key wrapping and then base64-encoded. By using key wrapping, the keys in the database are safe against people sniffing the info, as the data is still encrypted using the Key Encryption Key (which in essence is just an extra passphrase of the same length as the volume passphrases used).

When the storage daemon needs to mount the volume, it will ask the director for the volume information and that protocol is extended with the exchange of the base64-wrapped encryption key (passphrase). The storage daemon provides an extra config option in which it records the Key Encryption Key of the particular director, and as such can unwrap the key sent into the original passphrase.

As can be seen from the above info we don’t allow the user to enter a passphrase, but generate a semi-random passphrase using the openssl random functions (if available) and convert that into a readable ASCII stream of letters, numbers and most other characters, apart from the quotes and space etc. This will produce much stronger passphrases than when requesting the info from a user. As we store this information in the database, the user never has to enter these passphrases.

The volume label is written in unencrypted form to the volume, so we can always recognize a Bareos volume. When the key is loaded onto the drive, we set the decryption mode to mixed, so we can read both unencrypted and encrypted data from the volume. When no key or the wrong key has been loaded, the drive will give an IO error when trying to read the volume. For disaster recovery you can store the Key Encryption Key and the content of the wrapped encryption keys somewhere safe and the
:ref:`bscrypto <bscrypto>` tool together with the scsicrypto-sd plugin can be used to get access to your volumes, in case you ever lose your complete environment.

If you don’t want to use the scsicrypto-sd plugin when doing DR and you are only reading one volume, you can also set the crypto key using the bscrypto tool. Because we use the mixed decryption mode, in which you can read both encrypted and unencrypted data from a volume, you can set the right encryption key before reading the volume label.

If you need to read more than one volume, you better use the scsicrypto-sd plugin with tools like bscan/bextract, as the plugin will then auto-load the correct encryption key when it loads the volume, similiarly to what the storage daemon does when performing backups and restores.

The volume label is unencrypted, so a volume can also be recognized by a non-encrypted installation, but it won’t be able to read the actual data from it. Using an encrypted volume label doesn’t add much security (there is no security-related info in the volume label anyhow) and it makes it harder to recognize either a labeled volume with encrypted data or an unlabeled new volume (both would return an IO-error on read of the label.)

.. _configuration-1:

Configuration
^^^^^^^^^^^^^

SCSI crypto setup
'''''''''''''''''

The initial setup of SCSI crypto looks something like this:

-  Generate a Key Encryption Key e.g.

   .. code-block:: shell-session

      bscrypto -g -

For details see :ref:`bscrypto <bscrypto>`.

Security Setup
''''''''''''''

Some security levels need to be increased for the storage daemon to be able to use the low level SCSI interface for setting and getting the encryption status on a tape device.

The following additional security is needed for the following operating systems:

Linux (SG_IO ioctl interface):


The user running the storage daemon needs the following additional capabilities: :index:`\ <single: Platform; Linux; Privileges>`\

-  :strong:`CAP_SYS_RAWIO` (see capabilities(7))

   -  On older kernels you might need :strong:`CAP_SYS_ADMIN`. Try :strong:`CAP_SYS_RAWIO` first and if that doesn’t work try :strong:`CAP_SYS_ADMIN`

-  If you are running the storage daemon as another user than root (which has the :strong:`CAP_SYS_RAWIO` capability), you need to add it to the current set of capabilities.

-  If you are using systemd, you could add this additional capability to the CapabilityBoundingSet parameter.

   -  For systemd add the following to the bareos-sd.service: :strong:`Capabilities=cap_sys_rawio+ep`

You can also set up the extra capability on :command:`bscrypto` and :command:`bareos-sd` by running the following commands:

.. code-block:: shell-session

   setcap cap_sys_rawio=ep bscrypto
   setcap cap_sys_rawio=ep bareos-sd

Check the setting with

.. code-block:: shell-session

   getcap -v bscrypto
   getcap -v bareos-sd

:command:`getcap` and :command:`setcap` are part of libcap-progs.

If :command:`bareos-sd` does not have the appropriate capabilities, all other tape operations may still work correctly, but you will get "Unable to perform SG\_IO ioctl" errors.

Solaris (USCSI ioctl interface):


The user running the storage daemon needs the following additional privileges: :index:`\ <single: Platform; Solaris; Privileges>`\

-  :strong:`PRIV_SYS_DEVICES` (see privileges(5))

If you are running the storage daemon as another user than root (which has the :strong:`PRIV_SYS_DEVICES` privilege), you need to add it to the current set of privileges. This can be set up by setting this either as a project for the user, or as a set of extra privileges in the SMF definition starting the storage daemon. The SMF setup is the cleanest one.

For SMF make sure you have something like this in the instance block:

.. code-block:: bareosconfig

   <method_context working_directory=":default"> <method_credential user="bareos" group="bareos" privileges="basic,sys_devices"/> </method_context>

Changes in bareos-sd.conf
'''''''''''''''''''''''''

-  Set the Key Encryption Key

   -  :config:option:`sd/director/KeyEncryptionKey`\  = :strong:`passphrase`

-  Enable the loading of storage daemon plugins

   -  :config:option:`sd/storage/PluginDirectory`\  = :file:`path_to_sd_plugins`

-  Enable the SCSI encryption option

   -  :config:option:`sd/device/DriveCryptoEnabled`\  = yes

-  Enable this, if you want the plugin to probe the encryption status of the drive when it needs to clear a pending key

   -  :config:option:`sd/device/QueryCryptoStatus`\  = yes

Changes in bareos-dir.conf
''''''''''''''''''''''''''

-  Set the Key Encryption Key

   -  :config:option:`dir/director/KeyEncryptionKey`\  = :strong:`passphrase`

Testing
^^^^^^^

Restart the Storage Daemon and the Director. After this you can label new volumes with the encrypt option, e.g.

.. code-block:: bareosconfig

   label slots=1-5 barcodes encrypt

Disaster Recovery
^^^^^^^^^^^^^^^^^

For Disaster Recovery (DR) you need the following information:

-  Actual bareos-sd.conf with config options enabled as described above, including, among others, a definition of a director with the Key Encryption Key used for creating the encryption keys of the volumes.

-  The actual keys used for the encryption of the volumes.

This data needs to be availabe as a so called crypto cache file which is used by the plugin when no connection to the director can be made to do a lookup (most likely on DR).

Most of the times the needed information, e.g. the bootstrap info, is available on recently written volumes and most of the time the encryption cache will contain the most recent data, so a recent copy of the :file:`bareos-sd.<portnr>.cryptoc` file in the working directory is enough most of the time. You can also save the info from database in a safe place and use bscrypto to populate this info (VolumeName |rarr| EncryptKey) into the crypto cache file used by
:command:`bextract` and :command:`bscan`. You can use :command:`bscrypto` with the following flags to create a new or update an existing crypto cache file e.g.:

.. code-block:: shell-session

   bscrypto -p /var/lib/bareos/bareos-sd.<portnr>.cryptoc

-  A valid BSR file containing the location of the last safe of the database makes recovery much easier. Adding a post script to the database save job could collect the needed info and make sure its stored somewhere safe.

-  Recover the database in the normal way e.g. for postgresql:

   .. code-block:: shell-session

      bextract -D <director_name> -c bareos-sd.conf -V <volname> \ /dev/nst0 /tmp -b bootstrap.bsr
      /usr/lib64/bareos/create_bareos_database
      /usr/lib64/bareos/grant_bareos_privileges
      psql bareos < /tmp/var/lib/bareos/bareos.sql

Or something similar (change paths to follow where you installed the software or where the package put it).

**Note:** As described at the beginning of this chapter, there are different types of key management, AME, LME and KMA. If the Library is set up for LME or KMA, it probably won’t allow our AME setup and the scsi-crypto plugin will fail to set/clear the encryption key. To be able to use AME you need to "Modify Encryption Method" and set it to something like "Application Managed". If you decide to use LME or KMA you don’t have to bother with the whole setup
of AME which may for big libraries be easier, although the overhead of using AME even for very big libraries should be minimal.

scsitapealert-sd
~~~~~~~~~~~~~~~~

:index:`\ <single: Plugin; scsitapealert-sd>`\

This plugin is part of the **bareos-storage-tape** package.

python-sd Plugin
~~~~~~~~~~~~~~~~

:index:`\ <single: Plugin; Python; Storage Daemon>`\

The **python-sd** plugin behaves similar to the :ref:`director-python-plugin`.

.. _dirPlugins:

Director Plugins
----------------

.. _director-python-plugin:

python-dir Plugin
~~~~~~~~~~~~~~~~~

:index:`\ <single: Plugin; Python; Director>`\

The **python-dir** plugin is intended to extend the functionality of the Bareos Director by Python code. A working example is included.

-  install the **bareos-director-python-plugin** package

-  change to the Bareos plugin directory (:file:`/usr/lib/bareos/plugins/` or :file:`/usr/lib64/bareos/plugins/`)

-  copy :file:`bareos-dir.py.template` to :file:`bareos-dir.py`

-  activate the plugin in the Bareos Director configuration

-  restart the Bareos Director

-  change :file:`bareos-dir.py` as required

-  restart the Bareos Director

Loading plugins
^^^^^^^^^^^^^^^

Since :sinceVersion:`14.4.0: multiple Python plugins` multiple Python plugins can be loaded and plugin names can be arbitrary. Before this, the Python plugin always loads the file :file:`bareos-dir.py`.

The director plugins are configured in the Job-Resource (or JobDefs resource). To load a Python plugin you need

-  pointing to your plugin directory (needs to be enabled in the Director resource, too

-  Your plugin (without the suffix .py)

-  default is ’0’, you can leave this, as long as you only have 1 Director Python plugin. If you have more than 1, start with instance=0 and increment the instance for each plugin.

-  You can add plugin specific option key-value pairs, each pair separated by ’:’ key=value

Single Python Plugin Loading Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Single Python Plugin Loading Example

   Director {
     # ...
     # Plugin directory
     Plugin Directory = /usr/lib64/bareos/plugins
     # Load the python plugin
     Plugin Names = "python"
   }

   JobDefs {
     Name = "DefaultJob"
     Type = Backup
     # ...
     # Load the class based plugin with testoption=testparam
     Dir Plugin Options = "python:instance=0:module_path=/usr/lib64/bareos/plugins:module_name=bareos-dir-class-plugins:testoption=testparam
     # ...
   }

Multiple Python Plugin Loading Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Multiple Python Plugin Loading Example

   Director {
     # ...
     # Plugin directory
     Plugin Directory = /usr/lib64/bareos/plugins
     # Load the python plugin
     Plugin Names = "python"
   }

   JobDefs {
     Name = "DefaultJob"
     Type = Backup
     # ...
     # Load the class based plugin with testoption=testparam
     Dir Plugin Options = "python:instance=0:module_path=/usr/lib64/bareos/plugins:module_name=bareos-dir-class-plugins:testoption=testparam1
     Dir Plugin Options = "python:instance=1:module_path=/usr/lib64/bareos/plugins:module_name=bareos-dir-class-plugins:testoption=testparam2
     # ...
   }

Write your own Python Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some plugin examples are available on https://github.com/bareos/bareos-contrib. The class-based approach lets you easily reuse stuff already defined in the baseclass BareosDirPluginBaseclass, which ships with the **bareos-director-python-plugin** package. The examples contain the plugin bareos-dir-nsca-sender, that submits the results and performance data of a backup job directly to Icinga:index:`\ <single: Icinga>`\  or
Nagios:index:`\ <single: Nagios|see{Icinga}>`\  using the NSCA protocol.



