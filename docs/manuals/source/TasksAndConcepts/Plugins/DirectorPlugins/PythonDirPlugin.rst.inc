.. _director-python-plugin:

.. _section-python-dir Plugin:

python-dir Plugin
~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Python; Director

The **python-dir** (or **python3-dir**) plugin is intended
to extend the functionality of the |dir| by Python code.
Configuration:

-  install the **bareos-director-python-plugin** package
-  configure the :config:option:`dir/director/PluginDirectory`
-  activate the plugin in the |dir| configuration (:config:option:`dir/director/PluginNames`)
-  configure the plugin via :config:option:`dir/job/DirPluginOptions`
-  restart the |dir|

The director plugins are configured in the :config:option:`dir/job/DirPluginOptions` (or JobDefs resource).
To load a Python plugin you need




instance
   default is ’0’, you can leave this, as long as you only have 1 Director Python plugin. If you have more than 1, start with instance=0 and increment the instance for each plugin.

module_name
    The file (or directory) name of your plugin (without the suffix .py)
module_path
    Plugin path (optional, only required when using non default paths)

Plugin specific options can be added as key-value pairs, each pair separated by ’:’ key=value.

Single Director Python Plugin Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Single Python Plugin Loading Example

   Director {
     # ...
     # Plugin directory
     Plugin Directory = /usr/lib64/bareos/plugins
     # Load the python plugin
     Plugin Names = "python3"
   }

   JobDefs {
     Name = "DefaultJob"
     Type = Backup
     # ...
     # Load the class based plugin with testoption=testparam
     Dir Plugin Options = "python3"
                          ":instance=0"
                          ":module_name=bareos-dir-class-plugins"
                          ":testoption=testparam"
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
     Plugin Names = "python3"
   }

   JobDefs {
     Name = "DefaultJob"
     Type = Backup
     # ...
     # Load the class based plugin with testoption=testparam
     Dir Plugin Options = "python3"
                          ":instance=0"
                          ":module_name=bareos-dir-class-plugins"
                          ":testoption=testparam1"
     Dir Plugin Options = "python3"
                          ":instance=1"
                          ":module_name=bareos-dir-class-plugins"
                          ":testoption=testparam2"
     # ...
   }

Write your own Python Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The class-based approach lets you easily reuse code already defined in the Python base class,
which ships with the **bareos-director-python-plugin** package.

Some plugin examples are available on https://github.com/bareos/bareos/tree/master/contrib/dir-plugins,
e.g. the  plugin bareos-dir-nsca-sender,
that submits the results and performance data of a backup job directly
to Icinga :index:`\ <single: Icinga>`\ 
or Nagios :index:`\ <single: Nagios|see{Icinga}>`\  using the NSCA protocol.
