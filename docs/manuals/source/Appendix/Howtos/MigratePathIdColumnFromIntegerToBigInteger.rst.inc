.. _section-MigrateCatalogPathIdIntegerToBigInteger:

Migrate Catalog ``PathId`` column from ``Integer`` to ``biginteger``
--------------------------------------------------------------------

.. index::
   single: Integer to BigInteger
   single: PathId; Migration
   single: Database; Migration; PostgreSQL
   single: Catalog; Migration; PostgreSQL
   pair: Migration; PostgreSQL

If you receive the warning from the |dir| that your ``Path`` table primary sequence is reaching
its 32 bits integer limit, you should take action before this stop you to create new backup.

The following chapter will explain how to manually migrate your database to handle such situation.

Since Bareos :sinceVersion:`26.0.0: Postgresql Catalog PathId migration to bigint` the ``PathId`` is
fully treated as `bigint` inside the code.

While it is possible to run Bareos version :sinceVersion:`25.1.1: 25.1.1` with a such modified database,
additional constraint has to be added. See below.


Plan the downtime
~~~~~~~~~~~~~~~~~

This operation will take some time, tables ``File``, ``Path`` and their indexes will rewritten by
|postgresql|, you must plan a downtime, and decide which strategy to adopt concerning bvfs cache tables.


.. warning::

   The two biggest tables and their indexes will be recreated, you must ensure you have enough free
   space avaiable on your |postgresql| cluster partition.

   See documentation :ref:`PostgresSize` how to verify free and used space.


Make a backup of your old database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. warning::

   Make a backup of your old database before you start the migration process!

We highly recommend to use native and efficient tool like :command:`pg_basebackup`.

.. #See :ref:`CatalogMaintenance#pg_basebackup`



Use empty ``bvfs`` cache table strategy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Instead waiting to upgrade both tables ``pathhierarchy`` and ``pathvisibility`` with the existing content
which might increase somewhat the migration process, you might prefer to empty those cache tables
before the migration.

Then once the migration is done, you can again fill them with normal :command:`.bvfs` command
or let the system fill them when you do a restore from the webui.

.. code-block:: bconsole
   :caption: empty bvfs cache

   *<input>*.bvfs_clear_cache yes</input>
   Automatically selected Catalog: mycatalog
   Using Catalog "mycatalog"
   OK


Migrate the fields
~~~~~~~~~~~~~~~~~~

The migration process will highly depend on your hardware and the size of the database.
In the following steps we let commented into the sql instructions some values you might
be interested to activate.

.. note::

   The example values are adapted to the following hardware:

   - 64 cpus
   - 512GB ram
   - db cluster storage is a raid6 of 10 Enterprise SSD



Prepare the database
^^^^^^^^^^^^^^^^^^^^

.. warning::

   - Stop the |dir| before starting the any other commands.
   - Don't start the |dir| before completing the migration.
   - Run scripts in a resilient terminal like :command:`tmux` or :command:`screen`.


Run Alter table ``File``
^^^^^^^^^^^^^^^^^^^^^^^^

We recommend to copy and paste these queries into :file:`/var/tmp/01_alter_file_table.sql`

.. code-block:: sql
   :caption: 01_alter_file_table.sql

   -- We adapt temporary the setup of the cluster to be a beast in term of massive insert and index creation
   -- set maintenance_io_concurrency = 50;

   -- Allocate massive RAM for sorting and index construction (Uses ~128GB of your 512GB)
   -- set maintenance_work_mem = '128GB';

   -- Max out parallel worker tasks to fully engage your 64 CPU cores
   -- set max_parallel_maintenance_workers = 32;

   -- Disable synchronous disk flushing delays for this session
   -- set synchronous_commit = off;

   -- alter table is able to maintain the indexes at the same time it do the conversion.curl
   -- but we will still drop the 2 indexes using pathid and recreate them afterwards with more parallelism
   begin;
   drop index file_pjidpart_idx;
   drop index file_jpfid_idx;
   commit;

   -- Do the conversion, be patient, PostgreSQL will certainly be limited to one cpu task
   begin;
   alter table file alter column pathid type bigint;
   alter table file alter column fileid set default nextval('file_fileid_seq'::regclass);
   commit;

   begin;
   analyze file;
   commit;

Then run the following in a terminal

.. code-block:: shell-session
   :caption: Run the alter table file as postgres user

   su -s /bin/bash bareos -c "psql -d bareos -f /var/tmp/01_alter_file_table.sql"


Run Alter table ``Path``
^^^^^^^^^^^^^^^^^^^^^^^^

We recommend to copy and paste these queries into :file:`/var/tmp/02_alter_path_table.sql`

