Selection Type is only used for Copy and Migration jobs, see :ref:`MigrationChapter`. It determines how a migration job will go about selecting what JobIds to migrate. In most cases, it is used in conjunction with a :config:option:`dir/job/SelectionPattern`\  to give you fine control over exactly what JobIds are selected. The possible values are:

SmallestVolume
   This selection keyword selects the volume with the fewest bytes from the Pool to be migrated. The Pool to be migrated is the Pool defined in the Migration Job resource. The migration control job will then start and run one migration backup job for each of the Jobs found on this Volume. The Selection Pattern, if specified, is not used.

OldestVolume
   This selection keyword selects the volume with the oldest last write time in the Pool to be migrated. The Pool to be migrated is the Pool defined in the Migration Job resource. The migration control job will then start and run one migration backup job for each of the Jobs found on this Volume. The Selection Pattern, if specified, is not used.

Client
   The Client selection type, first selects all the Clients that have been backed up in the Pool specified by the Migration Job resource, then it applies the :config:option:`dir/job/SelectionPattern`\  as a regular expression to the list of Client names, giving a filtered Client name list. All jobs that were backed up for those filtered (regexed) Clients will be migrated. The migration control job will then start and run one migration backup job for each of the JobIds found
   for those filtered Clients.

Volume
   The Volume selection type, first selects all the Volumes that have been backed up in the Pool specified by the Migration Job resource, then it applies the :config:option:`dir/job/SelectionPattern`\  as a regular expression to the list of Volume names, giving a filtered Volume list. All JobIds that were backed up for those filtered (regexed) Volumes will be migrated. The migration control job will then start and run one migration backup job for each of the JobIds found on
   those filtered Volumes.

Job
   The Job selection type, first selects all the Jobs (as defined on the :config:option:`dir/job/Name`\  directive in a Job resource) that have been backed up in the Pool specified by the Migration Job resource, then it applies the :config:option:`dir/job/SelectionPattern`\  as a regular expression to the list of Job names, giving a filtered Job name list. All JobIds that were run for those filtered (regexed) Job names will be migrated. Note, for a given
   Job named, they can be many jobs (JobIds) that ran. The migration control job will then start and run one migration backup job for each of the Jobs found.

SQLQuery
   The SQLQuery selection type, used the :config:option:`dir/job/SelectionPattern`\  as an SQL query to obtain the JobIds to be migrated. The Selection Pattern must be a valid SELECT SQL statement for your SQL engine, and it must return the JobId as the first field of the SELECT.

PoolOccupancy
   This selection type will cause the Migration job to compute the total size of the specified pool for all Media Types combined. If it exceeds the :config:option:`dir/pool/MigrationHighBytes`\  defined in the Pool, the Migration job will migrate all JobIds beginning with the oldest Volume in the pool (determined by Last Write time) until the Pool bytes drop below the :config:option:`dir/pool/MigrationLowBytes`\  defined in the Pool. This calculation
   should be consider rather approximative because it is made once by the Migration job before migration is begun, and thus does not take into account additional data written into the Pool during the migration. In addition, the calculation of the total Pool byte size is based on the Volume bytes saved in the Volume (Media) database entries. The bytes calculate for Migration is based on the value stored in the Job records of the Jobs to be migrated. These do not include the Storage daemon
   overhead as is in the total Pool size. As a consequence, normally, the migration will migrate more bytes than strictly necessary.

PoolTime
   The PoolTime selection type will cause the Migration job to look at the time each JobId has been in the Pool since the job ended. All Jobs in the Pool longer than the time specified on :config:option:`dir/pool/MigrationTime`\  directive in the Pool resource will be migrated.

PoolUncopiedJobs
   This selection which copies all jobs from a pool to an other pool which were not copied before is available only for copy Jobs.

