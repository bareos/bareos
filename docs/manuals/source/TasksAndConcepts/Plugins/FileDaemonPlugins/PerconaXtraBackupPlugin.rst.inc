.. _PerconaXtrabackupPlugin:
.. _backup-mysql-XtraBackup:

Percona XtraBackup Plugin
~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; MySQL Backup
   single: Percona XtraBackup
   single: XtraBackup

This plugin uses Perconas XtraBackup tool, to make full and incremental backups of |mysql| databases.

The key features of XtraBackup are:

- Incremental backups
- Backups that complete quickly and reliably
- Uninterrupted transaction processing during backups
- Savings on disk space and network bandwidth
- Higher uptime due to faster restore time

Incremental backups only work for INNODB tables, when using MYISAM, only full backups can be created.

.. warning::

  In MariaDB 10.1 and later, mariabackup is the recommended backup method to use instead of Percona XtraBackup.
  As such we recommend to use the dedicated plugin for MariaDB.


Prerequisites of percona XtraBackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Install the XtraBackup tool from Percona. Documentation and packages are available here: https://www.percona.com/mysql/software. The plugin was successfully tested with XtraBackup versions 2.3.5 and 2.4.4.

For authentication the :file:`.mycnf` file of the user running the |fd| is used. Before proceeding, make sure that XtraBackup can connect to the database and create backups.


Installation of percona XtraBackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Make sure you have met the prerequisites, after that install the package **bareos-filedaemon-percona_XtraBackup-python-plugin**.

Configuration of percona XtraBackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Activate your plugin directory in the |fd| configuration. See :ref:`fdPlugins` for more about plugins in general.

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = /usr/lib64/bareos/plugins
     Plugin Names = "python3"
   }

Now include the plugin as command-plugin in the Fileset resource:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/mysql.conf

   FileSet {
       Name = "mysql"
       Include  {
           Options {
               Signature = XXH128
           }
           #...
           Plugin = "python"
                    ":module_name=bareos-fd-percona-xtrabackup"
                    ":mycnf=/root/.my.cnf"
       }
   }

If used this way, the plugin will call XtraBackup to create a backup of all databases in the xbstream format. This stream will be processed by Bareos. If job level is incremental, XtraBackup will perform an incremental backup since the last backup – for InnoDB tables. If you have MyISAM tables, you will get a full backup of those.

You can append options to the plugin call as key=value pairs, separated by ’:’. The following options are available:

-  With :strong:`mycnf` you can make XtraBackup use a special mycnf-file with login credentials.

-  :strong:`dumpbinary` lets you modify the default command XtraBackup.

-  :strong:`dumpoptions` to modify the options for XtraBackup. Default setting is: :command:`--backup --datadir=/var/lib/mysql/ --stream=xbstream --extra-lsndir=/tmp/individual_tempdir`

-  :strong:`restorecommand` to modify the command for restore. Default setting is: :command:`xbstream -x -C`

-  :strong:`strictIncremental`: By default (false), an incremental backup will create data, even if the Log Sequence Number (LSN) was not increased since last backup. This is to ensure, that eventual changes to MYISAM tables get into the backup. MYISAM does not support incremental backups, you will always get a full backup of these tables. If set to true, no data will be written into backup, if the LSN was not changed.

Restore with percona XtraBackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With the usual Bareos restore mechanism a file-hierarchy will be created on the restore client under the default restore location:

:file:`/tmp/bareos-restores/_percona/`

Each restore job gets an own subdirectory, because Percona expects an empty directory. In that subdirectory, a new directory is created for every backup job that was part of the Full-Incremental sequence.

The naming scheme is: :file:`fromLSN_toLSN_jobid`

Example:

::

   /tmp/bareos-restores/_percona/351/
   |-- 00000000000000000000_00000000000010129154_0000000334
   |-- 00000000000010129154_00000000000010142295_0000000335
   |-- 00000000000010142295_00000000000010201260_0000000338

This example shows the restore tree for restore job with ID 351. First subdirectory has all files from the first full backup job with ID 334. It starts at LSN 0 and goes until LSN 10129154.

Next line is the first incremental job with ID 335, starting at LSN 10129154 until 10142295. The third line is the 2nd incremental job with ID 338.

To further prepare the restored files, use the :command:`XtraBackup --prepare` command. For more information read https://docs.percona.com/percona-xtrabackup/2.4/backup_scenarios/incremental_backup.html.


Troubleshooting
'''''''''''''''
If things don't work as expected, make sure that

- the |fd| (FD) works in general, so that you can make simple file backups and restores.
- the Bareos FD Python plugins work in general, try one of the shipped simple sample plugins.
- *XtraBackup* works as user root, MySQL access needs to be configured properly.
