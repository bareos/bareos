.. _GrpcPlugin:

Grpc Plugin
~~~~~~~~~~~

.. index::
   pair: Plugin; Grpc

The grpc plugin is a plugin that allows you to run a separate executable as a bareos plugin.
This executable talks to the core via [grpc](https://grpc.io) remote procedure calls.

This has multiple upsides for users, such as:

   * A crash inside a plugin will not also crash the daemon
   * It eliminates some classes of concurrency problems related to the
     use of global state inside plugins

This plugin on its own is not very useful.  Its only a bridge between the bareos core and
the actual plugin doing the work.

The plugin comes with two executables, `grpc-test-module` and `grpc-python-module`, which allow you
to make use of this bridge.
As the name suggests, `grpc-test-module` is a simple module that can be used to test that the bridge is working.
`grpc-python-module` on the other hand is a handy little executable that can be used to load and run
normal bareos plugins (including python plugins) in a separate process.

Status of the Grpc Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^
This plugin is still in an **experimental** phase.  The API between core and plugins may change at any time.


Installation of the Grpc Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The grpc plugin, together with the `grpc-test-module` and `grpc-python-module`, can be installed with the
**bareos-filedaemon-gprc-plugin** package.

Configuration of the Grpc Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The Grpc Plugin receives as first argument the name of the executable that it should execute.
This executable is assumed to be in the normal bareos plugin directory.

.. _GrpcPythonPlugin:

Grpc Python Plugin
~~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; Grpc
   single: Grpc Python Plugin

The Grpc Python Plugin is a Grpc Plugin that can be used to start the
:ref:`section-python-fd Plugin` (and other Bareos plugins) in
a separate process.  This ensures that no python state is shared between
different jobs.

.. uml::
   :caption: a quick overview of the architecture

   @startuml
   package "Bareos File Daemon Process" {
     [core] <-> [grpc-fd] : Bareos Plugin API
   }
   package "Inferior Process" {
     [grpc-python-module] <-> [python-fd] : Bareos Plugin API
     [python-fd] <-> [python files] : Python Plugin API
   }
   [grpc-fd] <--> [grpc-python-module] : Grpc Plugin API
   @enduml

This plugin does not take any options, but expects its arguments to be a
valid python-fd plugin definition.

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
       Plugin = "grpc-fd"
                ":grpc-python-module"
                ":python3"
                ":module_name=pyplug"
                ":arg1=val1"
     }
   }

This fileset will call the python-fd plugin `pyplug` with the argument
`arg1=val1` in a separate process.
