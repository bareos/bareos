.. _CatMaintenanceChapter:

Catalog Maintenance
===================

:index:`\ <single: Maintenance; Catalog>`\  :index:`\ <single: Catalog Maintenance>`\

Catalog Database
----------------

Bareos stores its catalog in a database. Different database backends are offered:

-  PostgreSQL (recommended)

-  MariaDB/MySQL (deprecated)

-  Sqlite (deprecated)

What database will be used, can be configured in the |dir| configuration, see the :ref:`DirectorResourceCatalog`.

The database often runs on the same server as the |dir|. However, it is also possible to run it on a different system. This might require some more manual configuration, a PostgreSQL example can be found in :ref:`catalog-maintenance-remote-psql`.


.. _section-dbconfig:

dbconfig-common (Debian)
~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Platform; Debian; dbconfig-common>`
:index:`\ <single: Platform; Ubuntu; dbconfig-common>`

Since Bareos :sinceVersion:`14.2.0: dbconfig-common (Debian)` the Debian (and Ubuntu) based packages support the **dbconfig-common** mechanism to create and update the Bareos database, according to the user choices.

The first choice is, if **dbconfig-common** should be used at all. If you decide against it, the database must be configured manually, see :ref:`CatMaintenanceManualConfiguration`.

If you decided to use **dbconfig-common**, the next question will only be asked, if more than one Bareos database backend (**bareos-database-***) is installed. If this is the case, select the database backend you want to use.

.. image:: /include/images/dbconfig-1-enable.*
   :width: 45.0%

.. image:: /include/images/dbconfig-2-select-database-type.*
   :width: 45.0%




Depending on the selected database backend, more questions about how to access the database will be asked. Often, the default values are suitable.

The **dbconfig-common** configuration (and credentials) is done by the **bareos-database-common** package. Settings are stored in the file :file:`/etc/dbconfig-common/bareos-database-common.conf`. If you need to repeat this step, you can use the :command:`dpkg-reconfigure bareos-database-common` command.

The Bareos database backend will get automatically configured in :file:`/etc/bareos/bareos-dir.d/catalog/MyCatalog.conf`. If the Server is not running locally you need to specify :config:option:`dir/catalog/DbAddress`\  in the catalog resource. A later reconfiguration might require manual adapt changes.



.. warning::

   When using the PostgreSQL backend and updating to Bareos < 14.2.3, it is necessary to manually grant database permissions (:command:`grant_bareos_privileges`), normally by

.. code-block:: shell-session

   su - postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges

For details see chapter :ref:`CatMaintenanceManualConfiguration`.

.. _CatMaintenanceManualConfiguration:

Manual Configuration
~~~~~~~~~~~~~~~~~~~~

Bareos comes with a number of scripts to prepare and update the databases. All these scripts are located in the Bareos script directory, normally at :file:`/usr/lib/bareos/scripts/`.

================================= ============== ===================================================
**Script**                        **Stage**      **Description**
================================= ============== ===================================================
:file:`create_bareos_database`    installation   create Bareos database
:file:`make_bareos_tables`        installation   create Bareos tables
:file:`grant_bareos_privileges`   installation   grant database access privileges
:file:`update_bareos_tables [-f]` update         update the database schema
:file:`drop_bareos_tables`        deinstallation remove Bareos database tables
:file:`drop_bareos_database`      deinstallation remove Bareos database
:file:`make_catalog_backup.pl`    backup         backup the Bareos database, default on Linux
:file:`make_catalog_backup`       backup         backup the Bareos database for systems without Perl
:file:`delete_catalog_backup`     backup helper  remove the temporary Bareos database backup file
================================= ============== ===================================================

The database preparation scripts have following configuration options:

db_type
   -  command line parameter $1

   -  :config:option:`dir/catalog/DbDriver`\  from the configuration

   -  installed database backends

   -  fallback: postgresql

db_name
   -  environment variable ``db_name``\

   -  :config:option:`dir/catalog/DbName`\  from the configuration

   -  default: bareos

db_user
   -  environment variable ``db_user``\

   -  :config:option:`dir/catalog/DbUser`\  from the configuration

   -  default: bareos

db_password
   -  environment variable ``db_password``\

   -  :config:option:`dir/catalog/DbPassword`\  from the configuration

   -  default: *none*

Reading the settings from the configuration require read permission for the current user. The normal PostgreSQL administrator user (**postgres**) doesn’t have these permissions. So if you plan to use non-default database settings, you might add the user **postgres** to the group :strong:`bareos`.

The database preparation scripts need to have password-less administrator access to the database. Depending on the distribution you’re using, this requires additional configuration. See the following section about howto achieve this for the different database systems.

To view and test the currently configured settings, use following commands:

.. code-block:: shell-session
   :caption: Show current database configuration

   /usr/sbin/bareos-dbcheck -B
   catalog=MyCatalog
   db_name=bareos
   db_driver=mysql
   db_user=bareos
   db_password=YourPassword
   db_address=
   db_port=0
   db_socket=
   db_type=MySQL
   working_dir=/var/lib/bareos

.. code-block:: shell-session
   :caption: Test the database connection. Example: wrong password

   /usr/sbin/bareos-dir -t -f -d 500
   [...]
   bareos-dir: mysql.c:204-0 Error 1045 (28000): Access denied for user 'bareos'@'localhost' (using password: YES)
   bareos-dir: dird.c:1114-0 Could not open Catalog "MyCatalog", database "bareos".
   bareos-dir: dird.c:1119-0 mysql.c:200 Unable to connect to MySQL server.
   Database=bareos User=bareos
   MySQL connect failed either server not running or your authorization is incorrect.
   bareos-dir: mysql.c:239-0 closedb ref=0 connected=0 db=0
   25-Apr 16:25 bareos-dir ERROR TERMINATION
   Please correct the configuration in /etc/bareos/bareos-dir.d/*/*.conf

PostgreSQL configuration
^^^^^^^^^^^^^^^^^^^^^^^^

On most distributions, PostgreSQL uses ident to allow access to the database system. The database administrator account is the Unix user **postgres**. Normally, this user can access the database without password, as the ident mechanism is used to identify the user.

If this works on your system can be verified by

.. code-block:: shell-session
   :caption: Access the local PostgreSQL database

   su - postgres
   psql

If your database is configured to require a password, this must be definied in the file `:file:`~/.pgpass` <http://www.postgresql.org/docs/8.2/static/libpq-pgpass.html>`_ in the following syntax: :strong:`HOST:PORT:DATABASE:USER:PASSWORD`, e.g.

