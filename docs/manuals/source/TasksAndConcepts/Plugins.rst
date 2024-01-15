.. _section-plugins:

Plugins
=======

.. index:: Plugin

The functionality of Bareos can be extended by plugins. Plugins exist
for the different daemons (Director, Storage- and File-Daemon).

To use plugins, they must be enabled in the configuration (:strong:`Plugin Directory`\  and optionally :strong:`Plugin Names`\ ).

If a :strong:`Plugin Directory`\  is specified, :strong:`Plugin Names`\  defines which plugins get loaded.

If :strong:`Plugin Names`\  is not defined, all plugins found in the :strong:`Plugin Directory` are loaded.

The program :ref:`bpluginfo` can be used to retrieve information about a specific plugin.


.. _section-Python Plugins:

Python Plugins
--------------

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

An example for such Python Plugins is the VMware Python Plugin.

With Bareos :sinceVersion:`23: Python API`,
the support of Python version *2* (which is end-of-life since Jan 1 2020) was removed.

The following plugins exist:

.. table:: Bareos Python plugins
   :widths: auto

   ===============  ============
    Python Version  Python 3
   ===============  ============
    |fd|            python3-fd
    |sd|            python3-sd
    |dir|           python3-dir
   ===============  ============

For implementation details see :ref:`section-Python Plugin API`.

Switching to Python 3
~~~~~~~~~~~~~~~~~~~~~

Switching to use the Python 3 plugin, the following needs to be changed:

  * Set `Plugin Names = "python3"` to make sure the Python3 plugin is loaded.
  * Adapt the Plugin setting in the fileset to use Python3: `Plugin = "python3:module_name=..."`


Recovering old backups
^^^^^^^^^^^^^^^^^^^^^^
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

.. include:: Plugins/DirectorPlugins.rst.inc
.. include:: Plugins/StorageDaemonPlugins.rst.inc
.. include:: Plugins/FileDaemonPlugins.rst.inc
