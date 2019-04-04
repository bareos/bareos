The :strong:`Type`\  directive specifies the Job type, which is one of the following:

Backup
   | 
   | :index:`[TAG=Backup] <single: Backup>` Run a backup Job. Normally you will have at least one Backup job for each client you want to save. Normally, unless you turn off cataloging, most all the important statistics and data concerning files backed up will be placed in the catalog.

Restore
   | 
   | :index:`[TAG=Restore] <single: Restore>` Run a restore Job. Normally, you will specify only one Restore job which acts as a sort of prototype that you will modify using the console program in order to perform restores. Although certain basic information from a Restore job is saved in the catalog, it is very minimal compared to the information stored for a Backup job – for example, no File database entries are generated since no Files are saved.

   Restore jobs cannot be automatically started by the scheduler as is the case for Backup, Verify and Admin jobs. To restore files, you must use the restore command in the console.

Verify
   | 
   | :index:`[TAG=Verify] <single: Verify>` Run a verify Job. In general, verify jobs permit you to compare the contents of the catalog to the file system, or to what was backed up. In addition, to verifying that a tape that was written can be read, you can also use verify as a sort of tripwire intrusion detection.

Admin
   | 
   | :index:`[TAG=Admin] <single: Admin>` Run an admin Job. An Admin job can be used to periodically run catalog pruning, if you do not want to do it at the end of each Backup Job. Although an Admin job is recorded in the catalog, very little data is saved.

Migrate
   defines the job that is run as being a Migration Job. A Migration Job is a sort of control job and does not have any Files associated with it, and in that sense they are more or less like an Admin job. Migration jobs simply check to see if there is anything to Migrate then possibly start and control new Backup jobs to migrate the data from the specified Pool to another Pool. Note, any original JobId that is migrated will be marked as having been migrated, and the original JobId can nolonger
   be used for restores; all restores will be done from the new migrated Job.

Copy
   defines the job that is run as being a Copy Job. A Copy Job is a sort of control job and does not have any Files associated with it, and in that sense they are more or less like an Admin job. Copy jobs simply check to see if there is anything to Copy then possibly start and control new Backup jobs to copy the data from the specified Pool to another Pool. Note that when a copy is made, the original JobIds are left unchanged. The new copies can not be used for restoration unless you
   specifically choose them by JobId. If you subsequently delete a JobId that has a copy, the copy will be automatically upgraded to a Backup rather than a Copy, and it will subsequently be used for restoration.

Consolidate
   is used to consolidate Always Incremental Backups jobs, see :ref:`section-alwaysincremental`. It has been introduced in Bareos :sinceVersion:`16.2.4: Job Type Consolidate`.

Within a particular Job Type, there are also Levels, see :config:option:`dir/job/Level`\ .

