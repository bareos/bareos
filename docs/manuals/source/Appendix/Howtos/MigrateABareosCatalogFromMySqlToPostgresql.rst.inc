.. _section-MigrationMysqlToPostgresql:

Migrate a Bareos Catalog from |mysql| to |postgresql|
-----------------------------------------------------

.. index::
   single: Migration from MySQL to PostgreSQL
   single: Database; Migration; MySQL
   single: Catalog; Migration; MySQL
   single: Database; Migration; PostgreSQL
   single: Catalog; Migration; PostgreSQL
   pair: Migration; MySQL
   pair: Migration; PostgreSQL
   single: bareos-dbcopy
   see: dbcopy; bareos-dbcopy

Since Bareos :sinceVersion:`21.0.0: MySQL backend removed` the |mysql| database backend is not shipped anymore.
Therefore you need to use Bareos 20 to migrate an existing |mysql| Bareos Catalog to |postgresql|.
To do so, upgrade to Bareos 20 first and use `bareos-dbcopy` to copy the whole Bareos MySQL catalog into a new |postgresql| catalog database.
This chapter describes how to do a migration using :command:`bareos-dbcopy`.

Make a backup of your old database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. warning::

   Make a backup of your old database before you start the migration process!


Prepare the new database
~~~~~~~~~~~~~~~~~~~~~~~~

.. warning::

   Don't start the |dir| before completing the migration.
   Otherwise the |dir| will already insert data into the new |postgresql| catalog while :command:`bareos-dbcopy` will skip every table already containing data.

Firstly, create a new |postgresql| database as described in :ref:`section-CreateDatabase`,
You have to give the dbtype to the script, so the right db engine is used.

.. code-block:: shell-session
   :caption: Run creating script with db type specified

   su - postgres /usr/lib/bareos/scripts/create_bareos_database postgresql
   su - postgres /usr/lib/bareos/scripts/make_bareos_tables postgresql
   su - postgres /usr/lib/bareos/scripts/grant_bareos_privileges postgresql


Both |mysql| and |postgresql| need to have the same Bareos database scheme version,
i.e. have the schema from the identical Bareos version
(this should be the case automatically, when upgrading the |mysql| catalog to Bareos 20 and creating a Bareos 20 |postgresql| catalog).

Add the new |postgresql| database to the current |dir| configuration,
but **do not remove** the |mysql| database from the config, yet.
Both catalog resources must be present during the migration process.

These are the catalog resources used in this example:

.. code-block:: bareosconfig
   :caption: Existing catalog resource for |mysql|

   Catalog
   {
     Name = MyCatalog
     DB Driver = mysql
     DB Name = bareos;
     DB User = bareos;
     DB Password = ""
   }

.. code-block:: bareosconfig
   :caption: New catalog resource for |postgresql|

   Catalog
   {
     Name = MyCatalog-psql
     DB Driver = postgresql
     DB Name = bareos;
     DB User = bareos;
     DB Password = ""
   }

Run bareos-dbcopy
~~~~~~~~~~~~~~~~~

Once the databases are running you can start to copy the contents from |mysql|
to |postgresql|.

.. note::

   Please run bareos-dbcopy as user **bareos** to avoid problems with access rights.
   See example below how to start the shell as user **bareos** and run dbcopy.

.. code-block:: shell-session
   :caption: Run the bareos-dbcopy command as bareos user

   # Start the shell as bareos user
   su -s /bin/bash - bareos

   # run the command as user bareos:
   bareos-dbcopy -c <path-to-bareos-config> MyCatalog MyCatalog-psql


Depending on the size of your database the copy process can run up to several hours.
In our tests with a database containing 160 Million rows in the file table took about
5 hours to copy (the testsystem was equipped with SSDs).


*bareos-dbcopy* will then examine the databases and copy the tables one by one.
The *file table* is by far the largest table and usually takes the longest time
to copy.


.. code-block:: shell-session
   :caption: Example output of bareos-dbcopy

   Copying tables from "MyCatalog" to "MyCatalog-psql"
   gathering information about source catalog "MyCatalog"...
   gathering information about destination catalog "MyCatalog-psql"...
   getting column descriptions...
   --> basefiles
   --> client
   --> counters

   ...

   --> version
   copying tables...
   ====== table BaseFiles ======
   --> checking destination table...
   --> requesting number of rows to be copied...
   --> nothing to copy...
   --> updating sequence
   --> success

   ...

   ====== table File ======
   --> checking destination table...
   --> requesting number of rows to be copied...
   --> copying 100'000 rows...
   --> Start: 2020-02-26 16:52:01
         4% ETA:2020-02-26 16:52:08 (running:0h00m00s, remaining:0h00m07s)
        12% ETA:2020-02-26 16:52:07 (running:0h00m00s, remaining:0h00m05s)
        20% ETA:2020-02-26 16:52:08 (running:0h00m01s, remaining:0h00m05s)
        28% ETA:2020-02-26 16:52:07 (running:0h00m02s, remaining:0h00m04s)
        36% ETA:2020-02-26 16:52:07 (running:0h00m02s, remaining:0h00m04s)
        44% ETA:2020-02-26 16:52:07 (running:0h00m03s, remaining:0h00m03s)
        52% ETA:2020-02-26 16:52:08 (running:0h00m03s, remaining:0h00m03s)
        60% ETA:2020-02-26 16:52:07 (running:0h00m04s, remaining:0h00m02s)
        68% ETA:2020-02-26 16:52:08 (running:0h00m04s, remaining:0h00m02s)
        76% ETA:2020-02-26 16:52:08 (running:0h00m05s, remaining:0h00m01s)
        84% ETA:2020-02-26 16:52:08 (running:0h00m05s, remaining:0h00m01s)
        92% ETA:2020-02-26 16:52:08 (running:0h00m06s, remaining:0h00m00s)
       100% ETA:2020-02-26 16:52:08 (running:0h00m06s, remaining:0h00m00s)
   --> updating sequence
   --> success

   ...

   ====== table webacula_where_acl ======
   --> checking destination table...
   --> destination table does not exist
   --> *** skipping ***
   database copy completed successfully

.. note::

   In the example above the table *webacula_where_acl* was skipped, because it
   does not exist in the target database scheme. This is the desired behaviour.


Complete the migration process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After bareos-dbcopy copied all data to the |postgresql| catalog the following
tasks need to be done:

* rename the |postgresql| catalog resource to the name of the |mysql| resource
  (in our example: MyCatalog-psql -> MyCatalog)
* comment out the |mysql| catalog resource from the director configuration
* start the |dir|

The migration is now completed.

.. note::

   Once you are ready to upgrade to :sinceVersion:`21.0.0: PostgreSQL`, **remove** the package
   **bareos-database-mysql** from your installation.
   We've made the upgrade process failing on purpose to avoid unattended upgrade.
