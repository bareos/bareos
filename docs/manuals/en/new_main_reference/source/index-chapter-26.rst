.. _section-plugins:

Plugins
=======

.. index:: Plugin

The functionality of Bareos can be extended by plugins. They do exists plugins for the different daemons (Director, Storage- and File-Daemon).

To use plugins, they must be enabled in the configuration (:strong:`Plugin Directory` and optionally :strong:`Plugin Names`).

If a :strong:`Plugin Directory` is specified :strong:`Plugin Names` defines, which plugins get loaded.

If :strong:`Plugin Names` is not defined, all plugins get loaded.



.. _fdPlugins:

File Daemon Plugins
-------------------

.. toctree::

   chapter26/plugins-fd.rst
   chapter26/plugins-vmware-plugin.rst



.. _sdPlugins:

Storage Daemon Plugins
----------------------
   
.. toctree::

   chapter26/plugins-sd.rst



.. _dirPlugins:

Director Plugins
----------------

.. toctree::
   
   chapter26/plugins-dir.rst
