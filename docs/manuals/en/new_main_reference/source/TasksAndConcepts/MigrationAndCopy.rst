.. _MigrationChapter:

Migration and Copy
==================

:index:`\ <single: Migration>` :index:`\ <single: Copy>`

The term Migration, as used in the context of Bareos, means moving data from one Volume to another. In particular it refers to a Job (similar to a backup job) that reads data that was previously backed up to a Volume and writes it to another Volume. As part of this process, the File catalog records associated with the first backup job are purged. In other words, Migration moves Bareos Job data from one Volume to another by reading the Job data from the Volume it is stored on, writing it to a
different Volume in a different Pool, and then purging the database records for the first Job.

The Copy process is essentially identical to the Migration feature with the exception that the Job that is copied is left unchanged. This essentially creates two identical copies of the same backup. However, the copy is treated as a copy rather than a backup job, and hence is not directly available for restore. If Bareos finds a copy when a job record is purged (deleted) from the catalog, it will promote the copy as *real* backup and will make it available for automatic restore.

Copy and Migration jobs do not involve the File daemon.

Jobs can be selected for migration based on a number of criteria such as:

-  a single previous Job

-  a Volume

-  a Client

-  a regular expression matching a Job, Volume, or Client name

-  the time a Job has been on a Volume

-  high and low water marks (usage or occupation) of a Pool

-  Volume size

The details of these selection criteria will be defined below.

To run a Migration job, you must first define a Job resource very similar to a Backup Job but with :config:option:`dir/job/Type`\  = Migrate instead of :config:option:`dir/job/Type`\  = Backup. One of the key points to remember is that the Pool that is specified for the migration job is the only pool from which jobs will be migrated, with one exception noted below. In addition, the Pool to which the selected Job or Jobs will be migrated is defined by the
:config:option:`dir/pool/NextPool`\  = ... in the Pool resource specified for the Migration Job.

Bareos permits Pools to contain Volumes of different Media Types. However, when doing migration, this is a very undesirable condition. For migration to work properly, you should use Pools containing only Volumes of the same Media Type for all migration jobs.

A migration job can be started manually or from a Schedule, like a backup job. It searches for existing backup Jobs that match the parameters specified in the migration Job resource, primarily a :config:option:`dir/job/SelectionType`\ . If no match was found, the Migration job terminates without further action. Otherwise, for each Job found this way, the Migration Job will run a new Job which copies the Job data to a new Volume in the Migration Pool.

Normally three jobs are involved during a migration:

-  The Migration control Job which starts the migration child Jobs.

-  The previous Backup Job (already run). The File records of this Job are purged when the Migration job terminates successfully. The data remain on the Volume until it is recycled.

-  A new Migration Backup Job that moves the data from the previous Backup job to the new Volume. If you subsequently do a restore, the data will be read from this Job.

If the Migration control Job finds more than one existing Job to migrate, it creates one migration job for each of them. This may result in a large number of Jobs. Please note that Migration doesn’t scale too well if you migrate data off of a large Volume because each job must read the same Volume, hence the jobs will have to run consecutively rather than simultaneously.

Important Migration Considerations
----------------------------------

:index:`\ <single: Migration; Important Migration Considerations>`

-  Each Pool into which you migrate Jobs or Volumes must contain Volumes of only one :config:option:`dir/storage/MediaType`\ .

-  Migration takes place on a JobId by JobId basis. That is each JobId is migrated in its entirety and independently of other JobIds. Once the Job is migrated, it will be on the new medium in the new Pool, but for the most part, aside from having a new JobId, it will appear with all the same characteristics of the original job (start, end time, ...). The column RealEndTime in the catalog Job table will contain the time and date that the Migration terminated, and by comparing it with the EndTime
   column you can tell whether or not the job was migrated. Also, the Job table contains a PriorJobId column which is set to the original JobId for migration jobs. For non-migration jobs this column is zero.

-  After a Job has been migrated, the File records are purged from the original Job. Moreover, the Type of the original Job is changed from "B" (backup) to "M" (migrated), and another Type "B" job record is added which refers to the new location of the data. Since the original Job record stays in the bareos catalog, it is still possible to restore from the old media by specifying the original JobId for the restore. However, no file selection is possible in this case, so one can only restore all
   files this way.

