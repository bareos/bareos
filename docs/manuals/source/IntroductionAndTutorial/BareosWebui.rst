
.. _section-webui:

Bareos Webui
============

This chapter addresses the usage of the |webui|.

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

