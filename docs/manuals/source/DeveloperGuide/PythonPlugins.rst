Python Plugins
--------------

.. index::
   pair: Plugin; Python

A special case of the Bareos Plugins are the Python Plugins.
The Python plugins create a connection between the Bareos Plugin API and the
Python programming language. With the Python plugins, it is possible to implement
Bareos Plugins by writing Python code.

For each daemon there exists a Daemon Python Plugin which is a plugin implementing
the C API for Bareos plugins.

This Python plugin can be configured via the usual plugin configuration mechanism
which python files to load. The python files then implement the plugin
functionality.

Examples for such Python Plugins are the VMware Python Plugin or the oVirt Python Plugin.


Modernization of the Python plugin API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For Bareos :sinceVersion:`20: Python API`, the Bareos Python API was refactored and
adapted to support both Python version *2* and Python version *3*.

Description of the Bareos Python plugin API for Bareos < 20
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In  Bareos < 20, the Bareos Python plugin API consists of a Bareos daemon
plugin (**python-fd**, **python-sd**, **python-dir**). These plugins are
shared objects that are loaded by the corresponding daemon during startup.


.. uml::
  :caption: Bareos Python Plugin Architecture for Bareos < 20

  package "Bareos Daemon" {
  [Core]
  }

  package "Daemon Python Plugin (shared library)" {
  [Python Extension Module]
  [Python Interpreter]
  }

  package "Python Plugin Scripts" {
  [Python Plugin Files]
  [Python Constants File]
  }

  [Core] <-> [Python Interpreter] : Bareos Plugin API

  [Python Interpreter] <-> [Python Extension Module] : use
  [Python Extension Module] <-> [Python Plugin Files] : Python Plugin API
  [Python Plugin Files] -> [Python Constants File] : imports


This plugin then creates an **internal Python extension module** and starts a **Python 2**
interpreter being able to access the Python module.

The **internal Python extension module** allows the Python plugin to call functions
implemented in Python, and implements callback functions that can be called
from the Python code into the core. It also implements the data types that are
exchanged via the Bareos plugin interface.

Finally, the Python interpreter loads the Python script configured in the
**Plugin string** of the file set and executes it. This Python script is the
Bareos plugin implemented in Python.

As the **internal Python extension module** is only created inside of the Python
plugin, debugging and testing is a challenge.

Definitions of constants required for the Python plugins callbacks into the
Bareos core are stored in a Python file called
*bareos_[fd|sd|dir]_consts*, for example *bareos_fd_consts.py*.

This file contained the required definitions in form of dictionaries, like the
following example:

.. code-block:: python
   :caption: bareos_fd_consts: definition as dictionary

    bRCs = dict(
        bRC_OK=0,
        bRC_Stop=1,
        bRC_Error=2,
        bRC_More=3,
        bRC_Term=4,
        bRC_Seen=5,
        bRC_Core=6,
        bRC_Skip=7,
        bRC_Cancel=8,
    )


To access these values, every Python plugin needs to import this file and
access the values via the dictionary like in the following example:

.. code-block:: python
   :caption: bareos_fd_consts: accessing

   import bareos_fd_consts
   ... # more code
   return bareos_fd_consts.bRCs["bRC_OK"]


The API also always carries a **context** variable which is part of
every function call between the Bareos core and Python, so that every function
being called from the core has an context which needs to be given back to every
call that goes into the core.


Description of the new Bareos Python plugin API for Bareos >= 20
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Since Bareos :sinceVersion:`20: Python3`, two Python plugins exist for
each Bareos daemon, where the **python-** prefix means that the plugin supports
Python 2, and the **python3-** prefix supports Python 3.

The following six plugins now exist:

.. table:: Bareos Python plugins
   :widths: auto

   ===============  =========== ============
    Python Version  Python 2    Python 3
   ===============  =========== ============
    filedaemon      python-fd   python3-fd
    storage daemon  python-sd   python3-sd
    director        python-dir  python3-dir
   ===============  =========== ============

