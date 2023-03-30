.. _mariabackupPlugin:
.. _backup-MariaDB-mariabackup:

MariaDB mariabackup Plugin
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; MariaDB Backup
   single: mariabackup

This plugin uses the tool :command:`mariabackup` to make full and incremental backups of |mariadb| databases. :command:`mariabackup` is part of the standard mariadb installation.

Documentation of mariabackup is available online: https://mariadb.com/kb/en/mariabackup/.

It is stable since MariaDB 10.1.48


Prerequisites of mariabackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The :command:`mariabackup` binary needs to be installed on the |fd|. refer to previous documentation link.

For authentication the :file:`.mycnf` file of the user running the |fd| is used. Before proceeding, make sure that mariabackup can connect to the database, create backups and is able to restore.


Installation of mariabackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Make sure you have met the prerequisites, then install the package **bareos-filedaemon-mariabackup-python-plugin**.

Configuration of mariabackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
   :caption: bareos-dir.d/fileset/mariadb.conf

   FileSet {
       Name = "mariadb"
       Include  {
           Options {
               signature = MD5
           }
           #...
           Plugin = "python"
                    ":module_name=bareos-fd-mariabackup"
                    ":mycnf=/root/.my.cnf"
       }
   }

The plugin will call :command:`mariabackup` to create a backup stream of all databases in the xbstream format. This stream will be processed by Bareos. Full backups can be made for all table formats, while incremental backups are only supported for **InnoDB** tables. Incremental backups for other table formats will create a full backup.

You can append options to the plugin call as key=value pairs, separated by ’:’. The following options are available:

-  With :strong:`mycnf` you can make mariabackup use a special mycnf-file with login credentials.

-  :strong:`dumpbinary` lets you modify the default command mariabackup.

-  :strong:`dumpoptions` to modify the options for mariabackup. Default setting is: :command:`--backup --stream=xbstream --extra-lsndir=/tmp/individual_tempdir`

-  :strong:`restorecommand` to modify the command for restore. Default setting is: :command:`mbstream -x -C`

-  :strong:`strictIncremental`: By default (false), an incremental backup will create data even if the Log Sequence Number (LSN) was not increased since last backup. This is to ensure that eventual changes to MYISAM/ARIA/Rocks tables get into the backup. MYISAM/ARIA/Rocks does not support incremental backups, you will always get a full backup of these tables. If set to true, no data will be written into backup, if the LSN was not changed.


Restore with mariabackup Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With the usual Bareos restore mechanism a file-hierarchy will be created on the restore client under the default restore location:

:file:`/tmp/bareos-restores/_mariabackup/`

Each restore job gets an own sub-directory named by its `jobid`, because mariabackup expects an empty directory. In that sub-directory, a new directory is created for every backup job that was part of the Full-Incremental sequence.

The naming scheme is: :file:`fromLSN_toLSN_jobid`

Example:

::

   /tmp/bareos-restores/_mariabackup/656/
   |-- 00000000000000000000_00000000000010129154_0000000604
   |-- 00000000000010129154_00000000000010142295_0000000635
   |-- 00000000000010142295_00000000000010201260_0000000708

This example shows the restore tree for restore job with ID 656. First sub-directory has all files from the first full backup job with ID 604. It starts at LSN 0 and goes until LSN 10129154.

Next line is the first incremental job with ID 635, starting at LSN 10129154 until 10142295. The third line is the 2nd incremental job with ID 708.

To further prepare the restored files, use the :command:`mariabackup --prepare` command. Read https://mariadb.com/kb/en/incremental-backup-and-restore-with-mariabackup/ for more information.

Also our systemtest can serve as example see `systemtests/tests/py2plug-fd-mariabackup/testrunner`


Troubleshooting
'''''''''''''''

If things don't work as expected, make sure that

- the |fd| (FD) works in general, so that you can make simple file backups and restores.
- the Bareos FD Python plugins work in general, try one of the shipped simple sample plugins.
- *mariabackup* works as user root, MariaDB access needs to be configured properly.
