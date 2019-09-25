
.. _section-webui:

Bareos Webui
============

.. _section-webui-command-acl-requirements:

Command ACL Requirements
------------------------

The following tables show which commands are required and optional for each
module of the |webui|.

Optional commands may be denied by :config:option:`dir/profile/CommandACL`
settings to limit specific functionality. If you deny a required command,
the module will not work.

.. note::

   The commands :bcommand:`.api`, :bcommand:`.help` and :bcommand:`use` are essential commands
   and should never be denied by :config:option:`dir/profile/CommandACL` settings in your
   :config:option:`dir/console` or :config:option:`dir/profile` resources.

.. csv-filter:: Client Module
   :header-rows: 1
   :included_cols: 0,1
   :exclude: {1: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Dashboard Module
   :header-rows: 1
   :included_cols: 0,3
   :exclude: {3: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Director Module
   :header-rows: 1
   :included_cols: 0,4
   :exclude: {4: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Fileset Module
   :header-rows: 1
   :included_cols: 0,5
   :exclude: {5: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Job Module
   :header-rows: 1
   :included_cols: 0,6
   :exclude: {6: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Volume/Media Module
   :header-rows: 1
   :included_cols: 0,7
   :exclude: {7: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Pool Module
   :header-rows: 1
   :included_cols: 0,8
   :exclude: {8: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Restore Module
   :header-rows: 1
   :included_cols: 0,9
   :exclude: {9: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Schedule Module
   :header-rows: 1
   :included_cols: 0,10
   :exclude: {10: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Storage Module
   :header-rows: 1
   :included_cols: 0,11
   :exclude: {11: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. note::

   For the web console in the |webui| all commands are optional besides
   the essential commands already mentioned above, :bcommand:`.api`,
   :bcommand:`.help` and :bcommand:`use`.


A complete overview of bconsole command usage in the |webui| can be found in
the Developer Guide chapter ":ref:`section-dev-webui-command-usage-in-modules`".

.. _section-webui-access-control-configuration:

Access Control Configuration
----------------------------

Access Control is configured in :config:option:`dir/profile` resources.

Below are some example profile resources that should serve you as guidance to
configure access to certain elements of the |webui| to your needs and use cases.

Full Access
~~~~~~~~~~~

No restrictions are given by :config:option:`dir/profile`,
everything is allowed.

.. code-block:: cfg
   :caption: Profile Resource - Administrator Access Example

   Profile {
      Name = "webui-admin"
      CommandACL = *all*
      JobACL = *all*
      ScheduleACL = *all*
      CatalogACL = *all*
      PoolACL = *all*
      StorageACL = *all*
      ClientACL = *all*
      FilesetACL = *all*
      WhereACL = *all*
   }

Limited Access
~~~~~~~~~~~~~~~

Users with the following profile example have limited access to various resources
but they are allowed to :bcommand:`run`, :bcommand:`rerun` and :bcommand:`cancel`
the jobs :strong:`backup-bareos-fd` and :strong:`backup-example-fd`.

.. note::

   Access to depending resources for the jobs set in the
   :config:option:`dir/profile/JobACL` needs also be given by
   :config:option:`dir/profile/ClientACL`,
   :config:option:`dir/profile/PoolACL`,
   :config:option:`dir/profile/StorageACL` and
   :config:option:`dir/profile/FilesetACL` settings.

Users of this profile are also able to do a restore from within the |webui| by
having access to the RestoreFiles job resource, the required Bvfs API commands
and the :bcommand:`restore` command itself.

.. code-block:: cfg
   :caption: Profile Resource - Limited Access Example

   Profile {
      Name = "webui-user"
      # Multiple CommandACL directives as given below are concatenated
      CommandACL = .api, .help, use, version, status, show
      CommandACL = list, llist
      CommandACL = run, rerun, cancel, restore
      CommandACL = .clients, .jobs, .filesets, .pools, .storages, .defaults, .schedule
      CommandACL = .bvfs_update, .bvfs_get_jobids, .bvfs_lsdirs, .bvfs_lsfiles
      CommandACL = .bvfs_versions, .bvfs_restore, .bvfs_cleanup
      JobACL = backup-bareos-fd, backup-example-fd, RestoreFiles
      ScheduleACL = WeeklyCycle
      CatalogACL = MyCatalog
      PoolACL = Full, Differential, Incremental
      StorageACL = File
      ClientACL = bareos-fd, example-fd
      FilesetACL = SelfTest, example-fileset
      WhereACL = *all*
   }

Read-Only Access
~~~~~~~~~~~~~~~~

This example profile resource denies access to most of the commands and
additionally restricts access to certain other resources like
:config:option:`dir/Job`, :config:option:`dir/Schedule`,
:config:option:`dir/Pool`, :config:option:`dir/Storage`,
:config:option:`dir/Client`, :config:option:`dir/Fileset`, etc.

Users of this profile would not be able to run or restore jobs, execute volume
and autochanger related operations, enable or disable resources besides other
restrictions.

.. code-block:: cfg
   :caption: Profile Resource - Read-Only Access Example 1

   Profile {
     Name = "webui-user-readonly-example-1"

     # Deny general command access
     CommandACL = !.bvfs_clear_cache, !.exit, !configure, !purge, !prune, !reload
     CommandACL = !create, !update, !delete, !disable, !enable
     CommandACL = !show, !status

     # Deny job related command access
     CommandACL = !run, !rerun, !restore, !cancel

     # Deny autochanger related command access
     CommandACL = !mount, !umount, !unmount, !export, !import, !move, !release, !automount

     # Deny media/volume related command access
     CommandACL = !add, !label, !relabel, !truncate

     # Deny SQL related command access
     CommandACL = !sqlquery, !query, !.sql

     # Deny debugging related command access
     CommandACL = !setdebug, !trace

     # Deny network related command access
     CommandACL = !setbandwidth, !setip, !resolve

     # Allow non-excluded command access
     CommandACL = *all*

     # Allow access to the following job resources
     Job ACL = backup-bareos-fd, RestoreFiles

     # Allow access to the following schedule resources
     Schedule ACL = WeeklyCycle

     # Allow access to the following catalog resources
     Catalog ACL = MyCatalog

     # Deny access to the following pool resources
     Pool ACL = !Scratch

     # Allow access to non-excluded pool resources
     Pool ACL = *all*

     # Allow access to the following storage resources
     Storage ACL = File

     # Allow access to the following client resources
     Client ACL = bareos-fd

     # Allow access to the following filset resources
     FileSet ACL = SelfTest

     # Allow access to restore to any filesystem location
     Where ACL = *all*
   }

Alternatively the example above can be configured as following if you prefer a
shorter version.

.. code-block:: cfg
   :caption: Profile Resource - Read-Only Access Example 2

   Profile {
     Name = "webui-user-readonly-example-2"

     # Allow access to the following commands
     CommandACL = .api, .help, use, version, status
     CommandACL = list, llist
     CommandACL = .clients, .jobs, .filesets, .pools, .storages, .defaults, .schedule
     CommandACL = .bvfs_lsdirs, .bvfs_lsfiles, .bvfs_update, .bvfs_get_jobids, .bvfs_versions, .bvfs_restore

     # Allow access to the following job resources
     Job ACL = backup-bareos-fd, RestoreFiles

     # Allow access to the following schedule resources
     Schedule ACL = WeeklyCycle

     # Allow access to the following catalog resources
     Catalog ACL = MyCatalog

     # Allow access to the following  pool resources
     Pool ACL = Full, Differential, Incremental

     # Allow access to the following storage resources
     Storage ACL = File

     # Allow access to the following client resources
     Client ACL = bareos-fd

     # Allow access to the following filset resources
     FileSet ACL = SelfTest

     # Allow access to restore to any filesystem location
     Where ACL = *all*
   }

For more details, please read :ref:`DirectorResourceProfile`.

.. _section-webui-restore:

Restore
-------

By default when running a restore in the |webui| the most recent version of all files from the available backups will be restored. You can change this behaviour by selecting the merge strategy and specific job selections in the fields described below. The |webui| allows you to restore multiple files or specific file versions.

.. _section-webui-restore-parameters:

Available restore parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-0.*
   :width: 80%

Client

   A list of available backup clients.


Backup jobs

   A list of successful backup jobs available for the selected client.


Merge all client filesets

   Determines if all available backup job filesets for the selected client should be merged into one file tree. This is helpful i.e. if multiple backup jobs with different filesets are available for the selected client. When you are just interested in a specific backup job, disable merging here and make the appropriate selection of a backup job.


Merge all related jobs to last full backup of selected backup job

   By default all most recent versions of a file from your incremental, differential and full backup jobs will be merged into the file tree. If this behaviour is not desirable and instead the file tree should show the contents of a particular backup job, set the value to "No" here. Select a specific backup job afterwards to browse through the according file tree which has been backed up by that job.


Restore to client

   In case you do not want to restore to the original client, you can select an alternative client here.


Restore job

   Sometimes dedicated restore jobs may be required, which can be selected here.


Replace files on client

   Here you can change the behaviour of how and when files should be replaced on the backup client while restoring.

      - always
      - never
      - if file being restored is older than existing file
      - if file being restored is newer than existing file


Restore location on client

   If you like to restore all files to the original location then enter a single :file:`/` here but keep the settings of "Replace files on client" in mind.

   In case you want to use another location, simply enter the path here where you want to restore to on the selected client, for example :file:`/tmp/bareos-restore/`.


Restore multiple files
~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-1.*
   :width: 80%

Restore a specific file version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-2.*
   :width: 80%

