python-fd Plugin
~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Python; File Daemon

The **python-fd** plugin behaves similar to the :ref:`director-python-plugin`. Base plugins and an example get installed via the package **bareos-filedaemon-python-plugin**. Configuration is done in the :ref:`DirectorResourceFileSet` on the director.



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
         Signature = MD5
       }
       Plugin = "python3"
                ":module_path=/usr/lib/bareos/plugins"
                ":module_name=bareos-fd-mysql"
     }
   }

.. index::
   single: MySQL; Backup

This example uses the :ref:`MySQL plugin <backup-mysql-python>` to backup MySQL dumps.

Option Plugins
^^^^^^^^^^^^^^

Option plugins are activated in the Options resource of a FileSet definition.

Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/option.conf: Python FD option plugins

   FileSet {
     Name = "option"
     Include {
       Options {
         Signature = MD5 # calculate md5 checksum per file
         Plugin = "python"
                  ":module_path=/usr/lib/bareos/plugins"
                  ":module_name=bareos-fd-file-interact"
       }
       File = "/etc"
       File = "/usr/lib/bareos/plugins"
     }
   }

This plugin bareos-fd-file-interact from https://github.com/bareos/bareos/tree/master/contrib/fd-plugins/options-plugin-sample has a method that is called before and after each file that goes into the backup, it can be used as a template for whatever plugin wants to interact with files before or after backup.
