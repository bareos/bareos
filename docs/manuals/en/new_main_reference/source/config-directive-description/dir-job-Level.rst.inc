The Level directive specifies the default Job level to be run. Each different :config:option:`dir/job/Type`\  (Backup, Restore, Verify, ...) has a different set of Levels that can be specified. The Level is normally overridden by a different value that is specified in the :ref:`DirectorResourceSchedule`. This directive is not required, but must be specified either by this directive or as an override specified in the
:ref:`DirectorResourceSchedule`.

Backup
   | 
   | For a Backup Job, the Level may be one of the following:

   Full
      | 
      | :index:`\ <single: Full>`\  When the Level is set to Full all files in the FileSet whether or not they have changed will be backed up.

   Incremental
      | 
      | :index:`\ <single: Incremental>`\  When the Level is set to Incremental all files specified in the FileSet that have changed since the last successful backup of the the same Job using the same FileSet and Client, will be backed up. If the Director cannot find a previous valid Full backup then the job will be upgraded into a Full backup. When the Director looks for a valid backup record in the catalog database, it looks for a previous Job with:

      -  The same Job name.

      -  The same Client name.

      -  The same FileSet (any change to the definition of the FileSet such as adding or deleting a file in the Include or Exclude sections constitutes a different FileSet.

      -  The Job was a Full, Differential, or Incremental backup.

      -  The Job terminated normally (i.e. did not fail or was not canceled).

      -  The Job started no longer ago than Max Full Interval.

      If all the above conditions do not hold, the Director will upgrade the Incremental to a Full save. Otherwise, the Incremental backup will be performed as requested.

      The File daemon (Client) decides which files to backup for an Incremental backup by comparing start time of the prior Job (Full, Differential, or Incremental) against the time each file was last "modified" (st_mtime) and the time its attributes were last "changed"(st_ctime). If the file was modified or its attributes changed on or after this start time, it will then be backed up.

      Some virus scanning software may change st_ctime while doing the scan. For example, if the virus scanning program attempts to reset the access time (st_atime), which Bareos does not use, it will cause st_ctime to change and hence Bareos will backup the file during an Incremental or Differential backup. In the case of Sophos virus scanning, you can prevent it from resetting the access time (st_atime) and hence changing st_ctime by using the :strong:`--no-reset-atime` option.
      For other software, please see their manual.

      When Bareos does an Incremental backup, all modified files that are still on the system are backed up. However, any file that has been deleted since the last Full backup remains in the Bareos catalog, which means that if between a Full save and the time you do a restore, some files are deleted, those deleted files will also be restored. The deleted files will no longer appear in the catalog after doing another Full save.

      In addition, if you move a directory rather than copy it, the files in it do not have their modification time (st_mtime) or their attribute change time (st_ctime) changed. As a consequence, those files will probably not be backed up by an Incremental or Differential backup which depend solely on these time stamps. If you move a directory, and wish it to be properly backed up, it is generally preferable to copy it, then delete the original.

      However, to manage deleted files or directories changes in the catalog during an Incremental backup you can use :ref:`accuratemode`. This is quite memory consuming process.

   Differential
      | 
      | :index:`\ <single: Differential>`\  When the Level is set to Differential all files specified in the FileSet that have changed since the last successful Full backup of the same Job will be backed up. If the Director cannot find a valid previous Full backup for the same Job, FileSet, and Client, backup, then the Differential job will be upgraded into a Full backup. When the Director looks for a valid Full backup record in the catalog database, it looks for a previous Job with:

      -  The same Job name.

      -  The same Client name.

      -  The same FileSet (any change to the definition of the FileSet such as adding or deleting a file in the Include or Exclude sections constitutes a different FileSet.

      -  The Job was a FULL backup.

      -  The Job terminated normally (i.e. did not fail or was not canceled).

      -  The Job started no longer ago than Max Full Interval.

      If all the above conditions do not hold, the Director will upgrade the Differential to a Full save. Otherwise, the Differential backup will be performed as requested.

      The File daemon (Client) decides which files to backup for a differential backup by comparing the start time of the prior Full backup Job against the time each file was last "modified" (st_mtime) and the time its attributes were last "changed" (st_ctime). If the file was modified or its attributes were changed on or after this start time, it will then be backed up. The start time used is displayed after the Since on the Job report. In rare cases, using the start time of the prior backup
      may cause some files to be backed up twice, but it ensures that no change is missed.

      When Bareos does a Differential backup, all modified files that are still on the system are backed up. However, any file that has been deleted since the last Full backup remains in the Bareos catalog, which means that if between a Full save and the time you do a restore, some files are deleted, those deleted files will also be restored. The deleted files will no longer appear in the catalog after doing another Full save. However, to remove deleted files from the catalog during a
      Differential backup is quite a time consuming process and not currently implemented in Bareos. It is, however, a planned future feature.

      As noted above, if you move a directory rather than copy it, the files in it do not have their modification time (st_mtime) or their attribute change time (st_ctime) changed. As a consequence, those files will probably not be backed up by an Incremental or Differential backup which depend solely on these time stamps. If you move a directory, and wish it to be properly backed up, it is generally preferable to copy it, then delete the original. Alternatively, you can move the directory, then
      use the touch program to update the timestamps.

      However, to manage deleted files or directories changes in the catalog during an Differential backup you can use :ref:`accurate mode <accuratemode>`. This is quite memory consuming process. See for more details.

      Every once and a while, someone asks why we need Differential backups as long as Incremental backups pickup all changed files. There are possibly many answers to this question, but the one that is the most important for me is that a Differential backup effectively merges all the Incremental and Differential backups since the last Full backup into a single Differential backup. This has two effects: 1. It gives some redundancy since the old backups could be used if the merged backup cannot
      be read. 2. More importantly, it reduces the number of Volumes that are needed to do a restore effectively eliminating the need to read all the volumes on which the preceding Incremental and Differential backups since the last Full are done.

   VirtualFull
      | 
      | :index:`\ <single: VirtualFull Backup>`\  

.. _VirtualFull:

 When the Level is set to VirtualFull, a new Full backup is generated from the last existing Full backup and the matching Differential- and Incremental-Backups. It matches this according the :config:option:`dir/client/Name`\  and :config:option:`dir/fileset/Name`\ . This means, a new Full backup get created without transfering all the data from the client to the backup
        server again. The new Full backup will be stored in the pool defined in :config:option:`dir/pool/NextPool`\ .

      

         .. warning::

            Opposite to the other backup levels, VirtualFull may require read and write access to multiple volumes. In most cases you have to make sure, that Bareos does not try to read and write to the same Volume.

Restore
   | 
   | For a Restore Job, no level needs to be specified.

Verify
   | 
   | For a Verify Job, the Level may be one of the following:

   InitCatalog
      | 
      | :index:`\ <single: InitCatalog>`\  does a scan of the specified FileSet and stores the file attributes in the Catalog database. Since no file data is saved, you might ask why you would want to do this. It turns out to be a very simple and easy way to have a Tripwire like feature using Bareos. In other words, it allows you to save the state of a set of files defined by the FileSet and later check to see if those files have been modified or deleted and if any new files have been added.
        This can be used to detect system intrusion. Typically you would specify a FileSet that contains the set of system files that should not change (e.g. /sbin, /boot, /lib, /bin, ...). Normally, you run the InitCatalog level verify one time when your system is first setup, and then once again after each modification (upgrade) to your system. Thereafter, when your want to check the state of your system files, you use a Verify level = Catalog. This compares the results of your InitCatalog
        with the current state of the files.

   Catalog
      | 
      | :index:`\ <single: Catalog>`\  Compares the current state of the files against the state previously saved during an InitCatalog. Any discrepancies are reported. The items reported are determined by the verify options specified on the Include directive in the specified FileSet (see the FileSet resource below for more details). Typically this command will be run once a day (or night) to check for any changes to your system files.

      

         .. warning::

            If you run two Verify Catalog jobs on the same client at
            the same time, the results will certainly be incorrect.  This is because
            Verify Catalog modifies the Catalog database while running in order to
            track new files.

   VolumeToCatalog
      | 
      | :index:`\ <single: VolumeToCatalog>`\  This level causes Bareos to read the file attribute data written to the Volume from the last backup Job for the job specified on the VerifyJob directive. The file attribute data are compared to the values saved in the Catalog database and any differences are reported. This is similar to the DiskToCatalog level except that instead of comparing the disk file attributes to the catalog database, the attribute data written to the Volume is read and
        compared to the catalog database. Although the attribute data including the signatures (MD5 or SHA1) are compared, the actual file data is not compared (it is not in the catalog).

      VolumeToCatalog jobs need a client to extract the metadata, but this client does not have to be the original client. We suggest to use the client on the backup server itself for maximum performance.

      

         .. warning::

            If you run two Verify VolumeToCatalog jobs on the same
            client at the same time, the results will certainly be incorrect.  This
            is because the Verify VolumeToCatalog modifies the Catalog database
            while running.

   DiskToCatalog
      | 
      | :index:`\ <single: DiskToCatalog>`\  This level causes Bareos to read the files as they currently are on disk, and to compare the current file attributes with the attributes saved in the catalog from the last backup for the job specified on the VerifyJob directive. This level differs from the VolumeToCatalog level described above by the fact that it doesn’t compare against a previous Verify job but against a previous backup. When you run this level, you must supply the verify options
        on your Include statements. Those options determine what attribute fields are compared.

      This command can be very useful if you have disk problems because it will compare the current state of your disk against the last successful backup, which may be several jobs.

      Note, the current implementation does not identify files that have been deleted.

