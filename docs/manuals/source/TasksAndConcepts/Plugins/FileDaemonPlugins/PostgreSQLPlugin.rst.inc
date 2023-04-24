.. _plugin-postgresql-fd:

PostgreSQL Plugin
~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; PostgreSQL Backup

The PostgreSQL plugin supports an online (Hot) backup of database files and database transaction logs (WAL) archiving. With online database and transaction logs the backup plugin can perform Point-In-Time-Restore up to a single selected transaction or date/time.

This plugin uses the standard API |postgresql| backup routines based on *pg_start_backup()* and *pg_stop_backup()*.

The key features are:

* Incremental backups
* Point in time recovery
* Backups that complete quickly and reliably
* Uninterrupted transaction processing during backups
* Savings on disk space and network bandwidth
* Higher uptime due to faster restore time


Concept
^^^^^^^

Please make sure to read the |postgresql| documentation about the backup and restore process: https://www.postgresql.org/docs/current/continuous-archiving.html

This is just a short outline of the tasks performed by the plugin.

#. Notify |postgresql| that we want to start backup the database files using the *SELECT pg_start_backup()* statement
#. Backup database files
#. Notify |postgresql| when done with file backups using the *SELECT pg_stop_backup()* statement
#. |postgresql| will write *Write-Ahead-Logfiles* (WAL) into the WAL Archive directory. These transaction logs contain transactions done while the file backup proceeded
#. Backup fresh created WAL files

Incremental and Differential backups will only have to backup WAL files created since last reference backup.

The restore basically works like this:

#. Restore all files to the original |postgresql| location
#. Configure |postgresql| for the recovery (see below)
#. Start |postgresql|
#. |postgresql| will restore the latest possible consistent point in time. You can manage to restore to any other point in in time available in the WAL files, please refer to the |postgresql| documentation for more details.


Prerequisites for the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This plugin is a Bareos Python plugin.
It requires |postgresql| >= 9 and the Python module **pg8000** to be installed.

Since :sinceVersion:`21: PostgreSQL Plugin` the plugin was changed to the Python module **pg8000** instead of **psycopg2** and using Python >= 3 is mandatory. The minimum required version of **pg8000** is 1.16. If a distribution provided package exists and is the same or newer version, it can be used. Otherwise it must be installed using the command :command:`pip3 install pg8000`.



The plugin must be installed on the same host where the |postgresql| database runs.

**You have to enable PostgreSQL WAL-Archiving** - the process and the plugin depend on it.

As a minimum this requires that you create an WAL archive directory
and matching settings in your |postgresql| configuration file **postgresql.conf**.
In our examples we assume the WAL archive directory as :file:`/var/lib/pgsql/wal_archive/`.

.. code-block:: cfg
   :caption: postgresql.conf

   ...
   # wal_level default is replica
   wal_level = replica
   archive_mode = on
   archive_command = 'test ! -f /var/lib/pgsql/wal_archive/%f && cp %p /var/lib/pgsql/wal_archive/%f'
   ...

Please refer to the |postgresql| documentation for details.

.. note::

   While the PostgreSQL plugin backups only the required files from the WAL archive directory,
   old files are not removed automatically.


Installation of the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Make sure you have met the prerequisites, after that install the package **bareos-filedaemon-postgres-python-plugin**.


Configuration of the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Activate your plugin directory in the |fd| configuration. See :ref:`fdPlugins` for more about plugins in general.

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
     ...
     Plugin Directory = /usr/lib64/bareos/plugins
     Plugin Names = "python3"
   }

Now include the plugin as command-plugin in the fileset resource and define a job using this fileset:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/postgres.conf

   FileSet {
       Name = "postgres"
       Include  {
           Options {
               Compression = LZ4
               Signature = XXH128
           }
           Plugin = "python"
                    ":module_name=bareos-fd-postgres"
                    ":postgresDataDir=/var/lib/pgsql/data"
                    ":walArchive=/var/lib/pgsql/wal_archive/"
       }
   }