.. code-block:: cfg
   :caption: PostgreSQL access credentials

   localhost:*:bareos:bareos:secret

The permission of this file must be 0600 (:command:`chmod 0600 ~/.pgpass`).

Again, verify that you have specified the correct settings by calling the :command:`psql` command. If this connects you to the database, your credentials are good. Exit the PostgreSQL client and run the Bareos database preparation scripts:

.. code-block:: shell-session
   :caption: Setup Bareos catalog database

   su - postgres
   /usr/lib/bareos/scripts/create_bareos_database
   /usr/lib/bareos/scripts/make_bareos_tables
   /usr/lib/bareos/scripts/grant_bareos_privileges

The encoding of the bareos database must be :strong:`SQL_ASCII`. The command :command:`create_bareos_database` automatically creates the database with this encoding. This can be verified by the command :command:`psql -l`, which shows information about existing databases:

.. code-block:: shell-session
   :caption: List existing databases

   psql -l
           List of databases
      Name    |  Owner   | Encoding
   -----------+----------+-----------
    bareos    | postgres | SQL_ASCII
    postgres  | postgres | UTF8
    template0 | postgres | UTF8
    template1 | postgres | UTF8
   (4 rows)

The owner of the database may vary. The Bareos database maintenance scripts don’t change the default owner of the Bareos database, so it stays at the PostgreSQL administration user. The :command:`grant_bareos_privileges` script grant the required permissions to the Bareos database user. In contrast, when installing (not updating) using :ref:`dbconfig <section-dbconfig>`, the database owner will be identical with the Bareos database user.

