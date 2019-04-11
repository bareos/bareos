.. _section-alwaysincremental:

Always Incremental Backup Scheme
================================

:index:`\ <single: Always Incremental>`

Always Incremental Backups are available since Bareos :sinceVersion:`16.2.4: Always Incremental`.

Conventional Backup Scheme Drawbacks
------------------------------------

:index:`\ <single: Retention>`

To better understand the advantages of the Always Incremental Backup scheme, we first analyze the way that the conventional Incremental - Differential - Full Backup Scheme works.

The following figure shows the jobs available for restore over time. Red are full backups, green are differential backups and blue are incremental Backups. When you look for a data at the horizontal axis, you see what backup jobs are available for a restore at this given time.

.. image:: /include/images/inc-diff-full-jobs_available.*



The next figure shows the amount of data being backed up over the network from that client over time:

.. image:: /include/images/inc-diff-full-jobdata.*



Depending on the retention periods, old jobs are removed to save space for newer backups:

.. image:: /include/images/inc-diff-full-jobs_available-zoom.*



The problem with this way of removing jobs is the fact that jobs are removed from the system which existing jobs depend on.

Always Incremental Concept
--------------------------

The Always Incremental Backup Scheme does only incremental backups of clients, which reduces the amount of data transferred over the network to a minimum.

.. limitation:: Always Incremental Backup: Only suitable for file based backups.

   Always Incremental backups are only suitable for file based backups. Other data can not be combined on the server side (e.g. vmware plugings, NDMP, ...)
   



The Always Incremental Backup Scheme works as follows:

Client Backups are always run as incremental backups. This would usually lead to an unlimited chain of incremental backups that are depend on each other.

To avoid this problem, existing incremental backups older than a configurable age are consolidated into a new backup.

These two steps are then executed every day:

-  Incremental Backup from Client

-  Consolidation of the jobs older than maximum configure age

Deleted files will be in the backup forever, if they are not detected as deleted using :config:option:`dir/job/Accurate`\  backup.

The Always Incremental Backup Scheme does not provide the option to have other longer retention periods for the backups.

For Longterm Storage of data longer than the Always Incremental Job Retention, there are two options:

-  A copy job can be configured that copies existing full backups into a longterm pool.

-  A virtual Full Job can be configured that creates a virtual full backup into a longterm pool consolidating all existing backups into a new one.

The implementation with copy jobs is easy to implement and automatically copies all jobs that need to be copied in a single configured resource. The disadvantage of the copy job approach is the fact that at a certain point in time, the data that is copied for long term archive is already "always incremental job retention" old, so that the data in the longterm storage is not the current data that is available from the client.

The solution using virtual full jobs to create longterm storage has the disadvantage, that for every backup job the a new longterm job needs to be created.

The big advantage is that the current data will be transferred into the longterm storage.

The way that bareos determines on what base the next incremental job will be done, would choose the longterm storage job to be taken as basis for the next incremental backup which is not what is intended. Therefore, the jobtype of the longterm job is updated to "archive", so that it is not taken as base for then next incrementals and the always incremental job will stand alone.

How to configure in Bareos
--------------------------

Always Incremental Backup Job
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To configure a job to use Always Incremental Backup Scheme, following configuration is required:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/example.conf

   Job {
       ...
       Accurate = yes
       Always Incremental = yes
       Always Incremental Job Retention = <timespec>
       Always Incremental Keep Number = <number>
       ...
   }

:config:option:`dir/job/Accurate = yes`\ 
   is required to detect deleted files and prevent that they are kept in the consolidated backup jobs.

:config:option:`dir/job/AlwaysIncremental = yes`\ 
   enables the Always Incremental feature.

:config:option:`dir/job/AlwaysIncrementalJobRetention`\ 
   set the age where incrementals of this job will be kept, older jobs will be consolidated.

:config:option:`dir/job/AlwaysIncrementalKeepNumber`\ 
   sets the number of incrementals that will be kept without regarding the age. This should make sure that a certain history of a job will be kept even if the job is not executed for some time.

:config:option:`dir/job/AlwaysIncrementalMaxFullAge`\ 
   is described later, see :ref:`section-AlwaysIncrementalMaxFullAge`.

