
.. _section-webui:

Bareos Webui
============

This chapter addresses the usage of the |webui|.

.. _section-webui-restore:

Restore
-------

By default settings the restore in |webui| is basically doing a restore of a backup client with the most recent version
of all files from the available backups. You can change this behaviour by selecting how things should be merged and
specific job selections. The |webui| allows you to restore multiple files or specific file versions.

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

   Determines if all available backup job filesets for the selected client should be merged into one file tree.
   If multiple backup jobs with different filesets are available for the selected client this might come in handy,
   when you are just interested in a specific backup job. In that case, disable merging here and make the proper
   selection of a backup job.


Merge all related jobs to last full backup of selected backup job

   By default all most recent versions of a file from your incremental, differential and full backup jobs will be merged into the
   file tree. If you don't want this behaviour and would like to browse the file tree of a particular backup job, set the value to "No"
   here. Select a specific backup job afterwards to browse through the specific file tree which has been backed up by that job.


Restore to client

   In case you don't want to restore to the original client, you can select an alternative client here.


Restore job

   Sometimes "special" restore jobs may be required, which can be selected here.


Replace files on client

   Here you can change the behaviour of how and when files should be replaced on the backup client while restoring.

      - always
      - never
      - if file being restored is older than existing file
      - if file being restored is newer than existing file


Restore location on client

   If you like to restore all files to the original location provide a :file:`/` here but keep the replace files setting on client in mind.

   In case you want to use another location, simply provide the path here where you want to restore them on the client selected,
   for example :file:`/tmp/bareos-restore/`.


Restore multiple files
~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-1.*
   :width: 80%

Restore a specific file version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-2.*
   :width: 80%