By default, using PostgreSQL ident, a Unix user can access a database of the same name. Therefore the user **bareos** can access the database :file:`bareos`.

.. code-block:: shell-session
   :caption: Verify Bareos database on PostgreSQL as Unix user bareos (bareos-13.2.3)

   root@linux:~# su - bareos -s /bin/sh
   bareos@linux:~# psql
   Welcome to psql 8.3.23, the PostgreSQL interactive terminal.

   Type:  \copyright for distribution terms
          \h for help with SQL commands
          \? for help with psql commands
          \g or terminate with semicolon to execute query
          \q to quit

   bareos=> \dt
                    List of relations
    Schema |          Name          | Type  |  Owner
   --------+------------------------+-------+----------
    public | basefiles              | table | postgres
    public | cdimages               | table | postgres
    public | client                 | table | postgres
    public | counters               | table | postgres
    public | device                 | table | postgres
    public | devicestats            | table | postgres
    public | file                   | table | postgres
    public | filename               | table | postgres
    public | fileset                | table | postgres
    public | job                    | table | postgres
    public | jobhisto               | table | postgres
    public | jobmedia               | table | postgres
    public | jobstats               | table | postgres
    public | location               | table | postgres
    public | locationlog            | table | postgres
    public | log                    | table | postgres
    public | media                  | table | postgres
    public | mediatype              | table | postgres
    public | ndmpjobenvironment     | table | postgres
    public | ndmplevelmap           | table | postgres
    public | path                   | table | postgres
    public | pathhierarchy          | table | postgres
    public | pathvisibility         | table | postgres
    public | pool                   | table | postgres
    public | quota                  | table | postgres
    public | restoreobject          | table | postgres
    public | status                 | table | postgres
    public | storage                | table | postgres
    public | unsavedfiles           | table | postgres
    public | version                | table | postgres
   (30 rows)

   bareos=> select * from Version;
    versionid
   -----------
         2002
   (1 row)

   bareos=> \du
                                    List of roles
      Role name   | Superuser | Create role | Create DB | Connections | Member of
   ---------------+-----------+-------------+-----------+-------------+-----------
    bareos        | no        | no          | no        | no limit    | {}
    postgres      | yes       | yes         | yes       | no limit    | {}
   (2 rows)

   bareos=> \dp
                    Access privileges for database "bareos"
    Schema |               Name                |   Type   |  Access privileges
   --------+-----------------------------------+----------+--------------------------------------
    public | basefiles                         | table    | {root=arwdxt/root,bareos=arwdxt/root}
    public | basefiles_baseid_seq              | sequence | {root=rwU/root,bareos=rw/root}
   ...

   bareos=>

.. _catalog-maintenance-remote-psql:

Remote PostgreSQL Database
^^^^^^^^^^^^^^^^^^^^^^^^^^

When configuring bareos with a remote database, your first step is to check the connection from the |dir| host into the database. A functional connection can be verified by

.. code-block:: shell-session
   :caption: Access the remote PostgreSQL database

   su - postgres
   psql --host bareos-database.example.com

With a correct configuration you can access the database. If it fails, you need to correct the PostgreSQL servers' configuration files.

One way to manually create the database is to execute the bareos database preparation scripts with the :strong:`--host` option, explained later. However, it is advised to use the **dbconfig-common**. Both methods require you to add the database hostname/address as :config:option:`dir/catalog/DbAddress`\ .

If you’re using **dbconfig-common** you should choose :strong:`New Host`, enter the hostname or the local address followed by the password. As **dbconfig-common** uses the :strong:`ident` authentication by default the first try to connect will fail. Don’t be bothered by that. Choose :strong:`Retry` when prompted. From there, read carefully and configure the database to your needs. The authentication should be set
to password, as the ident method will not work with a remote server. Set the user and administrator according to your PostgreSQL servers settings.

