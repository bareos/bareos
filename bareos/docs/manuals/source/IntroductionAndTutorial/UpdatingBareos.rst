.. _bareos-update:

Updating Bareos
===============

In most cases, a Bareos update is simply done by a package update of the distribution. Please remind, that Bareos Director and Bareos Storage Daemon must always have the same version. The version of the File Daemon may differ, see chapter about :ref:`backward compatibility <backward-compatibility>`.

Updating the configuration files
--------------------------------

When updating Bareos through the distribution packaging mechanism, the existing configuration kept as they are.

If you don’t want to modify the behavior, there is normally no need to modify the configuration.

However, in some rare cases, configuration changes are required. These cases are described in the :ref:`Release Notes <releasenotes>`.

With Bareos version 16.2.4 the default configuration uses the :ref:`section-SubdirectoryConfigurationScheme`. This scheme offers various improvements. However, if your are updating from earlier versions, your existing single configuration files (:file:`/etc/bareos/bareos-*.conf`) stay in place and are contentiously used by Bareos. The new default configuration resource files will also be installed (:file:`/etc/bareos/bareos-*.d/*/*.conf`). However,
they will only be used, when the legacy configuration file does not exist.

See :ref:`section-UpdateToConfigurationSubdirectories` for details and how to migrate to :ref:`section-SubdirectoryConfigurationScheme`.

Updating the database scheme
----------------------------

Sometimes improvements in Bareos make it necessary to update the database scheme.



.. warning::

   If the Bareos catalog database does not have the current schema, the Bareos Director refuses to start.


Detailed information can then be found in the log file :file:`/var/log/bareos/bareos.log`.

Take a look into the :ref:`Release Notes <releasenotes>` to see which Bareos updates do require a database scheme update.



.. warning::

   Especially the upgrade to Bareos >= 17.2.0 restructures the **File** database table. In larger installations this is very time consuming (up to several hours or days) and temporarily doubles the amount of required database disk space.

Debian based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Since Bareos :sinceVersion:`14.2.0: dbconfig-common (Debian)` the Debian (and Ubuntu) based packages support the **dbconfig-common** mechanism to create and update the Bareos database. If this is properly configured, the database schema will be automatically adapted by the Bareos packages.

.. warning::

   When using the PostgreSQL backend and updating to Bareos < 14.2.3, it is necessary to manually grant database permissions, normally by using the following command:


.. code-block:: shell-session

   root@host:~# su - postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges


For details see :ref:`section-dbconfig`.

If you disabled the usage of **dbconfig-common**, follow the instructions for :ref:`section-UpdateDatabaseOtherDistributions`.

.. _section-UpdateDatabaseOtherDistributions:

Other Platforms
~~~~~~~~~~~~~~~

This has to be done as database administrator. On most platforms Bareos knows only about the credentials to access the Bareos database, but not about the database administrator to modify the database schema.

The task of updating the database schema is done by the script :command:`/usr/lib/bareos/scripts/update_bareos_tables`.

However, this script requires administration access to the database. Depending on your distribution and your database, this requires different preparations. More details can be found in chapter :ref:`Catalog Maintenance <CatMaintenanceChapter>`.



   .. warning::

      If you're updating to Bareos <= 13.2.3 and have configured the Bareos database during install using Bareos environment variables (``db_name``\ , ``db_user``\  or ``db_password``\ , see :ref:`CatMaintenanceChapter`), make sure to have these variables defined in the same way when calling the update and grant scripts. Newer versions of Bareos read these variables from the Director configuration file \configFileDirUnix. However, make sure that the user running the database scripts has read access to this file (or set the environment variables). The **postgres** user normally does not have the required permissions.

PostgreSQL
^^^^^^^^^^

If your are using PostgreSQL and your PostgreSQL administrator is **postgres** (default), use following commands:

.. code-block:: shell-session
   :caption: Update PostgreSQL database schema

   su postgres -c /usr/lib/bareos/scripts/update_bareos_tables
   su postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges

The :command:`grant_bareos_privileges` command is required, if new databases tables are introduced. It does not hurt to run it multiple times.

After this, restart the Bareos Director and verify it starts without problems.

MySQL/MariaDB
^^^^^^^^^^^^^

Make sure, that **root** has direct access to the local MySQL server. Check if the command :command:`mysql` without parameter connects to the database. If not, you may be required to adapt your local MySQL configuration file :file:`~/.my.cnf`. It should look similar to this:

.. code-block:: cfg
   :caption: MySQL credentials file .my.cnf

   [client]
   host=localhost
   user=root
   password=<input>YourPasswordForAccessingMysqlAsRoot</input>

If you are able to connect via the :command:`mysql` to the database, run the following script from the Unix prompt:

.. code-block:: shell-session
   :caption: Update MySQL database schema

   /usr/lib/bareos/scripts/update_bareos_tables

Currently on MySQL is it not necessary to run :command:`grant_bareos_privileges`, because access to the database is already given using wildcards.

After this, restart the Bareos Director and verify it starts without problems.
