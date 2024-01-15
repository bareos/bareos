.. _section-python-fd Plugin:

python-fd Plugin
~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Python; File Daemon

The **python-fd** plugin behaves similar to the :ref:`director-python-plugin`.
Configuration is done in the :ref:`DirectorResourceFileSet` on the |dir| and in optional configuration files on the |fd|.


Configuration
^^^^^^^^^^^^^
To load a Python plugin you need

module_name
    The file (or directory) name of your plugin (without the suffix .py)
module_path
    Plugin path (optional, only required when using non default paths)

Plugin specific options can be added as key-value pairs, each pair separated by ’:’ key=value.

Configuration Files
'''''''''''''''''''
This plugin can handle additional configuration files for the python-based plugins it wraps.

When supplying one or both of the options `defaults_file` or `overrides_file`, the supplied value will be treated as a path relative to the |fd| configuration.
When using a single configuration file instead of a configuration directory, it will be relative to that file's parent directory.
All configuration files will be read on the |fd| that executes the job.

Depending on how the file was loaded, the options will have different precedence.
When loaded via `defaults_file` the options in the FileSet will override those from the file.
When loaded via `overrides_file` the options from the file will override those in the FileSet and the ones loaded from a `defaults_file`.
In other words: a `defaults_file` provides default values that you can override in your FileSet and an `overrides_file` provides mandatory values that always take precedence.


The configuration files should contain one key-value pair per line that will be used as if they were added to the :config:option:`dir/Fileset/Include/Plugin` option.
Empty lines or lines starting with semicolon (``;``), hash (``#``) or left square bracket (``[``) will be ignored.
Long values can be split across multiple lines by marking the end-of-line with a backslash (``\``).
Finally, whitespace around keys, values or continuation lines is discarded.

.. code-block:: bareosconfig
   :caption: plugin_defaults.ini: python-fd example configuration

   # this is a comment
   ; this is also a comment
   [sections like this will also be ignored]
   
   key=value
   
   # no inline comments
   key=value ; this is not a comment, but part of the value
   
   # whitespace around the key will be ignored
    another_key = another_value
   
   # whitespace around continuation lines will be ignored, too.
   long_value_key = very-long-value-\
                    split-across-lines

   # trailing whitespace of the continued line will be preserved.
   multiline_whitespace = value1 \
                          value2     \
                          value3

.. note::
   It is not possible to pass `module_path` or `module_name` using a configuration file.
   The python plugin will be loaded before the plugin options including the configuration files are handled.

Encoded option values
'''''''''''''''''''''

In some cases it is desireable to have option values in an encoded format.
Every option passed to a Python plugin, can be encoded using the supplied script ``bareos_encode_string.py`` from the scripts directory.
To use such an encoded value, the option-name must be suffixed with ``#enc`` so the plugin knows it needs to decode the value.
Thus, if you previously configured ``api_key=secret_string`` you could now configure ``api_key#enc=b7f<4WprP2baH8KX8``.


Python plugin types
^^^^^^^^^^^^^^^^^^^
We basically distinguish between command-plugin and option-plugins.

Command Plugins
'''''''''''''''

Command plugins are used to replace or extend the FileSet definition in the File Section. If you have a command-plugin, you can use it like in this example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Python FD command plugins

   FileSet {
     Name = "mysql"
     Include {
       Options {
         Signature = XXH128
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
''''''''''''''

Option plugins are activated in the Options resource of a FileSet definition.

Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/option.conf: Python FD option plugins

   FileSet {
     Name = "option"
     Include {
       Options {
         Signature = XXH128
         Plugin = "python3"
                  ":module_path=/usr/lib/bareos/plugins"
                  ":module_name=bareos_option_example"
       }
       File = "/etc"
       File = "/usr/lib/bareos/plugins"
     }
   }

This plugin from https://github.com/bareos/bareos/tree/master/contrib/fd-plugins/bareos_option_example has a method that is called before and after each file that goes into the backup, it can be used as a template for whatever plugin wants to interact with files before or after backup.