Consolidate Job
~~~~~~~~~~~~~~~

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/Consolidate.conf

   Job {
       Name = "Consolidate"
       Type = "Consolidate"
       Accurate = "yes"
       JobDefs = "DefaultJob"
   }

:config:option:`dir/job/Type = Consolidate`\ 
   configures a job to be a consolidate job. This type have been introduced with the Always Incremental feature. When used, it automatically trigger the consolidation of incremental jobs that need to be consolidated.

:config:option:`dir/job/Accurate = yes`\ 
   let the generated virtual backup job keep the accurate information.

:config:option:`dir/job/MaxFullConsolidations`\ 
   is described later, see :ref:`section-MaxFullConsolidations`.

The :config:option:`dir/job = Consolidate`\  job evaluates all jobs configured with :config:option:`dir/job/AlwaysIncremental = yes`\ . When a job is selected for consolidation, all job runs are taken into account, independent of the pool and storage where they are located.

The always incremental jobs need to be executed during the backup window (usually at night), while the consolidation jobs should be scheduled during the daytime when no backups are executed.



   .. warning::

      All Bareos job resources have some required directives, e.g. :config:option:`dir/job/Client`\ .
   Even so, none other than the mentioned directives are evaluated by a :config:option:`dir/job/Type = Consolidate`\ ,
   they still have to be defined.
   Normally all required directives are already set in :config:option:`dir/job/JobDefs = DefaultJob`\ .
   If not, you have to add them. You can use arbitrary, but valid values.

Storages and Pools
~~~~~~~~~~~~~~~~~~

