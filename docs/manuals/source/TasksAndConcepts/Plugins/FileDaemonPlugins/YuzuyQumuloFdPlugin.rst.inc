.. _section-yuzuy-qumulo Plugin:

Third parties Plugins
~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Third; Party; Plugin; Python; File Daemon;

Here's the list of know third party plugin that extend Bareos
backup and restore capabilities.


Yuzuy Qumolo Python Filedaemon Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: Yuzuy; Qumulo;

The **python-fd** Yuzuy Qumulo plugin is a third party python plugin allowing
easy integration of Qumulo clusters with Bareos. 

You can learn more about features and usage by consulting `Yuzuy Qumulo Backup <https://www.yuzuy.com/#!/backup-integration>`_
dedicated page.

Supported features
''''''''''''''''''

- full backups
- incremental backups
- fast scan for changed data (no tree walks)
- snapshot based backups for consistent file sets
- acl support
- file and folder exclusions
- Virtual Full can be realized with this plugin

Supported versions
''''''''''''''''''

- Bareos :sinceVersion:`22.0.0: Bareos`
- Qumulo :sinceVersion:`4.0.0: Qumulo`


Learn more about `Qumulo <https://qumulo.com/>`_
