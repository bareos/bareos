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
installation from a mysql/mariadb catalog to postgresql.

:program:`bareos-dbcopy` only copies the data over to the new catalog.
The ``<destinationcatalog>`` needs to be created and initialized with the
correct tables and columns via :program:`create_bareos_database`,
:program:`make_bareos_tables` and :program:`grant_bareos_privileges`.
Both ``sourcecatalog`` and ``destinationcatalog`` need to have the same database
scheme version, i.e. have the schema from the identical Bareos version..


Options
-------

.. program:: bareos-dbcopy

.. option:: -c <configuration directory>

   bareos-dbcopy reads the director configuration from the given
   <configuration directory> instead of :file:`/etc/bareos`

.. option:: -e

   Examine and verify every single data record of each table after
   copying. This can take a very long time. Without -e, only the first
   and last record of each database is examined and verified.

.. option:: -?

   Prints usage information
