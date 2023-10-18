.. _plugin-postgresql-fd:

PostgreSQL Plugin
~~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; PostgreSQL Backup

The PostgreSQL plugin supports an online (Hot) backup of database files and database transaction
logs (WAL).
With online database and transaction logs, the backup plugin can perform Point-In-Time-Restore
(PITR) up to a single selected transaction or date/time.

This plugin uses the standard API |postgresql| backup routines based on `pg_backup_start()` and
`pg_backup_stop()` functions in :strong:`non-exclusive mode`.
(Before PostgreSQL 15 formally `pg_start_backup()` and `pg_stop_backup()`)

The key features are:

* Incremental backups
* Point in time recovery
* Backups that complete quickly and reliably
* Uninterrupted transaction processing during backups
* Savings on disk space and network bandwidth
* Higher uptime due to faster restore time


Concept
^^^^^^^

Please make sure to read the |postgresql| documentation about the backup and restore process:
https://www.postgresql.org/docs/current/continuous-archiving.html

This is just a short outline of the tasks performed by the plugin.

#. Notify |postgresql| that we want to start backup the database files using the
   `SELECT pg_backup_start()` statement
#. Backup database files
#. Detect if tablespace are in use. Backup the external locations of all tablespaces.
#. Notify |postgresql| when done with file backups using the `SELECT pg_backup_stop()` statement
#. |postgresql| will write `Write-Ahead-Logfiles` (WAL) into the WAL Archive directory. These 
   transaction logs contain transactions done while the file backup proceeded
#. Backup fresh created WAL files
#. Add files required for a restore (`backup_label`, `recovery.signal`/`recovery.conf` (for version <= 12)
 and `tablespace_map` (if tablespaces are in use) as virtual files to the backup.

Incremental and Differential backups will only have to backup WAL files created since last reference backup.

The restore basically works like this:

#. Restore all files to the original |postgresql| location
#. Configure |postgresql| for the recovery (see below)
#. Start |postgresql|
#. |postgresql| will restore the latest possible consistent point in time. You can manage to restore
   to any other point in in time available in the WAL files, please refer to the |postgresql| 
   documentation for more details.


.. warning::

   In order to make coherent backups, it is imperative that :strong:`the same postgresql major version` 
   is used for :strong:`full and incremental/differential` Backups depending on each other.


Prerequisites for the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This plugin is a Bareos Python 3 plugin.
It requires |postgresql| cluster :strong:`version >= 10` and the Python module 
:strong:`pg8000 >= 1.16` to be installed.

Since :sinceVersion:`21: PostgreSQL Plugin` the plugin was changed to the Python 
module :strong:`pg8000`, with a required minimum version of 1.16, instead of 
:strong:`psycopg2` and using Python >= 3.6 is mandatory.

If a distribution provided pg8000 package exists and is the same or newer version, it can be used. 
Otherwise it must be installed using the command :command:`pip3 install pg8000`.


The plugin must be installed on the same host where the |postgresql| cluster runs, as files are 
backed up from the local filesystem.


.. warning::
   You have to enable PostgreSQL WAL-Archiving. The process and the plugin depend on it.

   As a minimum this requires that you create an WAL archive directory
   and matching settings in your |postgresql| configuration file :strong:`postgresql.conf`.


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

Make sure you have met the prerequisites, after that install the package :strong:`bareos-filedaemon-postgresql-python-plugin`.


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
   :caption: bareos-dir.d/fileset/postgresql.conf

   FileSet {
       Name = "postgresql"
       Include  {
           Options {
               Compression = LZ4
               Signature = XXH128
           }
           Plugin = "python"
                    ":module_name=bareos-fd-postgresql"
                    ":postgresql_data_dir=/var/lib/pgsql/data"
                    ":wal_archive_dir=/var/lib/pgsql/wal_archive/"
       }
   }



You can append options to the plugin call as key=value pairs, separated by ``:``. The following options are available:

postgresql_data_dir
   the Postgres data directory. Default: :file:`/var/lib/pgsql/data`

wal_archive_dir
   directory where |postgresql| archives the WAL files as defined in your :file:`postgresql.conf` 
   with the `archive_command` directive. This is a :strong:`mandatory` option, there is no default set.

db_user
   with this user the plugin will try to connect to the database.
   Default: `root`

db_password
   a optional password needed for the connection.
   Default: `None`

db_name
   there needs to be a named database for the connection. 
   Default: `postgres`

db_host
   useful, if socket is not in default location. Specify socket-directory with a leading / here
   Default: `None`

db_port
   useful, if cluster is not listening default port. 
   Default: `5432`