-  A Job will be migrated only if all Volumes on which the job is stored are marked Full, Used, or Error. In particular, Volumes marked Append will not be considered for migration which rules out the possibility that new files are appended to a migrated Volume. This policy also prevents deadlock situations, like attempting to read and write the same Volume from two jobs at the same time.

-  Migration works only if the Job resource of the original Job is still defined in the current Director configuration. Otherwise you’ll get a fatal error.

-  Setting the :config:option:`dir/pool/MigrationHighBytes`\  watermark is not sufficient for migration to take place. In addition, you must define and schedule a migration job which looks for jobs that can be migrated.

-  Bareos currently does only minimal Storage conflict resolution, so you must take care to ensure that you don’t try to read and write to the same device or Bareos may block waiting to reserve a drive that it will never find. A way to prevent problems is that all your migration pools contain only one :config:option:`dir/storage/MediaType`\ , and that you always migrate to a pool with a different Media Type.

-  The :config:option:`dir/pool/NextPool`\  = ... directive must be defined in the Pool referenced in the Migration Job to define the Pool into which the data will be migrated.

-  Migration has only be tested carefully for the "Job" and "Volume" selection types. All other selection types (time, occupancy, smallest, oldest, ...) are experimental features.

-  To figure out which jobs are going to be migrated by a given configuration, choose a debug level of 100 or more. This activates information about the migration selection process.

Configure Copy or Migration Jobs
--------------------------------

The following directives can be used to define a Copy or Migration job:

Job Resource
''''''''''''

-  :config:option:`dir/job/Type`\  = Migrate|Copy

-  

   :config:option:`dir/job/SelectionType`\ 

-  

   :config:option:`dir/job/SelectionPattern`\ 

-  | :config:option:`dir/job/Pool`\ 
   | For :config:option:`dir/job/SelectionType`\  other than SQLQuery, this defines what Pool will be examined for finding JobIds to migrate

-  

   :config:option:`dir/job/PurgeMigrationJob`\ 

Pool Resource
'''''''''''''

-  | :config:option:`dir/pool/NextPool`\ 
   | to what pool Jobs will be migrated

-  | :config:option:`dir/pool/MigrationTime`\ 
   | if :config:option:`dir/job/SelectionType`\  = PoolTime

-  | :config:option:`dir/pool/MigrationHighBytes`\ 
   | if :config:option:`dir/job/SelectionType`\  = PoolOccupancy

-  | :config:option:`dir/pool/MigrationLowBytes`\ 
   | optional if :config:option:`dir/job/SelectionType`\  = PoolOccupancy is used

-  | :config:option:`dir/pool/Storage`\ 
   | if Copy/Migration involves multiple Storage Daemon, see :ref:`section-CopyMigrationJobsMultipleStorageDaemons`

