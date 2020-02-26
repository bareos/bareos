.. _program-bareos-dbcopy:

bareos-dbcopy
=============

Synopsis
--------

**bareos-dbcopy** [*options*] <*sourcecatalog*> <*destinationcatalog*>

Description
-----------

:program:`bareos-dbcopy` copies the data of all tables from the Bareos catalog
``<sourcecatalog>`` into the Bareos catalog ``<destinationcatalog>``.

:program:`bareos-dbcopy` reads the Bareos director configuration from
:file:`/etc/bareos` to load the catalog settings. A configuration resource
for both ``<sourcecatalog>`` and ``<destinationcatalog>`` must exist.

The main use of :program:`bareos-dbcopy` is to migrate an existing Bareos
installation from |mysql| to postgreSQL. Therefore the allowed
:config:option:`dir/catalog/DbDriver` in the ``<sourcecatalog>`` is |mysql|,
for ``<destinationcatalog>`` only PostgreSQL is possible.

:program:`bareos-dbcopy` only copies the data over to the new catalog.
The ``<destinationcatalog>`` needs to be created and initialized with the
correct tables and columns via :program:`create_bareos_database`,
:program:`make_bareos_tables` and :program:`grant_bareos_privileges`.

Both ``sourcecatalog`` and ``destinationcatalog`` need to have the same database
scheme version, i.e. have the schema from the identical Bareos version.

Internal workflow
-----------------
* :program:`bareos-dbcopy` scans both the source and destination database for
  existing tables and column definitions.

* For each table, each row is transferred from the source to the destination
  database. Depending on the column type, data filters are applied.

* Tables existing in the source database but not in the destination database are
  skipped.

* If a destination table already contains data, then this table is skipped.

Options
-------

.. program:: bareos-dbcopy

.. option:: -c <configuration directory>

   bareos-dbcopy reads the director configuration from the given
   <configuration directory> instead of :file:`/etc/bareos`

.. option:: -i

   By default all rows are inserted into the postgres database by
   COPY FROM STDIN. This flag overrides the default and inserts all rows
   by INSERT INTO statements. Because this runs much slower it should only
   be used in case of errors.

.. option:: -?

   Prints usage information
