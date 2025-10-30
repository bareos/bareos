.. _section-yuzuy-qumulo Plugin:

Qumulo Python by Yuzuy (Third Party)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Yuzuy
   single: Qumulo
   single: Plugin; Qumulo
   single: Plugin; Third Party

The |fd| Yuzuy Qumulo plugin is a third party python plugin allowing
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

- Bareos :sinceVersion:`22.0.0: Qumulo Plugin`
- Qumulo >= `4.0.0`

Using the Qumulo plugin can reduce the run time of backup jobs by magnitudes.

Learn more about `Qumulo <https://qumulo.com/>`_