Set the PostgreSQL server IP as :config:option:`dir/catalog/DbAddress`\  in :ref:`DirectorResourceCatalog`. You can also customize other parameters or use the defaults. A quick check should display your recent changes:

.. code-block:: shell-session
   :caption: Show current database configuration

   /usr/sbin/bareos-dbcheck -B
   catalog=MyCatalog
   db_name=bareos
   db_driver=postgresql
   db_user=bareos
   db_password=secret
   db_address=bareos-database.example.com
   db_port=0
   db_socket=
   db_type=PostgreSQL
   working_dir=/var/lib/bareos

If **dbconfig-common** did not succeed or you choosed not to use it, run the Bareos database preparation scripts with:

.. code-block:: shell-session
   :caption: Setup Bareos catalog database

   su - postgres
   /usr/lib/bareos/scripts/create_bareos_database --host=bareos-database.example.com
   /usr/lib/bareos/scripts/make_bareos_tables --host=bareos-database.example.com
   /usr/lib/bareos/scripts/grant_bareos_privileges --host=bareos-database.example.com

Retention Periods
-----------------

Database Size
~~~~~~~~~~~~~

:index:`\ <single: Size; Database>`
:index:`\ <single: Database Size>`

As mentioned above, if you do not do automatic pruning, your Catalog will grow each time you run a Job. Normally, you should decide how long you want File records to be maintained in the Catalog and set the File Retention period to that time. Then you can either wait and see how big your Catalog gets or make a calculation assuming approximately 154 bytes for each File saved and knowing the number of Files that are saved during each backup and the number of Clients you backup.

For example, suppose you do a backup of two systems, each with 100,000 files. Suppose further that you do a Full backup weekly and an Incremental every day, and that the Incremental backup typically saves 4,000 files. The size of your database after a month can roughly be calculated as:



::

   Size = 154 * No. Systems * (100,000 * 4 + 10,000 * 26)



where we have assumed four weeks in a month and 26 incremental backups per month. This would give the following:



::

   Size = 154 * 2 * (100,000 * 4 + 10,000 * 26) = 203,280,000 bytes



So for the above two systems, we should expect to have a database size of approximately 200 Megabytes. Of course, this will vary according to how many files are actually backed up.

You will note that the File table (containing the file attributes) make up the large bulk of the number of records as well as the space used. As a consequence, the most important Retention period will be the File Retention period.

Without proper setup and maintenance, your Catalog may continue to grow indefinitely as you run Jobs and backup Files, and/or it may become very inefficient and slow. How fast the size of your Catalog grows depends on the number of Jobs you run and how many files they backup. By deleting records within the database, you can make space available for the new records that will be added during the next Job. By constantly deleting old expired records (dates older than the Retention period), your
database size will remain constant.

.. _Retention:

