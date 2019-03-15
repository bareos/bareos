.. config:option:: dir/job/Accurate

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-Accurate.rst.inc



.. config:option:: dir/job/AddPrefix

   :type: STRING

   .. include:: /config-directive-description/dir-job-AddPrefix.rst.inc



.. config:option:: dir/job/AddSuffix

   :type: STRING

   .. include:: /config-directive-description/dir-job-AddSuffix.rst.inc



.. config:option:: dir/job/AllowDuplicateJobs

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-job-AllowDuplicateJobs.rst.inc



.. config:option:: dir/job/AllowHigherDuplicates

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-job-AllowHigherDuplicates.rst.inc



.. config:option:: dir/job/AllowMixedPriority

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-AllowMixedPriority.rst.inc



.. config:option:: dir/job/AlwaysIncremental

   :type: BOOLEAN
   :default: no
   :version: 16.2.4

   Enable/disable always incremental backup scheme.

   .. include:: /config-directive-description/dir-job-AlwaysIncremental.rst.inc



.. config:option:: dir/job/AlwaysIncrementalJobRetention

   :type: TIME
   :default: 0
   :version: 16.2.4

   Backup Jobs older than the specified time duration will be merged into a new Virtual backup.

   .. include:: /config-directive-description/dir-job-AlwaysIncrementalJobRetention.rst.inc



.. config:option:: dir/job/AlwaysIncrementalKeepNumber

   :type: PINT32
   :default: 0
   :version: 16.2.4

   Guarantee that at least the specified number of Backup Jobs will persist, even if they are older than "Always Incremental Job Retention".

   .. include:: /config-directive-description/dir-job-AlwaysIncrementalKeepNumber.rst.inc



.. config:option:: dir/job/AlwaysIncrementalMaxFullAge

   :type: TIME
   :version: 16.2.4

   If "AlwaysIncrementalMaxFullAge" is set, during consolidations only incremental backups will be considered while the Full Backup remains to reduce the amount of data being consolidated. Only if the Full Backup is older than "AlwaysIncrementalMaxFullAge", the Full Backup will be part of the consolidation to avoid the Full Backup becoming too old .

   .. include:: /config-directive-description/dir-job-AlwaysIncrementalMaxFullAge.rst.inc



.. config:option:: dir/job/BackupFormat

   :type: STRING
   :default: Native

   .. include:: /config-directive-description/dir-job-BackupFormat.rst.inc



.. config:option:: dir/job/Base

   :type: RESOURCE_LIST

   .. include:: /config-directive-description/dir-job-Base.rst.inc



.. config:option:: dir/job/Bootstrap

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-job-Bootstrap.rst.inc



.. config:option:: dir/job/CancelLowerLevelDuplicates

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-CancelLowerLevelDuplicates.rst.inc



.. config:option:: dir/job/CancelQueuedDuplicates

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-CancelQueuedDuplicates.rst.inc



.. config:option:: dir/job/CancelRunningDuplicates

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-CancelRunningDuplicates.rst.inc



.. config:option:: dir/job/Catalog

   :type: RES
   :version: 13.4.0

   .. include:: /config-directive-description/dir-job-Catalog.rst.inc



.. config:option:: dir/job/Client

   :type: RES

   .. include:: /config-directive-description/dir-job-Client.rst.inc



.. config:option:: dir/job/ClientRunAfterJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-job-ClientRunAfterJob.rst.inc



.. config:option:: dir/job/ClientRunBeforeJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-job-ClientRunBeforeJob.rst.inc



.. config:option:: dir/job/Description

   :type: STRING

   .. include:: /config-directive-description/dir-job-Description.rst.inc



.. config:option:: dir/job/DifferentialBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-job-DifferentialBackupPool.rst.inc



.. config:option:: dir/job/DifferentialMaxRuntime

   :type: TIME

   .. include:: /config-directive-description/dir-job-DifferentialMaxRuntime.rst.inc



.. config:option:: dir/job/DifferentialMaxWaitTime

   :type: TIME
   :version: deprecated

   .. include:: /config-directive-description/dir-job-DifferentialMaxWaitTime.rst.inc



.. config:option:: dir/job/DirPluginOptions

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-job-DirPluginOptions.rst.inc



.. config:option:: dir/job/Enabled

   :type: BOOLEAN
   :default: yes

   En- or disable this resource.

   .. include:: /config-directive-description/dir-job-Enabled.rst.inc



.. config:option:: dir/job/FdPluginOptions

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-job-FdPluginOptions.rst.inc