.. code-block:: sql
   :caption: 02_alter_path_table.sql

   -- We adapt temporary the setup of the cluster to be a beast in term of massive insert and index creation
   -- set maintenance_io_concurrency = 50;

   -- Allocate massive RAM for sorting and index construction (Uses ~128GB of your 512GB)
   -- set maintenance_work_mem = '128GB';

   -- Max out parallel worker tasks to fully engage your 64 CPU cores
   -- set max_parallel_maintenance_workers = 32;

   -- Disable synchronous disk flushing delays for this session
   -- set synchronous_commit = off;

   -- if you prefer you can empty bvfs cache tables by this way
   -- begin;
   -- truncate table pathvisibility;
   -- truncate table pathhierarchy;
   -- commit;

   -- set pathid,ppathid bigint on those tables
   begin;
   alter table pathvisibility alter column pathid type bigint;
   alter table pathhierarchy alter column pathid type bigint, alter column ppathid type bigint;
   commit;


   -- upgrade path table and its keys
   begin;
   alter table path alter column pathid type bigint;
   analyze path;
   commit;

   begin;
   analyze path;
   commit;

Then run the second steps in a terminal

.. code-block:: shell-session
   :caption: Run the alter table path as postgres user

   su -s /bin/bash bareos -c "psql -d bareos -f /var/tmp/02_alter_path_table.sql"


.. warning::

   In case you are still using Bareos :sinceVersion:`25.0.0: 25.0.0` binaries, you should add the
   following constraint to avoid |dir| trying to use number over the unsigned 32bits limit.

   .. code-block:: sql
      :caption: Path add constraint for unsigned 32bits

      -- add constraint to not insert something bigger than unsigned int32 (4294967294)
      begin;
      alter table path add constraint uint32_lower CHECK (pathid < 4294967294);
      commit;


Recreate indexes in parallel using multi-threading
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create the each one file per index to recreate.

First index :file:`03_index_file_jpfid_idx.sql`

.. code-block:: sql
   :caption: 03_index_file_jpfid_idx.sql

   -- We adapt temporary the setup of the cluster to be a beast in term of massive insert and index creation
   -- set maintenance_io_concurrency = 50;

   -- Allocate massive RAM for sorting and index construction (Uses ~64GB of your 512GB)
   -- We divided this value compared to other sql to half as we will create 2 distincts sessions.
   -- set maintenance_work_mem = '64GB';

   -- Max out parallel worker tasks to fully engage your 64 CPU cores
   -- Same here we divided by 2 the previous used value.
   -- set max_parallel_maintenance_workers = 32;

   -- Disable synchronous disk flushing delays for this session
   -- set synchronous_commit = off;

   begin;
   create index file_jpfid_idx on file (jobid, pathid, name);
   commit;

Second index :file:`/var/tmp/03_index_file_pjidpart_idx.sql`

.. code-block:: sql
   :caption: 03_index_file_pjidpart_idx.sql

   -- We adapt temporary the setup of the cluster to be a beast in term of massive insert and index creation
   -- set maintenance_io_concurrency = 50;

   -- Allocate massive RAM for sorting and index construction (Uses ~64GB of your 512GB)
   -- We divided this value compared to other sql to half as we will create 2 distinct sessions.
   -- set maintenance_work_mem = '64GB';

   -- Max out parallel worker tasks to fully engage your 64 CPU cores
   -- Same here we divided by 2 the previous used value.
   -- set max_parallel_maintenance_workers = 32;

   -- Disable synchronous disk flushing delays for this session
   -- set synchronous_commit = off;

   begin;
   create index file_pjidpart_idx on file (pathid, jobid) WHERE fileindex = 0 AND name = ''::text;
   commit;

Then you can run one command each time in a distinct terminal:

.. code-block:: shell-session
   :caption: Run the file jpfid index as postgres user

   time su postgres -c "psql -d bareos -f 3_index_file_pjidpart_idx.sql"


.. code-block:: shell-session
   :caption: Run the file pjidpart index as postgres user

   time su postgres -c "psql -d bareos -f 3_index_file_pjidpart_idx.sql"



Check migration results
~~~~~~~~~~~~~~~~~~~~~~~

Once the indexes have been rebuild you can check the changes with the following commands:


