.. _backward-compatibility:

Backward Compatibility
======================

:index:`\ <single: Compatibility; Backward>`\


.. _backward-compatibility-tape-format:

Tape Formats
------------

:index:`\ <single: Tape; Format>`\

One of the major goals of Backup software is to ensure that you can restore tapes (the word tape should also include disk volumes) that you wrote years ago. This means that each new version of the software should be able to read old format tapes. The first problem you will have is to ensure that the hardware is still working some years down the road, and the second problem will be to ensure that the media will still be good, then your OS must be able to interface to the device, and finally
Bareos must be able to recognize old formats. All the problems except the last are ones that we cannot solve, but by careful planning you can.

Since the very beginning of Bacula (January 2000) until today (2015), there have been three major Bacula/Bareos tape formats. The second format was introduced in Bacula version 1.27 in November of 2002. Bareos has been required to adapt the tape format to avoid potential trademark issues, but is able to read also the old Bacula tape formats.

Though the tape format is basically fixed, the kinds of data that we can put on the tapes are extensible, and that is how we added new features such as ACLs, Win32 data, encrypted data, ... Obviously, an older version of Bacula/Bareos would not know how to read these newer data streams, but each newer version of Bareos should know how to read all the older streams.


.. _compat-bacula:

Compatibility between Bareos and Bacula
---------------------------------------

:index:`\ <single: Compatibility; Bacula>`
:index:`\ <single: Bacula>`

A Director and a Storage Daemon should (must) always run at the same version. This is true for Bareos as well as for Bacula. It is not possible to mix these components. This is because the protocol between Director and Storage Daemon itself is not versioned (also true for Bareos and Bacula). If you want to be able to switch back from Bareos to Bacula after using a Bareos director and storage daemon you have to enable the compatible mode in the Bareos storage daemon to have it write the data in
the same format as the Bacula storage daemon.

The |fd| is compatible with all version of the Bacula director (tested with version 5.2.13 and lower) when you enable the compatible mode in the configuration of the |fd|. The compatible option was set by default in Bareos < 15.2.0, and is disabled by default since :sinceVersion:`15.2.0: Compatible = no`.

To be sure this is enabled you can explicitly set the compatible option:

:config:option:`fd/client/Compatible = True`\

A |dir| can only talk to Bacula file daemons of version 2.0 or higher. Through a change in the Bacula network protocols, it is currently not possible to use a Bacula file daemon > 6.0 with a |dir|.

These combinations of Bareos and Bacula are know to work together:

============ ================== =========================== ===========
**Director** **Storage Daemon** **File Daemon**             **Remarks**
============ ================== =========================== ===========
Bareos       Bareos             Bareos 
Bareos       Bareos             2.0 <= Bacula < 6.0
Bacula       Bacula             Bacula 
Bacula       Bacula             Bareos (compatibility mode)
============ ================== =========================== ===========

Other combinations like Bacula Director with |sd| will not work. However this wasn’t even possible with different versions of bacula-dir and bacula-sd.

.. _upgrade-from-bacula-to-bareos:

Upgrade from Bacula 5.2 to Bareos
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Upgrade from Bacula to Bareos>`
:index:`\ <single: Bareos; Upgrading>`

Upgrade is supported from Bacula version 5.2.x. If you are running any older version of Bacula, please update to 5.2 first (see Bacula documentation).



.. warning::

   Updating from Bacula >= 7.0 to Bareos has not been tested.



.. warning::

   As Bareos and Bacula packages bring binaries with identical paths and names,
   it is on most platforms not possible to install components from both in parallel.
   Your package management tool will warn you about this.


Rename user and group before upgrading
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To have bareos running without any permission hassle, it is recommended to rename the user and group **bacula** to the user and group **bareos** before upgrading. That way, we minimize the effort for the user to recheck all config files and the rights on every script/directory etc. involved in the existing setup.

The required commands should look something like this:

.. code-block:: shell-session

   usermod  -l bareos bacula
   groupmod -n bareos bacula

MySQL
^^^^^

Proceed with the following steps:

-  Stop bacula services

-  Backup your catalog database:

   .. code-block:: shell-session

      mysqldump bacula > /tmp/bacula_5.2.sql

-  Make the user bareos have the same userid and the group bareos the same groupid as the user/group bacula had before. This will solve a lot of rights problems.

-  Install Bareos packages

-  Run the update script on the old bacula database:

   .. code-block:: shell-session

      export db_name=bacula
      /usr/lib/bareos/update_bareos_tables
      unset db_name

-  Backup upgraded DB:

   .. code-block:: shell-session

      mysqldump bacula > /tmp/bacula.sql

-  Create bareos database:

   .. code-block:: shell-session

      /usr/lib/bareos/create_bareos_database

-  Insert backed up db into new database:

   .. code-block:: shell-session

      cat /tmp/bacula.sql | mysql bareos

-  Grant permissions:

   .. code-block:: shell-session

      /usr/lib/bareos/grant_mysql_privileges

-  Adapt file permissions to bareos, if you have any file storage

-  Adapt configs (not complete)

   -  With bacula the default setting for pid files was :file:`/var/run`, which may not work if the bareos-director runs as user bareos. Best way is to comment out the entry :command:`Pid Directory = "/var/run"` in your director config. Bareos will set a working default value (supposed to be :file:`/var/lib/bareos/`)

PostgreSQL
^^^^^^^^^^

Renaming a postgresql database:

-  Become postgresql user

-  psql template1

   .. code-block:: shell-session

      ALTER DATABASE bacula RENAME TO bareos;
      ALTER USER bacula RENAME TO bareos;
      ALTER USER bareos UNENCRYPTED PASSWORD 'password';
