.. _GrpcPlugin:

Grpc Plugin
~~~~~~~~~~~

.. index::
   pair: Plugin; Grpc

The grpc plugin is a plugin that allows you to run a separate executable as a bareos plugin.
This executable talks to the core via `grpc <https://grpc.io/>`_ remote procedure calls.

This has multiple upsides for users, such as:

   * A crash inside a plugin will not also crash the daemon
   * It eliminates some classes of concurrency problems related to the
     use of global state inside plugins

This plugin on its own is not very useful.  Its only a bridge between the bareos core and
the actual plugin doing the work.

The plugin comes with two executables, `grpc-test-module` and `bareos-grpc-fd-plugin-bridge`, which allow you
to make use of this bridge.
As the name suggests, `grpc-test-module` is a simple module that can be used to test that the bridge is working.
`bareos-grpc-fd-plugin-bridge` on the other hand is a handy little executable that can be used to load and run
normal bareos plugins (including python plugins) in a separate process.

Status of the Grpc Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^
This plugin is still in an **experimental** phase.  The API between core and plugins may change at any time.


Installation of the Grpc Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The grpc plugin, together with the `grpc-test-module` and `bareos-grpc-fd-plugin-bridge`, can be installed with the
**bareos-filedaemon-gprc-plugin** package.

Configuration of the Grpc Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The Grpc Plugin receives as first argument the name of the executable that it should execute.
This executable is assumed to be in the normal bareos plugin directory.

.. _GrpcFdPluginBridge:

Grpc Fd Plugin Bridge Module
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Grpc
   single: Grpc Fd Plugin Bridge

The Grpc Fd Plugin Bridge is a grpc module that can be used to start any
:ref:`fdPlugins` in a separate process.  This is especially useful when
used with the :ref:`section-python-fd Plugin` as this ensures that no
state is shared between different jobs.

.. uml::
   :caption: a quick overview of the architecture

   @startuml
   package "Bareos File Daemon Process" {
     [core] <-> [grpc-fd] : Bareos Plugin API
   }
   package "Inferior Process" {
     [bareos-grpc-fd-plugin-bridge] <-> [python-fd] : Bareos Plugin API
     [python-fd] <-> [python files] : Python Plugin API
   }
   [grpc-fd] <--> [bareos-grpc-fd-plugin-bridge] : Grpc Plugin API
   @enduml

This plugin does not take any options, but expects its arguments to be a
valid bareos plugin definition.

Example
^^^^^^^

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/fileset/GrpcPython.conf

   FileSet {
     Name = "GrpcPython"
     Description = "Run a python plugin in a separate process"
     Include {
       Options {
         Signature = XXH128
       }
       Plugin = "grpc"
                ":bareos-grpc-fd-plugin-bridge"
                ":python3"
                ":module_name=pyplug"
                ":arg1=val1"
     }
   }

This fileset will call the python-fd plugin `pyplug` with the argument
`arg1=val1` in a separate process.

.. note::
   We recommend using :ref:`GrpcFallback` instead of explicitly using the grpc plugin
   if you plan on using the :ref:`GrpcFdPluginBridge`.



.. _GrpcFallback:

Grpc Fallback
^^^^^^^^^^^^^

If you want to use the grpc plugin to start normal bareos plugins via the :ref:`GrpcFdPluginBridge`
in a separate process, then we recommend you to use grpc fallback.
In this mode regular plugin directives in your fileset get sent to the grpc plugin _if_ the specified
plugin was not loaded.  E.g. if your filedaemon does *not* load the :ref:`section-python-fd Plugin`
and you try to backup a fileset including a python plugin directive, then the grpc plugin is asked
to take care of it.

The grpc fallback mode is enabled if you dont unset :config:option:`fd/client/GrpcModule`
and load the :ref:`GrpcPlugin` in the fd.

Example (Fallback)
^^^^^^^^^^^^^^^^^^

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/fileset/Python.conf

   FileSet {
     Name = "Python"
     Description = "Run a python plugin in a separate process"
     Include {
       Options {
         Signature = XXH128
       }
       Plugin = "python3"
                ":module_name=pyplug"
                ":arg1=val1"
     }
   }

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-fd.d/client/myself.conf

   Client {
     Name = "fallback-fd"
     Plugin Directory = "/usr/local/lib64/bareos/plugins"
     Plugin Names = "grpc" # Note: we dont load python!
   }

When we now start a backup with the **Python** fileset on the client **fallback-fd**, then
the file daemon will automatically use the grpc plugin and the :ref:`GrpcFdPluginBridge`
to try and load **python-fd.so** and run the backup in a separate process.

This backup will count as a backup done with the python plugin itself, so it can be restored
even on clients which only have access to the python plugin.  This also happens completely
transparently to the director, so it will happily continue the current backup chain, i.e.
it will not force trigger a full backup.

You can verify that the grpc fallback was indeed chosen by looking into the joblog:

.. code-block:: bconsole
   :caption: joblog from grpc output

   [...]
   fallback-fd JobId 1: selected inferior: python3
   fallback-fd JobId 1: loaded inferior: Python File Daemon Plugin
   Version: 4 (May 2020)
   Author: Bareos GmbH & Co. KG
   License: Bareos AGPLv3
   fallback-fd JobId 1: loaded inferior successfully
   [...]