.. code-block:: shell-session
   :caption: Control `bigint` conversion on tables file, path, pathhierarchy, pathvisibility

   su postgres -c 'psql -d bareos -c "\d+ file" -c "\d+ path" -c "\d+ pathhierarchy" -c "\d+ pathvisibility"'

                                                                  Table "public.file"
      Column   |     Type      | Collation | Nullable |               Default                | Storage  | Compression | Stats target | Description
    -----------+---------------+-----------+----------+--------------------------------------+----------+-------------+--------------+-------------
    fileid    | bigint        |           | not null | nextval('file_fileid_seq'::regclass) | plain    |             |              |
    fileindex | integer       |           | not null | 0                                    | plain    |             |              |
    jobid     | integer       |           | not null |                                      | plain    |             |              |
    pathid    | bigint        |           | not null |                                      | plain    |             |              |
    deltaseq  | smallint      |           | not null | 0                                    | plain    |             |              |
    markid    | integer       |           | not null | 0                                    | plain    |             |              |
    lstat     | text          |           | not null |                                      | extended |             |              |
    md5       | text          |           | not null |                                      | extended |             |              |
    name      | text          |           |          | ''::text                             | extended |             |              |
    fhinfo    | numeric(20,0) |           | not null | 0                                    | main     |             |              |
    fhnode    | numeric(20,0) |           | not null | 0                                    | main     |             |              |
    Indexes:
        "file_pkey" PRIMARY KEY, btree (fileid)
        "file_jpfid_idx" btree (jobid, pathid, name)
        "file_pjidpart_idx" btree (pathid, jobid) WHERE fileindex = 0 AND name = ''::text
    Not-null constraints:
        "file_fileid_not_null" NOT NULL "fileid"
        "file_fileindex_not_null" NOT NULL "fileindex"
        "file_jobid_not_null" NOT NULL "jobid"
        "file_pathid_not_null" NOT NULL "pathid"
        "file_deltaseq_not_null" NOT NULL "deltaseq"
        "file_markid_not_null" NOT NULL "markid"
        "file_lstat_not_null" NOT NULL "lstat"
        "file_md5_not_null" NOT NULL "md5"
        "file_fhinfo_not_null" NOT NULL "fhinfo"
        "file_fhnode_not_null" NOT NULL "fhnode"
    Access method: heap
    Options: autovacuum_vacuum_threshold=1000, autovacuum_analyze_threshold=1000, autovacuum_enabled=true, autovacuum_analyze_scale_factor=0.02, autovacuum_vacuum_scale_factor=0.02

                                                            Table "public.path"
    Column |  Type  | Collation | Nullable |               Default                | Storage  | Compression | Stats target | Description
    --------+--------+-----------+----------+--------------------------------------+----------+-------------+--------------+-------------
    pathid | bigint |           | not null | nextval('path_pathid_seq'::regclass) | plain    |             |              |
    path   | text   |           |          |                                      | extended |             | 1000         |
    Indexes:
        "path_pkey" PRIMARY KEY, btree (pathid)
        "path_name_idx" UNIQUE, btree (path)
    Not-null constraints:
        "path_pathid_not_null" NOT NULL "pathid"
    Access method: heap
    Options: autovacuum_vacuum_threshold=1000, autovacuum_analyze_threshold=1000, autovacuum_enabled=true, autovacuum_analyze_scale_factor=0.02, autovacuum_vacuum_scale_factor=0.02

                                          Table "public.pathhierarchy"
    Column  |  Type  | Collation | Nullable | Default | Storage | Compression | Stats target | Description
    ---------+--------+-----------+----------+---------+---------+-------------+--------------+-------------
    pathid  | bigint |           | not null |         | plain   |             |              |
    ppathid | bigint |           | not null |         | plain   |             |              |
    Indexes:
        "pathhierarchy_pkey" PRIMARY KEY, btree (pathid)
        "pathhierarchy_ppathid" btree (ppathid)
    Not-null constraints:
        "pathhierarchy_pathid_not_null" NOT NULL "pathid"
        "pathhierarchy_ppathid_not_null" NOT NULL "ppathid"
    Access method: heap

                                        Table "public.pathvisibility"
    Column |  Type   | Collation | Nullable | Default | Storage | Compression | Stats target | Description
    --------+---------+-----------+----------+---------+---------+-------------+--------------+-------------
    pathid | bigint  |           | not null |         | plain   |             |              |
    jobid  | integer |           | not null |         | plain   |             |              |
    size   | bigint  |           |          | 0       | plain   |             |              |
    files  | integer |           |          | 0       | plain   |             |              |
    Indexes:
        "pathvisibility_pkey" PRIMARY KEY, btree (jobid, pathid)
    Not-null constraints:
        "pathvisibility_pathid_not_null" NOT NULL "pathid"
        "pathvisibility_jobid_not_null" NOT NULL "jobid"
    Access method: heap


Second stage of check the next value of sequences.

.. code-block:: shell-session
   :caption: Check next value for pathid sequence

   su postgres -c "psql -d bareos -c \"WITH seq AS (
     SELECT
       t.relname AS table_name,
       a.attname AS column_name,
       pg_sequence_last_value(ps.seqrelid) AS last_value,
       ps.seqmax                           AS seq_max,
       CASE a.atttypid
        WHEN 'int2'::regtype THEN 32767::bigint
        WHEN 'int4'::regtype THEN 2147483647::bigint
        WHEN 'int8'::regtype THEN 9223372036854775807::bigint
       END                                 AS col_max
     FROM pg_sequence ps
     JOIN pg_depend d
     ON d.objid      = ps.seqrelid
      AND d.classid    = 'pg_class'::regclass
      AND d.refclassid = 'pg_class'::regclass
      AND d.deptype    = 'a'
     JOIN pg_class     t ON t.oid = d.refobjid
     JOIN pg_attribute a ON a.attrelid = t.oid
      AND a.attnum   = d.refobjsubid
     WHERE t.relnamespace = 'public'::regnamespace
      AND t.relkind IN ('r', 'p')
     )
     SELECT
      table_name,
      column_name,
      last_value,
      seq_max,
      col_max,
      round(last_value * 100.0 / least(seq_max, col_max), 2) AS pct_used
     FROM seq
     ORDER BY pct_used DESC NULLS LAST;
   \""


Complete the migration process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

   Once the migration is done we highly recommend to redo a complete backup of the cluster,
   so you will be able to restore the cluster with bigint conversion done, without any delay.


* start the |dir|

The migration is now completed.