The functionality of the former *internal Python extension module* is now implemented
as real Python extension module with the name *bareos[fd|sd|dir]*, for example
**bareosfd**.
Every Python plugin now has a corresponding Python extension module.

.. uml::
  :caption: Bareos Python Plugin Architecture for Bareos >= 20

  package "Bareos Daemon" {
  [Core]
  }

  package "Daemon Python Plugin (shared library)" {
  [Python Interpreter]
  }

  package "Python Extension Module (shared object)" {
  [Python Extension Module]
  }

  package "Python Plugin Scripts" {
  [Python Plugin Files]
  }

  [Core] <-> [Python Interpreter] : Bareos Plugin API

  [Python Interpreter] <-> [Python Extension Module] :  load and use
  [Python Extension Module] <-> [Python Plugin Files] : Python Plugin API



The Python plugin creates a Python interpreter with either Python 2 or Python 3
which then loads the corresponding Python extension module. Afterwards the interpreter
loads the Python script configured in the *Plugin* fileset setting and executes
it.

As the Python extension module for the Python Plugin is now available outside of the
Daemon Python Plugin. It is now a real stand-alone Python extension module implemented in
C which can be loaded and tested independently.

Definitions required for the Python plugin callbacks into the Bareos core
are now **compiled into** the *bareos[fd|sd|dir]* Python extension module, and the
Python files containing the constant definitions have been removed.

To access these values, every Python plugin imports the corresponding Python
extension module and can access the variables immediately.

.. code-block:: python
   :caption: bareosfd: accessing compiled-in constants:

   import bareosfd
   ...
   return bareos_fd.bRC_OK

The dictionaries used before still exist, but every value that was stored in
the dictionaries before is also available directly in the **bareosfd**
namespace. To see what is available in the plugin you can import the python
extension module and use the **dir()** function:

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



During the restructuring of the plugin API it became clear that the *context*
that was always transferred between the core and the Python Plugin and back was
**unnecessary**, so it was completely removed from the API.

The members of the **bareosfd.stat_pkt** type have been renamed with the
**st_** prefix to match with the member names of Pythons' **os.stat_result**.

Porting existing Python plugins
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Porting existing Python plugin from Version 19 to 20 requires is not very hard
and requires the following steps:

   Remove "context" everywhere
      The context being transferred between all Python API calls was unnecessary
      and was removed, so it needs to be removed in all Python callers and callbacks.

   Switch to the constants being defined in the bareos[fd|sd|dir] Python plugin.
      Import bareos[fd|sd|dir] instead of bareos_[fd|sd|dir]_consts.

   Add the **st_** prefix to all members of **stat_pkt** type.

   Adapt the code to run on Python 2 **and** Python 3
      It is important to make sure the code works both for Python 2 and 3.
      While the c++ code and the Python api have been reorganized, the Python
      plugin code itself is the same being run with Python 2 or 3.  Existing
      plugins have been ported and the current Python 2 version support
      already a lot of things required by Python 3.
      For more details see `Python 3 Porting Guide, strings chapter: <https://portingguide.readthedocs.io/en/latest/strings.html>`_



Switching to Python 3
^^^^^^^^^^^^^^^^^^^^^

Switching to use the Python 3 plugin, the following needs to be changed:
  * Set `Plugin Names = "python3"` to make sure the Python3 plugin is loaded.
  * Adapt the Plugin setting in the fileset to use Python3: `Plugin = "python3:module_path ...`

.. warning::

   It is not possible to load the python2 and python3 plugins at the same time.


Recovering old backups
^^^^^^^^^^^^^^^^^^^^^^^
When doing backups, the plugin parameter string is stored into the backup stream.
During restore, this string is used to determine the plugin that will handle this
data.

To be able to restore backups created with Python plugins using the
**python3-fd** plugin that were created using the **python-fd** plugin,
the code determining the plugin that will handle the data also matches for
the basename of the current available plugins without the last character.

So backups created with the **python** plugin (which uses Python 2) can be restored
with the **python3** plugin (which uses Python 3).

.. warning::

   It is not possible to use the python plugin to restore backups created with
   the python3 plugin. Once switched, you need to stay on python3.