For the Always Incremental Backup Scheme at least two storages are needed. See :ref:`section-MultipleStorageDevices` how to setup multiple storages.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/pool/AI-Incremental.conf

   Pool {
     Name = AI-Incremental
     Pool Type = Backup
     Recycle = yes                       # Bareos can automatically recycle Volumes
     Auto Prune = yes                    # Prune expired volumes
     Volume Retention = 360 days         # How long should jobs be kept?
     Maximum Volume Bytes = 50G          # Limit Volume size to something reasonable
     Label Format = "AI-Incremental-"
     Volume Use Duration = 23h
     Storage = File1
     Next Pool = AI-Consolidated         # consolidated jobs go to this pool
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/pool/AI-Consolidated.conf

   Pool {
     Name = AI-Consolidated
     Pool Type = Backup
     Recycle = yes                       # Bareos can automatically recycle Volumes
     Auto Prune = yes                    # Prune expired volumes
     Volume Retention = 360 days         # How long should jobs be kept?
     Maximum Volume Bytes = 50G          # Limit Volume size to something reasonable
     Label Format = "AI-Consolidated-"
     Volume Use Duration = 23h
     Storage = File2
     Next Pool = AI-Longterm             # copy jobs write to this pool
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/pool/AI-Longterm.conf

   Pool {
     Name = AI-Longterm
     Pool Type = Backup
     Recycle = yes                       # Bareos can automatically recycle Volumes
     Auto Prune = yes                    # Prune expired volumes
     Volume Retention = 10 years         # How long should jobs be kept?
     Maximum Volume Bytes = 50G          # Limit Volume size to something reasonable
     Label Format = "AI-Longterm-"
     Volume Use Duration = 23h
     Storage = File1
   }

:config:option:`dir/pool = AI-Longterm`\  is optional and will be explained in :ref:`section-AlwaysIncrementalLongTermStorage`.

How it works
------------

The following configuration extract shows how a client backup is configured for always incremental Backup. The Backup itself is scheduled every night to run as incremental backup, while the consolidation is scheduled to run every day.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/BackupClient1.conf

   Job {
       Name = "BackupClient1"
       JobDefs = "DefaultJob"

       # Always incremental settings
       AlwaysIncremental = yes
       AlwaysIncrementalJobRetention = 7 days

       Accurate = yes

       Pool = AI-Incremental
       Full Backup Pool = AI-Consolidated
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/Consolidate.conf

   Job {
       Name = "Consolidate"
       Type = "Consolidate"
       Accurate = "yes"
       JobDefs = "DefaultJob"
   }

The following image shows the available backups for each day:

.. image:: /include/images/always-incremental.*



-  The backup cycle starts with a full backup of the client.

-  Every day a incremental backup is done and is additionally available.

-  When the age of the oldest incremental reaches :config:option:`dir/job/AlwaysIncrementalJobRetention`\ , the consolidation job consolidates the oldest incremental with the full backup before to a new full backup.

This can go on more or less forever and there will be always an incremental history of :config:option:`dir/job/AlwaysIncrementalJobRetention`\ .

The following plot shows what happens if a job is not run for a certain amount of time.

.. image:: /include/images/always-incremental-with-pause-7days-retention-no-keep.*



As can be seen, the nightly consolidation jobs still go on consolidating until the last incremental is too old and then only one full backup is left. This is usually not what is intended.

For this reason, the directive :config:option:`dir/job/AlwaysIncrementalKeepNumber`\  is available which sets the minimum number of incrementals that should be kept even if they are older than :config:option:`dir/job/AlwaysIncrementalJobRetention`\ .

Setting :config:option:`dir/job/AlwaysIncrementalKeepNumber`\  to 7 in our case leads to the following result:

.. image:: /include/images/always-incremental-with-pause-7days-retention-7days-keep.*



:config:option:`dir/job/AlwaysIncrementalKeepNumber`\  incrementals are always kept, and when the backup starts again the consolidation of old incrementals starts again.

Enhancements for the Always Incremental Backup Scheme
-----------------------------------------------------

Besides the available backups at each point in time which we have considered until now, the amount of data being moved during the backups is another very important aspect.

We will have a look at this aspect in the following pictures:

The basic always incremental scheme
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The basic always incremental scheme does an incremental backup from the client daily which is relatively small and as such is very good.

During the consolidation, each day the full backup is consolidated with the oldest incremental backup, which means that more or less the full amount of data being stored on the client is moved. Although this consolidation only is performed locally on the storage daemon without client interaction, it is still an enormous amount of data being touched and can take an considerable amount of time.

If all clients use the "always incremental" backup scheme, this means that the complete data being stored in the backup system needs to be moved every day!

This is usually only feasible in relatively small environments.

The following figure shows the Data Volume being moved during the normal always incremental scheme.

-  The red bar shows the amount of the first full backup being copied from the client.

-  The blue bars show the amount of the daily incremental backups. They are so little that the can be barely seen.

-  The green bars show the amount of data being moved every day during the consolidation jobs.

.. image:: /include/images/always-incremental-jobdata.*



.. _section-AlwaysIncrementalMaxFullAge:

Always Incremental Max Full Age
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To be able to cope with this problem, the directive :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  was added. When :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  is configured, in daily operation the Full Backup is left untouched while the incrementals are consolidated as usual. Only if the Full Backup is older than :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\ , the full backup will also be part of
the consolidation.

Depending on the setting of the :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\ , the amount of daily data being moved can be reduced without losing the advantages of the always incremental Backup Scheme.

:config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  must be larger than :config:option:`dir/job/AlwaysIncrementalJobRetention`\ .

The resulting interval between full consolidations when running daily backups and daily consolidations is :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  - :config:option:`dir/job/AlwaysIncrementalJobRetention`\ .



.. figure:: /include/images/always-incremental-jobdata-AlwaysIncrementalMaxFullAge_21_days.*
   :alt: Data Volume being moved with "Always Incremental Max Full Age"

   Data Volume being moved with "Always Incremental Max Full Age"



.. figure:: /include/images/always-incremental-jobs_available-AlwaysIncrementalMaxFullAge_21_days.*
   :alt: Jobs Available with "Always Incremental Max Full Age"

   Jobs Available with "Always Incremental Max Full Age"

.. _section-MaxFullConsolidations:

Max Full Consolidations
~~~~~~~~~~~~~~~~~~~~~~~

When the :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  of many clients is set to the same value, it is probable that all full backups will reach the :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  at once and so consolidation jobs including the full backup will be started for all clients at once. This would again mean that the whole data being stored from all clients will be moved in one day.

The following figure shows the amount of data being copied by the virtual jobs that do the consolidation when having 3 identically configured backup jobs:

.. image:: /include/images/jobdata_multiple_clients.*



As can be seen, virtual jobs including the full are triggered for all three clients at the same time.

This is of course not desirable so the directive :config:option:`dir/job/MaxFullConsolidations`\  was introduced.

:config:option:`dir/job/MaxFullConsolidations`\  needs to be configured in the :config:option:`dir/job/Type = Consolidate`\  job:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/Consolidate.conf

   Job {
       Name = "Consolidate"
       Type = "Consolidate"
       Accurate = "yes"
       JobDefs = "DefaultJob"

       Max Full Consolidations = 1
   }

If :config:option:`dir/job/MaxFullConsolidations`\  is configured, the consolidation job will not start more than the specified Consolidations that include the Full Backup.

This leads to a better load balancing of full backup consolidations over different days. The value should configured so that the consolidation jobs are completed before the next normal backup run starts.

The number of always incremental jobs, the interval that the jobs are triggered and the setting of :config:option:`dir/job/AlwaysIncrementalMaxFullAge`\  influence the value that makes sense for :config:option:`dir/job/MaxFullConsolidations`\ .



.. figure:: /include/images/jobdata_multiple_clients_maxfullconsilidate.*
   :alt: Data Volume being moved with Max Full Consolidations = 1

   Data Volume being moved with Max Full Consolidations = 1



.. figure:: /include/images/jobs_available_multiple_clients_maxfullconsolidate.*
   :alt: Jobs Available with Max Full Consolidations = 1

   Jobs Available with Max Full Consolidations = 1

.. _section-AlwaysIncrementalLongTermStorage:

Long Term Storage of Always Incremental Jobs
--------------------------------------------

What is missing in the always incremental backup scheme in comparison to the traditional "Incremental Differential Full" scheme is the option to store a certain job for a longer time.

When using always incremental, the usual maximum age of data stored during the backup cycle is :config:option:`dir/job/AlwaysIncrementalJobRetention`\ .

Usually, it is desired to be able to store a certain backup for a longer time, e.g. monthly a backup should be kept for half a year.

There are two options to achieve this goal.

Copy Jobs
~~~~~~~~~

The configuration of archiving via copy job is simple, just configure a copy job that copies over the latest full backup at that point in time.

As all full backups go into the :config:option:`dir/pool = AI-Consolidated`\ , we just copy all uncopied backups in the :config:option:`dir/pool = AI-Consolidated`\  to a longterm pool:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/CopyLongtermFull.conf

   Job {
     Name = "CopyLongtermFull"
     Schedule = LongtermFull
     Type = Copy
     Level = Full
     Pool = AI-Consolidated
     Selection Type = PoolUncopiedJobs
     Messages = Standard
   }

As can be seen in the plot, the copy job creates a copy of the current full backup that is available and is already 7 days old.

.. image:: /include/images/always-incremental-copy-job-archiving.*



The other disadvantage is, that it copies all jobs, not only the virtual full jobs. It also includes the virtual incremental jobs from this pool.

Virtual Full Jobs
~~~~~~~~~~~~~~~~~

The alternative to Copy Jobs is creating a virtual Full Backup Job when the data should be stored in a long-term pool.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/VirtualLongtermFull.conf

   Job {
     Name = "VirtualLongtermFull"
     Client = bareos-fd
     FileSet = SelfTest
     Schedule = LongtermFull
     Type = Backup
     Level = VirtualFull
     Pool = AI-Consolidated
     Messages = Standard

     Priority = 13                 # run after  Consolidate
     Run Script {
           console = "update jobid=%i jobtype=A"
           Runs When = After
           Runs On Client = No
           Runs On Failure = No
     }
   }

To make sure the longterm :config:option:`dir/job/Level = VirtualFull`\  is not taken as base for the next incrementals, the job type of the copied job is set to :config:option:`dir/job/Type = Archive`\  with the :config:option:`dir/job/RunScript`\ .

As can be seen on the plot, the :config:option:`dir/job/Level = VirtualFull`\  archives the current data, i.e. it consolidates the full and all incrementals that are currently available.

.. image:: /include/images/always-incremental-virtualfull-job-archiving.*