.. config:option:: dir/job/FileHistorySize

   :type: SIZE64
   :default: 10000000
   :version: 15.2.4

   .. include:: /config-directive-description/dir-job-FileHistorySize.rst.inc



.. config:option:: dir/job/FileSet

   :type: RES

   .. include:: /config-directive-description/dir-job-FileSet.rst.inc



.. config:option:: dir/job/FullBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-job-FullBackupPool.rst.inc



.. config:option:: dir/job/FullMaxRuntime

   :type: TIME

   .. include:: /config-directive-description/dir-job-FullMaxRuntime.rst.inc



.. config:option:: dir/job/FullMaxWaitTime

   :type: TIME
   :version: deprecated

   .. include:: /config-directive-description/dir-job-FullMaxWaitTime.rst.inc



.. config:option:: dir/job/IncrementalBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-job-IncrementalBackupPool.rst.inc



.. config:option:: dir/job/IncrementalMaxRuntime

   :type: TIME

   .. include:: /config-directive-description/dir-job-IncrementalMaxRuntime.rst.inc



.. config:option:: dir/job/IncrementalMaxWaitTime

   :type: TIME
   :version: deprecated

   .. include:: /config-directive-description/dir-job-IncrementalMaxWaitTime.rst.inc



.. config:option:: dir/job/JobDefs

   :type: RES

   .. include:: /config-directive-description/dir-job-JobDefs.rst.inc



.. config:option:: dir/job/JobToVerify

   :type: RES

   .. include:: /config-directive-description/dir-job-JobToVerify.rst.inc



.. config:option:: dir/job/Level

   :type: BACKUP_LEVEL

   .. include:: /config-directive-description/dir-job-Level.rst.inc



.. config:option:: dir/job/MaxConcurrentCopies

   :type: PINT32
   :default: 100

   .. include:: /config-directive-description/dir-job-MaxConcurrentCopies.rst.inc



.. config:option:: dir/job/MaxDiffInterval

   :type: TIME

   .. include:: /config-directive-description/dir-job-MaxDiffInterval.rst.inc



.. config:option:: dir/job/MaxFullConsolidations

   :type: PINT32
   :default: 0
   :version: 16.2.4

   If "AlwaysIncrementalMaxFullAge" is configured, do not run more than "MaxFullConsolidations" consolidation jobs that include the Full backup.

   .. include:: /config-directive-description/dir-job-MaxFullConsolidations.rst.inc



.. config:option:: dir/job/MaxFullInterval

   :type: TIME

   .. include:: /config-directive-description/dir-job-MaxFullInterval.rst.inc



.. config:option:: dir/job/MaxRunSchedTime

   :type: TIME

   .. include:: /config-directive-description/dir-job-MaxRunSchedTime.rst.inc



.. config:option:: dir/job/MaxRunTime

   :type: TIME

   .. include:: /config-directive-description/dir-job-MaxRunTime.rst.inc



.. config:option:: dir/job/MaxStartDelay

   :type: TIME

   .. include:: /config-directive-description/dir-job-MaxStartDelay.rst.inc



.. config:option:: dir/job/MaxVirtualFullInterval

   :type: TIME
   :version: 14.4.0

   .. include:: /config-directive-description/dir-job-MaxVirtualFullInterval.rst.inc



.. config:option:: dir/job/MaxWaitTime

   :type: TIME

   .. include:: /config-directive-description/dir-job-MaxWaitTime.rst.inc



.. config:option:: dir/job/MaximumBandwidth

   :type: SPEED

   .. include:: /config-directive-description/dir-job-MaximumBandwidth.rst.inc



.. config:option:: dir/job/MaximumConcurrentJobs

   :type: PINT32
   :default: 1

   .. include:: /config-directive-description/dir-job-MaximumConcurrentJobs.rst.inc



.. config:option:: dir/job/Messages

   :required: True
   :type: RES

   .. include:: /config-directive-description/dir-job-Messages.rst.inc



.. config:option:: dir/job/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-job-Name.rst.inc



.. config:option:: dir/job/NextPool

   :type: RES

   .. include:: /config-directive-description/dir-job-NextPool.rst.inc



.. config:option:: dir/job/PluginOptions

   :type: STRING_LIST
   :version: deprecated

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-job-PluginOptions.rst.inc



.. config:option:: dir/job/Pool

   :required: True
   :type: RES

   .. include:: /config-directive-description/dir-job-Pool.rst.inc