You can append options to the plugin call as key=value pairs, separated by ``:``. The following options are available:

postgresDataDir
   the Postgres data directory. Default: :file:`/var/lib/pgsql/data`

walArchive
   directory where Postgres archives the WAL files as defined in your :file:`postgresql.conf` with the *archive_command* directive. This is a **mandatory** option, there is no default set.

dbuser
   with this user the plugin will try to connect to the database. Default: *root*

dbname
   there needs to be a named database for the connection. Default: *postgres*

dbHost
   useful, if socket is not in default location. Specify socket-directory with a leading / here

ignoreSubdirs
   a list of comma separated directories below the *postgresDataDir*, that will not be backed up. Default: *pg_wal,pg_log,pg_xlog*

switchWal
   If set to *true* (default), the plugin will let Postgres write a new wal file, if the current Log Sequence Number (LSN) is greater than the LSN from the previous job to make sure changes will go into the backup. Default: *true*

switchWalTimeout
   Timeout in seconds to wait for WAL archiving after WAL switch, default 60 seconds.

role
   Set the role used after login, before the first sql call. Default: *None*



Restore with the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With the usual Bareos restore mechanism a file-hierarchy will be created on the restore client under the default restore location according to the options set:

-   :file:`<restore prefix>/<postgresDataDir>/`
-   :file:`<restore prefix>/<walArchive>/`

This example describes how to restore to the latest possible consistent point in time. You can manage to restore to any other point in in time available in the WAL files, please refer to the |postgresql| documentation for more details.

PostgreSQL >= 12
''''''''''''''''

Beginning with |postgresql| >= 12 the configuration must be done in your |postgresql| configuration file :file:`postgresql.conf`:

.. code-block:: cfg
   :caption: postgresql.conf

   ...
   restore_command = 'cp /var/lib/pgsql/wal_archive/%f %p'
   ...

Additionally a (empty) file named :file:`recovery.signal` must be created in your |postgresql| datadir.


PostgreSQL < 12
'''''''''''''''

For |postgresql| < 12 you need to place a minimal :file:`recovery.conf` in your |postgresql| datadir.

Example:

.. code-block:: cfg
   :caption: recovery.conf

   restore_command = 'cp /var/lib/pgsql/wal_archive/%f %p'


Where :file:`/var/lib/pgsql/wal_archive/` is the *walArchive* directory.


Initiate the Recovery Process
'''''''''''''''''''''''''''''

Make sure that the user **postgres** is allowed to rename the recovery marker file (:file:`recovery.signal` or :file:`recovery.conf`),
as the file will be renamed during the recovery process.
You might have to adapt your SELINUX configuration for this.

Starting the |postgresql| server shall now initiate the recovery process.


Troubleshooting the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If things don't work as expected, make sure that

- the |fd| (FD) works in general, so that you can make simple file backups and restores
- the Bareos FD Python plugins work in general, try one of
  the shipped simple sample plugins
- check your Postgres data directory for a file named backup_label. If it exists, another backup-process is already running. This file contains an entry like *LABEL: SomeLabel*. If the backup was triggered by this plugin, the label will look like: *LABEL: Bareos.pgplugin.jobid.<jobid>*.
  You may want to stop it using the *SELECT pg_stop_backup()* statement.
- make sure your *dbuser* can connect to the database *dbname* and is allowed to issue the following statements matching your |postgresql| version:

  .. code-block:: sql

     SELECT current_setting('server_version_num');
     -- Postgres version >= 9
     SELECT pg_start_backup();
     SELECT pg_backup_start_time();
     SELECT pg_stop_backup();
     -- Postgres version >=10:
     SELECT pg_current_wal_lsn();
     SELECT pg_switch_wal();
     -- Postgres version 9 only:
     SELECT pg_current_xlog_location();
     SELECT pg_switch_xlog();