Setting Retention Periods
~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Setting Retention Periods>`\  :index:`\ <single: Periods; Setting Retention>`\

Bareos uses three Retention periods: the File Retention period, the Job Retention period, and the Volume Retention period. Of these three, the File Retention period is by far the most important in determining how large your database will become.

The File Retention and the Job Retention are specified in each Client resource as is shown below. The Volume Retention period is specified in the Pool resource, and the details are given in the next chapter of this manual.

File Retention = <time-period-specification>
   :index:`\ <single: File Retention>`\  :index:`\ <single: Retention; File>`\  The File Retention record defines the length of time that Bareos will keep File records in the Catalog database. When this time period expires, and if AutoPrune is set to yes, Bareos will prune (remove) File records that are older than the specified File Retention period. The pruning will occur at the end of a backup Job for the given Client. Note that the Client database record contains a copy of the
   File and Job retention periods, but Bareos uses the current values found in the Director’s Client resource to do the pruning.

   Since File records in the database account for probably 80 percent of the size of the database, you should carefully determine exactly what File Retention period you need. Once the File records have been removed from the database, you will no longer be able to restore individual files in a Job. However, as long as the Job record still exists, you will be able to restore all files in the job.

   Retention periods are specified in seconds, but as a convenience, there are a number of modifiers that permit easy specification in terms of minutes, hours, days, weeks, months, quarters, or years on the record. See :config:datatype:`TIME` for additional details of modifier specification.

   The default File retention period is 60 days.

Job Retention = <time-period-specification>
   :index:`\ <single: Job; Retention>`\  :index:`\ <single: Retention; Job>`\  The Job Retention record defines the length of time that Bareos will keep Job records in the Catalog database. When this time period expires, and if AutoPrune is set to yes Bareos will prune (remove) Job records that are older than the specified Job Retention period. Note, if a Job record is selected for pruning, all associated File and JobMedia records will also be pruned regardless of the File Retention
   period set. As a consequence, you normally will set the File retention period to be less than the Job retention period.

   As mentioned above, once the File records are removed from the database, you will no longer be able to restore individual files from the Job. However, as long as the Job record remains in the database, you will be able to restore all the files backed up for the Job. As a consequence, it is generally a good idea to retain the Job records much longer than the File records.

   The retention period is specified in seconds, but as a convenience, there are a number of modifiers that permit easy specification in terms of minutes, hours, days, weeks, months, quarters, or years. See :config:datatype:`TIME` for additional details of modifier specification.

   The default Job Retention period is 180 days.

:config:option:`dir/client/AutoPrune`\
   :index:`\ <single: AutoPrune>`\  :index:`\ <single: Job; Retention; AutoPrune>`\  If set to yes, Bareos will automatically apply the File retention period and the Job retention period for the Client at the end of the Job. If you turn this off by setting it to no, your Catalog will grow each time you run a Job.

.. _section-JobStatistics:

Job Statistics
^^^^^^^^^^^^^^

:index:`\ <single: Statistics>`\  :index:`\ <single: Job; Statistics>`\

Bareos catalog contains lot of information about your IT infrastructure, how many files, their size, the number of video or music files etc. Using Bareos catalog during the day to get them permit to save resources on your servers.

In this chapter, you will find tips and information to measure Bareos efficiency and report statistics.

If you want to have statistics on your backups to provide some Service Level Agreement indicators, you could use a few SQL queries on the Job table to report how many:

-  jobs have run

-  jobs have been successful

-  files have been backed up

-  ...

However, these statistics are accurate only if your job retention is greater than your statistics period. Ie, if jobs are purged from the catalog, you won’t be able to use them.

Now, you can use the :bcommand:`update stats [days=num]` console command to fill the JobHistory table with new Job records. If you want to be sure to take in account only good jobs, ie if one of your important job has failed but you have fixed the problem and restarted it on time, you probably want to delete the first bad job record and keep only the successful one. For that simply let your staff do the job, and update JobHistory table after two or three days depending on your
organization using the :strong:`[days=num]` option.

These statistics records aren’t used for restoring, but mainly for capacity planning, billings, etc.

The :config:option:`dir/director/StatisticsRetention`\  defines the length of time that Bareos will keep statistics job records in the Catalog database after the Job End time. This information is stored in the ``JobHistory`` table. When this time period expires, and if user runs :bcommand:`prune stats` command, Bareos will prune (remove) Job records that are older than the specified period.

You can use the following Job resource in your nightly :config:option:`dir/job = BackupCatalog`\  job to maintain statistics.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/BackupCatalog.conf

   Job {
     Name = BackupCatalog
     ...
     RunScript {
       Console = "update stats days=3"
       Console = "prune stats yes"
       RunsWhen = After
       RunsOnClient = no
     }
   }


PostgreSQL Database
-------------------

:index:`\ <single: Database; PostgreSQL>`\  :index:`\ <single: PostgreSQL>`\

.. _CompactingPostgres:

Compacting Your PostgreSQL Database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Database; PostgreSQL; Compacting>`\

