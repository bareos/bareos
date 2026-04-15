.. _section-Python Plugin API:

Python Plugin API
-----------------

.. index::
   pair: Plugin; Python

A special case of the Bareos Plugins are the Python Plugins.
The Python plugins create a connection between the Bareos Plugin API and the
Python programming language. With the Python plugins, it is possible to implement
Bareos Plugins by writing Python code.

For each daemon there exists a Daemon Python Plugin which is a plugin implementing
the C API for Bareos plugins,
see :ref:`section-python-fd Plugin`, :ref:`section-python-sd Plugin` and :ref:`section-python-dir Plugin`.

This Python plugin can be configured via the usual plugin configuration mechanism
which python files to load. The python files then implement the plugin
functionality.

For Bareos :sinceVersion:`23: Python API`, the support for Python version *2*
(end-of-live since January 1, 2020) was removed.


Description of the Bareos Python plugin API for Bareos >= 26
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following plugins now exist:

.. table:: Bareos Python plugins
   :widths: auto

   =============== ============
    Python Version Python 3
   =============== ============
    |fd|           python3-fd
    |sd|           python3-sd
    |dir|          python3-dir
   =============== ============

Each plugin exposes the bareos api to the python plugin via an module
with the name *bareos[fd|sd|dir]*, for example **bareosfd**.

For Bareos :sinceVersion:`26: Python API` only the **bareosfd** module
is implemented via an python extension module, i.e. an dll.
The other modules are created during runtime.

.. uml::
  :caption: Bareos Python Plugin Architecture for Bareos >= 26

  package "Bareos Daemon" {
  [Core]
  }

  package "Daemon Python Plugin (shared library)" {
  [Python Interpreter]
  }

  package "Python Module (internal)" {
  [Python Module]
  }

  package "Python Plugin Scripts" {
  [Python Plugin Files]
  }

  [Core] <-> [Python Interpreter] : Bareos Plugin API

  [Python Interpreter] <-> [Python Module] :  load and use
  [Python Module] <-> [Python Plugin Files] : Python Plugin API



The Python plugin creates a Python interpreter with Python 3
which then loads or creates the corresponding Python module. Afterwards the interpreter
loads the Python script configured in the *Plugin* fileset setting and executes
it.

To access these values, every Python plugin imports the corresponding Python
module and can access the variables immediately.

.. code-block:: python
   :caption: bareosfd: accessing compiled-in constants:

   import bareosfd
   ...
   return bareos_fd.bRC_OK

The constants used in the bareos function calls are available both in the
corresponding module as well as in dictionaries inside that module.
E.g. the **bVarJobId** constant can be accessed both from the **bareosfd** module
as well as the **bVariable** dictionary.

This means that you can see all functions and constants that the module exports
by simply doing a **dir()** call on that module.

