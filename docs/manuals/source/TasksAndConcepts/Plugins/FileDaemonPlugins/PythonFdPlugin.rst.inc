.. _section-python-fd Plugin:

python-fd Plugin
~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Python; File Daemon

The **python-fd** plugin behaves similar to the :ref:`director-python-plugin`.
Configuration is done in the :ref:`DirectorResourceFileSet` on the |dir|.

To load a Python plugin you need

module_name
    The file (or directory) name of your plugin (without the suffix .py)
module_path
    Plugin path (optional, only required when using non default paths)

Plugin specific options can be added as key-value pairs, each pair separated by ’:’ key=value.



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
         Signature = MD5
         Plugin = "python3"
                  ":module_path=/usr/lib/bareos/plugins"
                  ":module_name=bareos_option_example"
       }
       File = "/etc"
       File = "/usr/lib/bareos/plugins"
     }
   }

This plugin from https://github.com/bareos/bareos/tree/master/contrib/fd-plugins/bareos_option_example has a method that is called before and after each file that goes into the backup, it can be used as a template for whatever plugin wants to interact with files before or after backup.
