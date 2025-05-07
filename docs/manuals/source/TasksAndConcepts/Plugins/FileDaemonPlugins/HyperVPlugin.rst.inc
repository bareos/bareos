.. _HyperVPlugin:

Hyper-V Plugin
~~~~~~~~~~~~~~

.. index::
   pair: Plugin; HyperV

The Hyper-V Plugin allows you to backup hyper-v virtual machines.  It makes use of RCT (Resilient Change Tracking)
Technology to make extremely fast incremental backups.

Status of the Hyper-V Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
This plugin is still in an **experimental** phase.  The final feature set is not yet set in stone.
In the current testing phase we only support backing up a single local virtual machine per fileset.

.. limitation::
   Currently only disks with a local path can be backed up.
.. limitation::
   Currently vhd disks are unsupported; only vhdx disks are supported.

Configuration of the Hyper-V Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The Hyper-V Plugin receives as its only argument the name of a configuration file.
This configuration file is a json formatted text file containing a single object with the following keys:

- vmname: The name of the vm to be backed up (as a string)
   If the vm name is not unique, then bareos will refuse to continue the backup.

Example
^^^^^^^

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = "C:/Program Files/bareos/plugins"
     Plugin Names = "hyper-v"
     ...
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/hyperv.conf

   FileSet {
     Name = "hyper-v"
     Description = "backup a hyper-v vm via the hyper-v plugin"
     Include {
       Options {
         ...
         Compression = LZ4 # recommended if using dynamic disks
       }
       Plugin = "hyper-v:config=C:\\hyper-v.conf"
     }
   }

.. code-block:: json
   :caption: C:\\hyper-v.conf

   {
     "vmname" : "My Virtual Machine"
   }