ignore_subdirs
   a list of comma separated directories below the `postgres_data_dir`, that will not be backed up. 
   Default: `pgsql_tmp`

   .. note::
      As recommended by upstream the content of the following sub-directories content
      will not be backup. But the sub-directory as directory will always be.
      `pg_dynshmem`, `pg_notify`, `pg_serial`, `pg_snapshots`, `pg_stat_tmp`, `pg_subtrans`, `pg_wal`

switch_wal
   If set to `true` (default), the plugin will let |postgresql| write a new wal file, if the current
   Log Sequence Number (LSN) is greater than the LSN from the previous job.
   This makes sure that all changes will be backed up.
   Default: `true`

switch_wal_timeout
   Timeout in seconds to wait for WAL archiving after WAL switch: 
   Default `60 seconds`

role
   Set the role used after login, before the first sql call. 
   Default: `None`

start_fast
   By default, the backup will start after a checkpoint which can take some time. 
   If `start_fast_start` is `true`, `pg_backup_start` will be executed as quickly as possible. 
   This enforces an immediate checkpoint which can cause a spike in I/O operations and slowing 
   any concurrently executing queries.
   Default: `False`

stop_wait_wal_archive
   Optional parameter of type `boolean`. It controls if the plugin will wait for the WAL archiving
   to be complete at the end of the backup. In the default case the plugin will wait.
   We don't recommend to change the default here. 
   Default: `True`

.. note::

   The plugin is using the *non-exclusive backup* method. Several backups can run at the same
   time on the cluster: which allow different tools to backup the cluster simultaneously.

   For Bareos we recommend to set :config:option:`dir/job/AllowDuplicateJobs`\ = No
   to limit the number of job to only one at the same time.


Restore with the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With the usual Bareos restore mechanism a file-hierarchy will be created on the restore client
under the default restore location according to the options set:

-   :file:`<restore prefix>/<postgresql_data_dir>/`
-   :file:`<restore prefix>/<wal_archive_dir>/`

This example describes how to restore to the latest possible consistent point in time. You can
manage to restore to any other point in time available in the WAL files, please refer to the
|postgresql| documentation for more details.

PostgreSQL >= 12
''''''''''''''''

Beginning with |postgresql| >= 12 the configuration must be done in your |postgresql| 
configuration file :file:`postgresql.conf`:

.. code-block:: cfg
   :caption: postgresql.conf

   ...
   restore_command = 'cp /var/lib/pgsql/wal_archive/%f %p'
   ...

Additionally a file named :file:`recovery.signal` is created in your |postgresql| datadir by the plugin.
It contains as a comment the backup label jobid and the |postgresql| major version used.


PostgreSQL < 12
'''''''''''''''

For |postgresql| < 12 you need to complete the :file:`recovery.conf` in your |postgresql| datadir.
It contains as a comment the backup label jobid and the |postgresql| major version used.

Example:

.. code-block:: cfg
   :caption: recovery.conf

   restore_command = 'cp /var/lib/pgsql/wal_archive/%f %p'


Where :file:`/var/lib/pgsql/wal_archive/` is the `wal_archive_dir` directory.


Initiate the Recovery Process
'''''''''''''''''''''''''''''

Make sure that the user :strong:`postgres` is allowed to rename the recovery marker file 
(:file:`recovery.signal` or :file:`recovery.conf`), as the file will be renamed during the
recovery process. Should be the case if restored by the plugin.
You might have to adapt your SELINUX configuration for this.

Starting the |postgresql| server shall now initiate the recovery process.

.. warning::

   When restoring a cluster which uses :strong:`tablespaces`, the table space location
   (directory) needs to be empty before the restore. Also ensure that the restored links
   in `data/pg_tblspc` point to the restored tablespace data.


Troubleshooting the PostgreSQL Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If things don't work as expected, make sure that

- the |fd| (FD) works in general, so that you can make simple file backups and restores
- the Bareos FD Python plugins work in general, try one of
  the shipped simple sample plugins
- check your |postgresql| data directory for files `backup_label` `recovery.signal` `tablespace_map`.
  If they exists, the cluster has been restored, but has not been restarted yet.
- make sure your `dbuser` can connect to the database `dbname` and is allowed to issue 
  the following statements matching your |postgresql| version:

  .. code-block:: sql

     SELECT current_setting('server_version_num');
     -- Version >= 15
     SELECT pg_backup_start();
     SELECT pg_backup_stop();
     
     -- Version >=10 < 15
     SELECT pg_start_backup();
     SELECT pg_stop_backup();

     SELECT pg_current_wal_lsn();
     SELECT pg_switch_wal();