.. code-block:: python
   :caption: bareosfd: show constants

   import bareosfd
   dir(bareosfd)
   ['AcceptFile', 'AclPacket', 'AddExclude', 'AddInclude', 'AddOptions',
   'AddRegex', 'AddWild', 'CF_CORE', 'CF_CREATED', 'CF_ERROR', 'CF_EXTRACT',
   'CF_SKIP', 'CheckChanges', 'ClearSeenBitmap', 'DebugMessage', 'FT_BASE',
   'FT_DELETED', 'FT_DIRBEGIN', 'FT_DIREND', 'FT_DIRNOCHG', 'FT_FIFO',
   'FT_INVALIDDT', 'FT_INVALIDFS', 'FT_ISARCH', 'FT_JUNCTION', 'FT_LNK',
   'FT_LNKSAVED', 'FT_NOACCESS', 'FT_NOCHG', 'FT_NOFOLLOW', 'FT_NOFSCHG',
   'FT_NOOPEN', 'FT_NORECURSE', 'FT_NOSTAT', 'FT_PLUGIN', 'FT_PLUGIN_CONFIG',
   'FT_PLUGIN_CONFIG_FILLED', 'FT_RAW', 'FT_REG', 'FT_REGE', 'FT_REPARSE',
   'FT_RESTORE_FIRST', 'FT_SPEC', 'GetInstanceCount', 'GetValue', 'IO_CLOSE',
   'IO_OPEN', 'IO_READ', 'IO_SEEK', 'IO_WRITE', 'IoPacket', 'JobMessage',
   'L_BASE', 'L_DIFFERENTIAL', 'L_FULL', 'L_INCREMENTAL', 'L_NONE', 'L_SINCE',
   'L_VERIFY_CATALOG', 'L_VERIFY_DATA', 'L_VERIFY_DISK_TO_CATALOG',
   'L_VERIFY_INIT', 'L_VERIFY_VOLUME_TO_CATALOG', 'L_VIRTUAL_FULL', 'M_ABORT',
   'M_ALERT', 'M_DEBUG', 'M_ERROR', 'M_ERROR_TERM', 'M_FATAL', 'M_INFO',
   'M_MOUNT', 'M_NOTSAVED', 'M_RESTORED', 'M_SAVED', 'M_SECURITY', 'M_SKIPPED',
   'M_TERM', 'M_VOLMGMT', 'M_WARNING', 'NewInclude', 'NewOptions',
   'NewPreInclude', 'RegisterEvents', 'RestoreObject', 'RestorePacket',
   'SavePacket', 'SetSeenBitmap', 'SetValue', 'StatPacket', 'UnRegisterEvents',
   'XattrPacket', '_C_API', '__doc__', '__file__', '__loader__', '__name__',
   '__package__', '__spec__', 'bCFs', 'bEventBackupCommand',
   'bEventCancelCommand', 'bEventEndBackupJob', 'bEventEndFileSet',
   'bEventEndRestoreJob', 'bEventEndVerifyJob', 'bEventEstimateCommand',
   'bEventHandleBackupFile', 'bEventJobEnd', 'bEventJobStart', 'bEventLevel',
   'bEventNewPluginOptions', 'bEventOptionPlugin', 'bEventPluginCommand',
   'bEventRestoreCommand', 'bEventRestoreObject', 'bEventSince',
   'bEventStartBackupJob', 'bEventStartRestoreJob', 'bEventStartVerifyJob',
   'bEventType', 'bEventVssBackupAddComponents', 'bEventVssBackupComplete',
   'bEventVssCloseRestore', 'bEventVssCreateSnapshots',
   'bEventVssInitializeForBackup', 'bEventVssInitializeForRestore',
   'bEventVssPrepareForBackup', 'bEventVssPrepareSnapshot',
   'bEventVssRestoreLoadComponentMetadata',
   'bEventVssRestoreSetComponentsSelected', 'bEventVssSetBackupState',
   'bFileType', 'bIOPS', 'bJobMessageType', 'bLevels', 'bRC_Cancel',
   'bRC_Core', 'bRC_Error', 'bRC_More', 'bRC_OK', 'bRC_Seen', 'bRC_Skip',
   'bRC_Stop', 'bRC_Term', 'bRCs', 'bVarAccurate', 'bVarClient',
   'bVarDistName', 'bVarExePath', 'bVarFDName', 'bVarFileSeen', 'bVarJobId',
   'bVarJobName', 'bVarJobStatus', 'bVarLevel', 'bVarPrefixLinks',
   'bVarPrevJobName', 'bVarRegexWhere', 'bVarSinceTime', 'bVarType',
   'bVarVersion', 'bVarVssClient', 'bVarWhere', 'bVarWorkingDir', 'bVariable']



Direct I/O option in Python plugin API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The Bareos Plugin API calls the plugin also to do the I/O (*read* and *write*)
of the data being backed up. The read operation during backup and the write
operation during restore are the API calls that are by far most frequently
called of all API calls.

The standard way of operation is that the API calls the ``plugin_io()`` function
provided by the python plugin which reads the data from the source and passes
this data back to the Bareos core as ``bytearray`` on backup gets the data back in
a ``bytearray`` from the core to write it back during restore.
While this procedure implements the full Bareos plugin logic, it is not very
efficient as the data needs to be moved between the python interpreter and the
Bareos core and can slow down performance of backups and restores significantly.


The solution to this problem is doing the **I/O operation directly in the core**.

If the python plugin is able to provide a **valid filedescriptor** to the plugin
core, the I/O operation now can be done by the Bareos core itself by **directly
reading from/writing to the provided filedescriptor** instead of calling the
``plugin_io()`` function every time. Doing so, the overall backup performance of
file backups through the python plugin interface **reaches the same speed as a
native file backup** without plugin involved.

To adapt an existing plugin for doing direct I/O, the plugin in the function
`plugin_io_open()` needs to set `IOP.status` to `bareosfd.iostat_do_in_core`
and the `filedes` needs to be set to the valid filedescriptor in the
`IoPacket`.
To let the plugin do the I/O, just set `IOP.status` to
`bareosfd.iostat_do_in_plugin`:

.. code-block:: python
   :caption: enable direct I/O in python plugins

            if I_want_io_in_core:
                # do io in core
                IOP.filedes = self.file.fileno()
                IOP.status =  bareosfd.iostat_do_in_core
            else:
                #  do io in plugin
                IOP.status = bareosfd.iostat_do_in_plugin

Using large lists may cause performance issues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using large lists in Python can have a significant performance impact as it incurs O(n) memory movement costs for ``pop(0)`` and ``insert(0, v)`` operations. Plugins which use large lists should use ``collections.deque`` instead as its ``popleft()`` and ``appendleft()`` functions perform well. If possible it is even better to use a generator to also decrease the memory footprint.

For more details see `Python collections.deque documentation <https://docs.python.org/3/library/collections.html#collections.deque>`
