.. _fdPlugins:

File Daemon Plugins
-------------------

File Daemon plugins are configured by the :strong:`Plugin` directive of a :ref:`File Set <directive-fileset-plugin>`.

.. warning::

   Currently the plugin command is being stored as part of the backup. The restore command in your directive should be flexible enough if things might change in future, otherwise you could run into trouble.




.. include:: Plugins/FileDaemonPlugins/ApacheLibcloudPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/BpipePlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/GlusterFSPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/LDAPPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/MariaDBmariabackupPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/MSSqlPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/MySQLPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/PerconaXtraBackupPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/PostgreSQLPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/PythonFdPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/VMwarePlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/GrpcPlugin.rst.inc
.. include:: Plugins/FileDaemonPlugins/HyperVPlugin.rst.inc