Over time, as noted above, your database will tend to grow until Bareos starts deleting old expired records based on retention periods. After that starts, it is expected that the database size remains constant, provided that the amount of clients and files being backed up is constant.

Note that PostgreSQL uses multiversion concurrency control (MVCC), so that an UPDATE or DELETE of a row does not immediately remove the old version of the row. Space occupied by outdated or deleted row versions is only reclaimed for reuse by new rows when running **VACUUM**. Such outdated or deleted row versions are also referred to as *dead tuples*.

Since PostgreSQL Version 8.3, autovacuum is enabled by default, so that setting up a cron job to run VACUUM is not necesary in most of the cases. Note that there are two variants of VACUUM: standard VACUUM and VACUUM FULL. Standard VACUUM only marks old row versions for reuse, it does not free any allocated disk space to the operating system. Only VACUUM FULL can free up disk space, but it requires exclusive table locks so that it can not be used in parallel with production database operations
and temporarily requires up to as much additional disk space that the table being processed occupies.

All database programs have some means of writing the database out in ASCII format and then reloading it. Doing so will re-create the database from scratch producing a compacted result, so below, we show you how you can do this for PostgreSQL.

For a PostgreSQL database, you could write the Bareos database as an ASCII file (:file:`bareos.sql`) then reload it by doing the following:

.. code-block:: shell-session

   pg_dump -c bareos > bareos.sql
   cat bareos.sql | psql bareos
   rm -f bareos.sql

Depending on the size of your database, this will take more or less time and a fair amount of disk space. For example, you can :command:`cd` to the location of the Bareos database (typically :file:`/var/lib/pgsql/data` or possible :file:`/usr/local/pgsql/data`) and check the size.

Except from special cases PostgreSQL does not need to be dumped/restored to keep the database efficient. A normal process of vacuuming will prevent the database from getting too large. If you want to fine-tweak the database storage, commands such as VACUUM, VACUUM FULL, REINDEX, and CLUSTER exist specifically to keep you from having to do a dump/restore.

More details on this subject can be found in the PostgreSQL documentation. The page http://www.postgresql.org/docs/ contains links to the documentation for all PostgreSQL versions. The section *Routine Vacuuming* explains how VACUUM works and why it is required, see http://www.postgresql.org/docs/current/static/routine-vacuuming.html for the current PostgreSQL version.

.. _PostgresSize:

What To Do When The Database Keeps Growing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Especially when a high number of files are beeing backed up or when working with high retention periods, it is probable that autovacuuming will not work. When starting to use Bareos with an empty Database, it is normal that the file table and other tables grow, but the growth rate should drop as soon as jobs are deleted by retention or pruning. The file table is usually the largest table in Bareos.

The reason for autovacuuming not beeing triggered is then probably the default setting of ``autovacuum_vacuum_scale_factor = 0.2``, the current value can be shown with the following query or looked up in ``postgresql.conf``:

.. code-block:: shell-session
   :caption: SQL statement to show the autovacuum\_vacuum\_scale\_factor parameter

   bareos=# show autovacuum_vacuum_scale_factor;
    autovacuum_vacuum_scale_factor
    --------------------------------
     0.2
     (1 row)

In essence, this means that a VACUUM is only triggered when 20% of table size are obsolete. Consequently, the larger the table is, the less frequently VACUUM will be triggered by autovacuum. This make sense because vacuuming has a performance impact. While it is possible to override the autovacuum parameters on a table-by-table basis, it can then still be triggered at any time.

To learn more details about autovacuum see http://www.postgresql.org/docs/current/static/routine-vacuuming.html#AUTOVACUUM

The following example shows how to configure running VACUUM on the file table by using an admin-job in Bareos. The job will be scheduled to run at a time that should not run in parallel with normal backup jobs, here by scheduling it to run after the BackupCatalog job.

First step is to check the amount of dead tuples and if autovacuum triggers VACUUM:

