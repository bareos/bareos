.. _SpoolingChapter:

Data Spooling
=============



.. _section-spooling:

 

.. _section-DataSpooling:

 :index:`[TAG=Data Spooling] <single: Data Spooling>` :index:`[TAG=Spooling->Data] <pair: Spooling; Data>`

Bareos allows you to specify that you want the Storage daemon to initially write your data to disk and then subsequently to tape. This serves several important purposes.

-  It takes a long time for data to come in from the File daemon during an Incremental backup. If it is directly written to tape, the tape will start and stop or shoe-shine as it is often called causing tape wear. By first writing the data to disk, then writing it to tape, the tape can be kept in continual motion.

-  While the spooled data is being written to the tape, the despooling process has exclusive use of the tape. This means that you can spool multiple simultaneous jobs to disk, then have them very efficiently despooled one at a time without having the data blocks from several jobs intermingled, thus substantially improving the time needed to restore files. While despooling, all jobs spooling continue running.

-  Writing to a tape can be slow. By first spooling your data to disk, you can often reduce the time the File daemon is running on a system, thus reducing downtime, and/or interference with users. Of course, if your spool device is not large enough to hold all the data from your File daemon, you may actually slow down the overall backup.

Data spooling is exactly that "spooling". It is not a way to first write a "backup" to a disk file and then to a tape. When the backup has only been spooled to disk, it is not complete yet and cannot be restored until it is written to tape.

Bareos also supports writing a backup to disk then later migrating or moving it to a tape (or any other medium). For details on this, please see the :ref:`MigrationChapter` chapter of this manual for more details.

The remainder of this chapter explains the various directives that you can use in the spooling process.

Data Spooling Directives
------------------------

:index:`[TAG=Data Spooling->Directives] <pair: Data Spooling; Directives>`

The following directives can be used to control data spooling.

-  Turn data spooling on/off at the Job level: :config:option:`dir/job/SpoolData`\  = :strong:`Yes|No`

-  This setting can be overwritten in a Schedule :config:option:`dir/schedule/Run`\  directive: ``SpoolData=``:strong:`Yes|No`

-  To override the Job specification in a bconsole session using the :bcommand:`run` command: ``SpoolData=``:strong:`Yes|No`

   Please note that this does not refer to a configuration statement, but to an argument for the run command.

-  To limit the the maximum spool file size for a particular job: :config:option:`dir/job/SpoolSize`\ 

-  To limit the maximum total size of the spooled data for a particular device: :config:option:`sd/device/MaximumSpoolSize`\ 

-  To limit the maximum total size of the spooled data for a particular device for a single job: :config:option:`sd/device/MaximumJobSpoolSize`\ 

-  To specify the spool directory for a particular device: :config:option:`sd/device/SpoolDirectory`\ 

Additional Notes
~~~~~~~~~~~~~~~~

-  

   

   .. warning::
      Exclude your the spool directory from any backup,
      otherwise, your job will write enormous amounts of data to the Volume, and
      most probably terminate in error. This is because in attempting to backup the
      spool file, the backup data will be written a second time to the spool file,
      and so on ad infinitum.

-  Another advice is to always specify the :config:option:`sd/device/MaximumSpoolSize`\  so that your disk doesn’t completely fill up. In principle, data spooling will properly detect a full disk, and despool data allowing the job to continue. However, attribute spooling is not so kind to the user. If the disk on which attributes are being spooled fills, the job will be canceled. In addition, if your working directory is on the same partition as the spool directory, then
   Bareos jobs will fail possibly in bizarre ways when the spool fills.

-  When data spooling is enabled, Bareos automatically turns on attribute spooling. In other words, it also spools the catalog entries to disk. This is done so that in case the job fails, there will be no catalog entries pointing to non-existent tape backups.

-  Attribute despooling occurs near the end of a job. The Storage daemon accumulates file attributes during the backup and sends them to the Director at the end of the job. The Director then inserts the file attributes into the catalog. During this insertion, the tape drive may be inactive. When the file attribute insertion is completed, the job terminates.

-  Attribute spool files are always placed in the working directory of the Storage daemon.

-  When Bareos begins despooling data spooled to disk, it takes exclusive use of the tape. This has the major advantage that in running multiple simultaneous jobs at the same time, the blocks of several jobs will not be intermingled.

-  It is probably best to provide as large a spool file as possible to avoid repeatedly spooling/despooling. Also, while a job is despooling to tape, the File daemon must wait (i.e. spooling stops for the job while it is despooling).

-  If you are running multiple simultaneous jobs, Bareos will continue spooling other jobs while one is despooling to tape, provided there is sufficient spool file space.