Example Migration Jobs
~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Example; Migration Jobs>`

Assume a simple configuration with a single backup job as described below.

.. code-block:: bareosconfig
   :caption: Backup Job

   # Define the backup Job
   Job {
     Name = "NightlySave"
     Type = Backup
     Level = Incremental                 # default
     Client=rufus-fd
     FileSet="Full Set"
     Schedule = "WeeklyCycle"
     Messages = Standard
     Pool = Default
   }

   # Default pool definition
   Pool {
     Name = Default
     Pool Type = Backup
     AutoPrune = yes
     Recycle = yes
     Next Pool = Tape
     Storage = File
     LabelFormat = "File"
   }

   # Tape pool definition
   Pool {
     Name = Tape
     Pool Type = Backup
     AutoPrune = yes
     Recycle = yes
     Storage = DLTDrive
   }

   # Definition of File storage device
   Storage {
     Name = File
     Address = rufus
     Password = "secret"
     Device = "File"          # same as Device in Storage daemon
     Media Type = File        # same as MediaType in Storage daemon
   }

   # Definition of DLT tape storage device
   Storage {
     Name = DLTDrive
     Address = rufus
     Password = "secret"
     Device = "HP DLT 80"      # same as Device in Storage daemon
     Media Type = DLT8000      # same as MediaType in Storage daemon
   }

Note that the backup job writes to the :config:option:`dir/pool = Default`\  pool, which corresponds to :config:option:`Dir/Storage = File`\  storage. There is no :config:option:`dir/pool/Storage`\  directive in the Job resource while the two :config:option:`Dir/Pool`\  resources contain different :config:option:`dir/pool/Storage`\  directives. Moreover, the :config:option:`dir/pool = Default`\  pool contains a
:config:option:`dir/pool/NextPool`\  directive that refers to the :config:option:`dir/pool = Tape`\  pool.

In order to migrate jobs from the :config:option:`Dir/Pool = Default`\  pool to the :config:option:`Dir/Pool = Tape`\  pool we add the following Job resource:

.. code-block:: bareosconfig
   :caption: migrate all volumes of a pool

   Job {
     Name = "migrate-volume"
     Type = Migrate
     Messages = Standard
     Pool = Default
     Selection Type = Volume
     Selection Pattern = "."
   }

The :config:option:`dir/job/SelectionType`\  and :config:option:`dir/job/SelectionPattern`\  directives instruct Bareos to select all volumes of the given pool (:config:option:`dir/pool = Default`\ ) whose volume names match the given regular expression (:strong:`"."`), i.e., all volumes. Hence those jobs which were backed up to any volume in the :config:option:`dir/pool = Default`\  pool will be migrated. Because of the
:config:option:`dir/pool/NextPool`\  directive of the :config:option:`dir/pool = Default`\  pool resource, the jobs will be migrated to tape storage.

Another way to accomplish the same is the following Job resource:

.. code-block:: bareosconfig
   :caption: migrate all jobs named *Save

   Job {
     Name = "migrate"
     Type = Migrate
     Messages = Standard
     Pool = Default
     Selection Type = Job
     Selection Pattern = ".*Save"
   }

This migrates all jobs ending with :strong:`Save` from the :config:option:`dir/pool = Default`\  pool to the :config:option:`dir/pool = Tape`\  pool, i.e., from File storage to Tape storage.

.. _section-CopyMigrationJobsMultipleStorageDaemons:

Multiple Storage Daemons
^^^^^^^^^^^^^^^^^^^^^^^^

Beginning from Bareos :sinceVersion:`13.2.0: Copy and Migration Jobs between different Storage Daemons`, Migration and Copy jobs are also possible from one Storage daemon to another Storage Daemon.

Please note:

-  the director must have two different storage resources configured (e.g. storage1 and storage2)

-  each storage needs an own device and an individual pool (e.g. pool1, pool2)

-  each pool is linked to its own storage via the storage directive in the pool resource

-  to configure the migration from pool1 to pool2, the :config:option:`dir/pool/NextPool`\  directive of pool1 has to point to pool2

-  the copy job itself has to be of type copy/migrate (exactly as already known in copy- and migration jobs)

Example:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf: Copy Job between different Storage Daemons

   #bareos-dir.conf

   # Fake fileset for copy jobs
   Fileset {
     Name = None
     Include {
       Options {
         signature = MD5
       }
     }
   }

   # Fake client for copy jobs
   Client {
     Name = None
     Address = localhost
     Password = "NoNe"
     Catalog = MyCatalog
   }

   # Source storage for migration
   Storage {
      Name = storage1
      Address = sd1.example.com
      Password = "secret1"
      Device = File1
      Media Type = File
   }

   # Target storage for migration
   Storage {
      Name = storage2
      Address = sd2.example.com
      Password = "secret2"
      Device = File2
      Media Type = File2   # Has to be different than in storage1
   }

   Pool {
      Name = pool1
      Storage = storage1
      Next Pool = pool2    # This points to the target storage
   }

   Pool {
      Name = pool2
      Storage = storage2
   }

   Job {
      Name = CopyToRemote
      Type = Copy
      Messages = Standard
      Selection Type = PoolUncopiedJobs
      Spool Data = Yes
      Pool = pool1
   }