.. code-block:: shell-session
   :caption: Check dead tuples and vacuuming on PostgreSQL

   bareos=# SELECT relname, n_dead_tup, last_vacuum, last_autovacuum, last_analyze, last_autoanalyze
   FROM pg_stat_user_tables WHERE n_dead_tup > 0 ORDER BY n_dead_tup DESC;
   -[ RECORD 1 ]----+------------------------------
   relname          | file
   n_dead_tup       | 2955116
   last_vacuum      |
   last_autovacuum  |
   last_analyze     |
   last_autoanalyze |
   -[ RECORD 2 ]----+------------------------------
   relname          | log
   n_dead_tup       | 111298
   last_vacuum      |
   last_autovacuum  |
   last_analyze     |
   last_autoanalyze |
   -[ RECORD 3 ]----+------------------------------
   relname          | job
   n_dead_tup       | 1785
   last_vacuum      |
   last_autovacuum  | 2015-01-08 01:13:20.70894+01
   last_analyze     |
   last_autoanalyze | 2014-12-27 18:00:58.639319+01
   ...

In the above example, the file table has a high number of dead tuples and it has not been vacuumed. Same for the log table, but the dead tuple count is not very high. On the job table autovacuum has been triggered.

Note that the statistics views in PostgreSQL are not persistent, their values are reset on restart of the PostgreSQL service.

To setup a scheduled admin job for vacuuming the file table, the following must be done:

#. Create a file with the SQL statements for example
   ``/usr/local/lib/bareos/scripts/postgresql_file_table_maintenance.sql``
   with the following content:

   .. code-block:: shell-session
      :caption: SQL Script for vacuuming the file table on PostgreSQL

      \t \x
      SELECT relname, n_dead_tup, last_vacuum, last_autovacuum, last_analyze, last_autoanalyze
      FROM pg_stat_user_tables WHERE relname='file';
      VACUUM VERBOSE ANALYZE file;
      SELECT relname, n_dead_tup, last_vacuum, last_autovacuum, last_analyze, last_autoanalyze
      FROM pg_stat_user_tables WHERE relname='file';
      \t \x
      SELECT table_name,
        pg_size_pretty(pg_total_relation_size(table_name)) AS total_sz,
        pg_size_pretty(pg_total_relation_size(table_name) - pg_relation_size(table_name)) AS idx_sz
        FROM ( SELECT ('"' || relname || '"' ) AS table_name
          FROM pg_stat_user_tables WHERE relname != 'batch' ) AS all_tables
        ORDER BY pg_total_relation_size(table_name) DESC LIMIT 5;

   The SELECT statements are for informational purposes only, the final statement shows the total and index disk usage of the 5 largest tables.

#. Create a shell script that runs the SQL statements, for example
   ``/usr/local/lib/bareos/scripts/postgresql_file_table_maintenance.sh``
   with the following content:

   .. code-block:: shell-session
      :caption: SQL Script for vacuuming the file table on PostgreSQL

      #!/bin/sh
      psql bareos < /usr/local/lib/bareos/scripts/postgresql_file_table_maintenance.sql

#. As in PostgreSQL only the database owner or a database superuser is allowed to run VACUUM, the script will be run as the ``postgres`` user. To permit the ``bareos`` user to run the script via ``sudo``, write the following sudo rule to a file by executing ``visudo -f /etc/sudoers.d/bareos_postgres_vacuum``:

   .. code-block:: shell-session
      :caption: sudo rule for allowing bareos to run a script as postgres

      bareos ALL = (postgres) NOPASSWD: /usr/local/lib/bareos/scripts/postgresql_file_table_maintenance.sh

   and make sure that ``/etc/sudoers`` includes it, usually by the line

   ::

      #includedir /etc/sudoers.d