.. config:option:: dir/job/PreferMountedVolumes

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-job-PreferMountedVolumes.rst.inc



.. config:option:: dir/job/PrefixLinks

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-PrefixLinks.rst.inc



.. config:option:: dir/job/Priority

   :type: PINT32
   :default: 10

   .. include:: /config-directive-description/dir-job-Priority.rst.inc



.. config:option:: dir/job/Protocol

   :type: PROTOCOL_TYPE
   :default: Native

   .. include:: /config-directive-description/dir-job-Protocol.rst.inc



.. config:option:: dir/job/PruneFiles

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-PruneFiles.rst.inc



.. config:option:: dir/job/PruneJobs

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-PruneJobs.rst.inc



.. config:option:: dir/job/PruneVolumes

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-PruneVolumes.rst.inc



.. config:option:: dir/job/PurgeMigrationJob

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-PurgeMigrationJob.rst.inc



.. config:option:: dir/job/RegexWhere

   :type: STRING

   .. include:: /config-directive-description/dir-job-RegexWhere.rst.inc



.. config:option:: dir/job/Replace

   :type: REPLACE_OPTION
   :default: Always

   .. include:: /config-directive-description/dir-job-Replace.rst.inc



.. config:option:: dir/job/RerunFailedLevels

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-RerunFailedLevels.rst.inc



.. config:option:: dir/job/RescheduleInterval

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/dir-job-RescheduleInterval.rst.inc



.. config:option:: dir/job/RescheduleOnError

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-RescheduleOnError.rst.inc



.. config:option:: dir/job/RescheduleTimes

   :type: PINT32
   :default: 5

   .. include:: /config-directive-description/dir-job-RescheduleTimes.rst.inc



.. config:option:: dir/job/Run

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-job-Run.rst.inc



.. config:option:: dir/job/RunAfterFailedJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-job-RunAfterFailedJob.rst.inc



.. config:option:: dir/job/RunAfterJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-job-RunAfterJob.rst.inc



.. config:option:: dir/job/RunBeforeJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-job-RunBeforeJob.rst.inc



.. config:option:: dir/job/RunScript

   :type: RUNSCRIPT

   .. include:: /config-directive-description/dir-job-RunScript.rst.inc



.. config:option:: dir/job/SaveFileHistory

   :type: BOOLEAN
   :default: yes
   :version: 14.2.0

   .. include:: /config-directive-description/dir-job-SaveFileHistory.rst.inc



.. config:option:: dir/job/Schedule

   :type: RES

   .. include:: /config-directive-description/dir-job-Schedule.rst.inc



.. config:option:: dir/job/SdPluginOptions

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-job-SdPluginOptions.rst.inc



.. config:option:: dir/job/SelectionPattern

   :type: STRING

   .. include:: /config-directive-description/dir-job-SelectionPattern.rst.inc



.. config:option:: dir/job/SelectionType

   :type: MIGRATION_TYPE

   .. include:: /config-directive-description/dir-job-SelectionType.rst.inc



.. config:option:: dir/job/SpoolAttributes

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-SpoolAttributes.rst.inc



.. config:option:: dir/job/SpoolData

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-job-SpoolData.rst.inc



.. config:option:: dir/job/SpoolSize

   :type: SIZE64

   .. include:: /config-directive-description/dir-job-SpoolSize.rst.inc



.. config:option:: dir/job/Storage

   :type: RESOURCE_LIST

   .. include:: /config-directive-description/dir-job-Storage.rst.inc



.. config:option:: dir/job/StripPrefix

   :type: STRING

   .. include:: /config-directive-description/dir-job-StripPrefix.rst.inc



.. config:option:: dir/job/Type

   :required: True
   :type: JOB_TYPE

   .. include:: /config-directive-description/dir-job-Type.rst.inc



.. config:option:: dir/job/VerifyJob

   :type: RES

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-job-VerifyJob.rst.inc



.. config:option:: dir/job/VirtualFullBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-job-VirtualFullBackupPool.rst.inc



.. config:option:: dir/job/Where

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-job-Where.rst.inc



.. config:option:: dir/job/WriteBootstrap

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-job-WriteBootstrap.rst.inc



.. config:option:: dir/job/WritePartAfterJob

   :type: BOOLEAN
   :version: deprecated

   .. include:: /config-directive-description/dir-job-WritePartAfterJob.rst.inc



.. config:option:: dir/job/WriteVerifyList

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-job-WriteVerifyList.rst.inc