#. Create the following admin job in the director configuration

   .. code-block:: shell-session
      :caption: SQL Script for vacuuming the file table on PostgreSQL

      # PostgreSQL file table maintenance job
      Job {
        Name = FileTableMaintJob
        JobDefs = DefaultJob
        Schedule = "WeeklyCycleAfterBackup"
        Type = Admin
        Priority = 20

        RunScript {
          RunsWhen = Before
          RunsOnClient = no
          Fail Job On Error = yes
          Command = "sudo -u postgres /usr/local/lib/bareos/scripts/postgresql_file_table_maintenance.sh"
        }
      }

   In this example the job will be run by the schedule WeeklyCycleAfterBackup, the ``Priority`` should be set to a higher value than ``Priority`` in the BackupCatalog job.

.. _RepairingPSQL:

Repairing Your PostgreSQL Database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For Bareos specific problems, consider using :ref:`bareos-dbcheck` program.
In other cases, consult the PostgreSQL documents for how to repair the database.

.. _BackingUpBareos:

Backing Up Your Bareos Database
-------------------------------

.. index::
   single: Backup; Bareos database
   single: Backup; Catalog
   single: Database; Backup Bareos database

If ever the machine on which your Bareos database crashes, and you need to restore from backup tapes, one of your first priorities will probably be to recover the database. Although Bareos will happily backup your catalog database if it is specified in the FileSet, this is not a very good way to do it, because the database will be saved while Bareos is modifying it. Thus the database may be in an instable state. Worse yet, you will backup the database before all the Bareos updates have been
applied.

To resolve these problems, you need to backup the database after all the backup jobs have been run. In addition, you will want to make a copy while Bareos is not modifying it. To do so, you can use two scripts provided in the release make_catalog_backup and delete_catalog_backup. These files will be automatically generated along with all the other Bareos scripts. The first script will make an ASCII copy of your Bareos database into bareos.sql in the working directory you specified in your
configuration, and the second will delete the bareos.sql file.

The basic sequence of events to make this work correctly is as follows:

-  Run all your nightly backups

-  After running your nightly backups, run a Catalog backup Job

-  The Catalog backup job must be scheduled after your last nightly backup

-  You use :config:option:`dir/job/RunBeforeJob`\  to create the ASCII backup file and :config:option:`dir/job/RunAfterJob`\  to clean up

Assuming that you start all your nightly backup jobs at 1:05 am (and that they run one after another), you can do the catalog backup with the following additional Director configuration statements:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/BackupCatalog.conf

   # Backup the catalog database (after the nightly save)
   Job {
     Name = "BackupCatalog"
     Type = Backup
     Client=rufus-fd
     FileSet="Catalog"
     Schedule = "WeeklyCycleAfterBackup"
     Storage = DLTDrive
     Messages = Standard
     Pool = Default
     # This creates an ASCII copy of the catalog
     # Arguments to make_catalog_backup.pl are:
     #  make_catalog_backup.pl <catalog-name>
     RunBeforeJob = "/usr/lib/bareos/scripts/make_catalog_backup.pl MyCatalog"
     # This deletes the copy of the catalog
     RunAfterJob  = "/usr/lib/bareos/scripts/delete_catalog_backup"
     # This sends the bootstrap via mail for disaster recovery.
     # Should be sent to another system, please change recipient accordingly
     Write Bootstrap = "|/usr/sbin/bsmtp -h localhost -f \"\(Bareos\) \" -s \"Bootstrap for Job %j\" root@localhost"
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/schedule/WeeklyCycleAfterBackup.conf

   # This schedule does the catalog. It starts after the WeeklyCycle
   Schedule {
     Name = "WeeklyCycleAfterBackup"
     Run = Level=Full sun-sat at 1:10
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/fileset/Catalog.conf

   # This is the backup of the catalog
   FileSet {
     Name = "Catalog"
     Include {
       Options {
         signature=MD5
       }
       File = "/var/lib/bareos/bareos.sql" # database dump
       File = "/etc/bareos"                # configuration
     }
   }

It is preferable to write/send the :ref:`bootstrap <BootstrapChapter>` file to another computer. It will allow you to quickly recover the database backup should that be necessary. If you do not have a bootstrap file, it is still possible to recover your database backup, but it will be more work and take longer.
