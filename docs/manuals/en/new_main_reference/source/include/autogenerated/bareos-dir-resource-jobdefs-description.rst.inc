.. config:option:: dir/jobdefs/Accurate

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-Accurate.rst.inc



.. config:option:: dir/jobdefs/AddPrefix

   :type: STRING

   .. include:: /config-directive-description/dir-jobdefs-AddPrefix.rst.inc



.. config:option:: dir/jobdefs/AddSuffix

   :type: STRING

   .. include:: /config-directive-description/dir-jobdefs-AddSuffix.rst.inc



.. config:option:: dir/jobdefs/AllowDuplicateJobs

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-jobdefs-AllowDuplicateJobs.rst.inc



.. config:option:: dir/jobdefs/AllowHigherDuplicates

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-jobdefs-AllowHigherDuplicates.rst.inc



.. config:option:: dir/jobdefs/AllowMixedPriority

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-AllowMixedPriority.rst.inc



.. config:option:: dir/jobdefs/AlwaysIncremental

   :type: BOOLEAN
   :default: no
   :version: 16.2.4

   Enable/disable always incremental backup scheme.

   .. include:: /config-directive-description/dir-jobdefs-AlwaysIncremental.rst.inc



.. config:option:: dir/jobdefs/AlwaysIncrementalJobRetention

   :type: TIME
   :default: 0
   :version: 16.2.4

   Backup Jobs older than the specified time duration will be merged into a new Virtual backup.

   .. include:: /config-directive-description/dir-jobdefs-AlwaysIncrementalJobRetention.rst.inc



.. config:option:: dir/jobdefs/AlwaysIncrementalKeepNumber

   :type: PINT32
   :default: 0
   :version: 16.2.4

   Guarantee that at least the specified number of Backup Jobs will persist, even if they are older than "Always Incremental Job Retention".

   .. include:: /config-directive-description/dir-jobdefs-AlwaysIncrementalKeepNumber.rst.inc



.. config:option:: dir/jobdefs/AlwaysIncrementalMaxFullAge

   :type: TIME
   :version: 16.2.4

   If "AlwaysIncrementalMaxFullAge" is set, during consolidations only incremental backups will be considered while the Full Backup remains to reduce the amount of data being consolidated. Only if the Full Backup is older than "AlwaysIncrementalMaxFullAge", the Full Backup will be part of the consolidation to avoid the Full Backup becoming too old .

   .. include:: /config-directive-description/dir-jobdefs-AlwaysIncrementalMaxFullAge.rst.inc



.. config:option:: dir/jobdefs/BackupFormat

   :type: STRING
   :default: Native

   .. include:: /config-directive-description/dir-jobdefs-BackupFormat.rst.inc



.. config:option:: dir/jobdefs/Base

   :type: RESOURCE_LIST

   .. include:: /config-directive-description/dir-jobdefs-Base.rst.inc



.. config:option:: dir/jobdefs/Bootstrap

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-jobdefs-Bootstrap.rst.inc



.. config:option:: dir/jobdefs/CancelLowerLevelDuplicates

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-CancelLowerLevelDuplicates.rst.inc



.. config:option:: dir/jobdefs/CancelQueuedDuplicates

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-CancelQueuedDuplicates.rst.inc



.. config:option:: dir/jobdefs/CancelRunningDuplicates

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-CancelRunningDuplicates.rst.inc



.. config:option:: dir/jobdefs/Catalog

   :type: RES
   :version: 13.4.0

   .. include:: /config-directive-description/dir-jobdefs-Catalog.rst.inc



.. config:option:: dir/jobdefs/Client

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-Client.rst.inc



.. config:option:: dir/jobdefs/ClientRunAfterJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-jobdefs-ClientRunAfterJob.rst.inc



.. config:option:: dir/jobdefs/ClientRunBeforeJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-jobdefs-ClientRunBeforeJob.rst.inc



.. config:option:: dir/jobdefs/Description

   :type: STRING

   .. include:: /config-directive-description/dir-jobdefs-Description.rst.inc



.. config:option:: dir/jobdefs/DifferentialBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-DifferentialBackupPool.rst.inc



.. config:option:: dir/jobdefs/DifferentialMaxRuntime

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-DifferentialMaxRuntime.rst.inc



.. config:option:: dir/jobdefs/DifferentialMaxWaitTime

   :type: TIME
   :version: deprecated

   .. include:: /config-directive-description/dir-jobdefs-DifferentialMaxWaitTime.rst.inc



.. config:option:: dir/jobdefs/DirPluginOptions

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-jobdefs-DirPluginOptions.rst.inc



.. config:option:: dir/jobdefs/Enabled

   :type: BOOLEAN
   :default: yes

   En- or disable this resource.

   .. include:: /config-directive-description/dir-jobdefs-Enabled.rst.inc



.. config:option:: dir/jobdefs/FdPluginOptions

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-jobdefs-FdPluginOptions.rst.inc



.. config:option:: dir/jobdefs/FileHistorySize

   :type: SIZE64
   :default: 10000000
   :version: 15.2.4

   .. include:: /config-directive-description/dir-jobdefs-FileHistorySize.rst.inc



.. config:option:: dir/jobdefs/FileSet

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-FileSet.rst.inc



.. config:option:: dir/jobdefs/FullBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-FullBackupPool.rst.inc



.. config:option:: dir/jobdefs/FullMaxRuntime

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-FullMaxRuntime.rst.inc



.. config:option:: dir/jobdefs/FullMaxWaitTime

   :type: TIME
   :version: deprecated

   .. include:: /config-directive-description/dir-jobdefs-FullMaxWaitTime.rst.inc



.. config:option:: dir/jobdefs/IncrementalBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-IncrementalBackupPool.rst.inc



.. config:option:: dir/jobdefs/IncrementalMaxRuntime

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-IncrementalMaxRuntime.rst.inc



.. config:option:: dir/jobdefs/IncrementalMaxWaitTime

   :type: TIME
   :version: deprecated

   .. include:: /config-directive-description/dir-jobdefs-IncrementalMaxWaitTime.rst.inc



.. config:option:: dir/jobdefs/JobDefs

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-JobDefs.rst.inc



.. config:option:: dir/jobdefs/JobToVerify

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-JobToVerify.rst.inc



.. config:option:: dir/jobdefs/Level

   :type: BACKUP_LEVEL

   .. include:: /config-directive-description/dir-jobdefs-Level.rst.inc



.. config:option:: dir/jobdefs/MaxConcurrentCopies

   :type: PINT32
   :default: 100

   .. include:: /config-directive-description/dir-jobdefs-MaxConcurrentCopies.rst.inc



.. config:option:: dir/jobdefs/MaxDiffInterval

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-MaxDiffInterval.rst.inc



.. config:option:: dir/jobdefs/MaxFullConsolidations

   :type: PINT32
   :default: 0
   :version: 16.2.4

   If "AlwaysIncrementalMaxFullAge" is configured, do not run more than "MaxFullConsolidations" consolidation jobs that include the Full backup.

   .. include:: /config-directive-description/dir-jobdefs-MaxFullConsolidations.rst.inc



.. config:option:: dir/jobdefs/MaxFullInterval

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-MaxFullInterval.rst.inc



.. config:option:: dir/jobdefs/MaxRunSchedTime

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-MaxRunSchedTime.rst.inc



.. config:option:: dir/jobdefs/MaxRunTime

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-MaxRunTime.rst.inc



.. config:option:: dir/jobdefs/MaxStartDelay

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-MaxStartDelay.rst.inc



.. config:option:: dir/jobdefs/MaxVirtualFullInterval

   :type: TIME
   :version: 14.4.0

   .. include:: /config-directive-description/dir-jobdefs-MaxVirtualFullInterval.rst.inc



.. config:option:: dir/jobdefs/MaxWaitTime

   :type: TIME

   .. include:: /config-directive-description/dir-jobdefs-MaxWaitTime.rst.inc



.. config:option:: dir/jobdefs/MaximumBandwidth

   :type: SPEED

   .. include:: /config-directive-description/dir-jobdefs-MaximumBandwidth.rst.inc



.. config:option:: dir/jobdefs/MaximumConcurrentJobs

   :type: PINT32
   :default: 1

   .. include:: /config-directive-description/dir-jobdefs-MaximumConcurrentJobs.rst.inc



.. config:option:: dir/jobdefs/Messages

   :required: True
   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-Messages.rst.inc



.. config:option:: dir/jobdefs/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-jobdefs-Name.rst.inc



.. config:option:: dir/jobdefs/NextPool

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-NextPool.rst.inc



.. config:option:: dir/jobdefs/PluginOptions

   :type: STRING_LIST
   :version: deprecated

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-jobdefs-PluginOptions.rst.inc



.. config:option:: dir/jobdefs/Pool

   :required: True
   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-Pool.rst.inc



.. config:option:: dir/jobdefs/PreferMountedVolumes

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-jobdefs-PreferMountedVolumes.rst.inc



.. config:option:: dir/jobdefs/PrefixLinks

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-PrefixLinks.rst.inc



.. config:option:: dir/jobdefs/Priority

   :type: PINT32
   :default: 10

   .. include:: /config-directive-description/dir-jobdefs-Priority.rst.inc



.. config:option:: dir/jobdefs/Protocol

   :type: PROTOCOL_TYPE
   :default: Native

   .. include:: /config-directive-description/dir-jobdefs-Protocol.rst.inc



.. config:option:: dir/jobdefs/PruneFiles

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-PruneFiles.rst.inc



.. config:option:: dir/jobdefs/PruneJobs

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-PruneJobs.rst.inc



.. config:option:: dir/jobdefs/PruneVolumes

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-PruneVolumes.rst.inc



.. config:option:: dir/jobdefs/PurgeMigrationJob

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-PurgeMigrationJob.rst.inc



.. config:option:: dir/jobdefs/RegexWhere

   :type: STRING

   .. include:: /config-directive-description/dir-jobdefs-RegexWhere.rst.inc



.. config:option:: dir/jobdefs/Replace

   :type: REPLACE_OPTION
   :default: Always

   .. include:: /config-directive-description/dir-jobdefs-Replace.rst.inc



.. config:option:: dir/jobdefs/RerunFailedLevels

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-RerunFailedLevels.rst.inc



.. config:option:: dir/jobdefs/RescheduleInterval

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/dir-jobdefs-RescheduleInterval.rst.inc



.. config:option:: dir/jobdefs/RescheduleOnError

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-RescheduleOnError.rst.inc



.. config:option:: dir/jobdefs/RescheduleTimes

   :type: PINT32
   :default: 5

   .. include:: /config-directive-description/dir-jobdefs-RescheduleTimes.rst.inc



.. config:option:: dir/jobdefs/Run

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-jobdefs-Run.rst.inc



.. config:option:: dir/jobdefs/RunAfterFailedJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-jobdefs-RunAfterFailedJob.rst.inc



.. config:option:: dir/jobdefs/RunAfterJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-jobdefs-RunAfterJob.rst.inc



.. config:option:: dir/jobdefs/RunBeforeJob

   :type: RUNSCRIPT_SHORT

   .. include:: /config-directive-description/dir-jobdefs-RunBeforeJob.rst.inc



.. config:option:: dir/jobdefs/RunScript

   :type: RUNSCRIPT

   .. include:: /config-directive-description/dir-jobdefs-RunScript.rst.inc



.. config:option:: dir/jobdefs/SaveFileHistory

   :type: BOOLEAN
   :default: yes
   :version: 14.2.0

   .. include:: /config-directive-description/dir-jobdefs-SaveFileHistory.rst.inc



.. config:option:: dir/jobdefs/Schedule

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-Schedule.rst.inc



.. config:option:: dir/jobdefs/SdPluginOptions

   :type: STRING_LIST

   .. include:: /config-directive-description/dir-jobdefs-SdPluginOptions.rst.inc



.. config:option:: dir/jobdefs/SelectionPattern

   :type: STRING

   .. include:: /config-directive-description/dir-jobdefs-SelectionPattern.rst.inc



.. config:option:: dir/jobdefs/SelectionType

   :type: MIGRATION_TYPE

   .. include:: /config-directive-description/dir-jobdefs-SelectionType.rst.inc



.. config:option:: dir/jobdefs/SpoolAttributes

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-SpoolAttributes.rst.inc



.. config:option:: dir/jobdefs/SpoolData

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-jobdefs-SpoolData.rst.inc



.. config:option:: dir/jobdefs/SpoolSize

   :type: SIZE64

   .. include:: /config-directive-description/dir-jobdefs-SpoolSize.rst.inc



.. config:option:: dir/jobdefs/Storage

   :type: RESOURCE_LIST

   .. include:: /config-directive-description/dir-jobdefs-Storage.rst.inc



.. config:option:: dir/jobdefs/StripPrefix

   :type: STRING

   .. include:: /config-directive-description/dir-jobdefs-StripPrefix.rst.inc



.. config:option:: dir/jobdefs/Type

   :required: True
   :type: JOB_TYPE

   .. include:: /config-directive-description/dir-jobdefs-Type.rst.inc



.. config:option:: dir/jobdefs/VerifyJob

   :type: RES

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-jobdefs-VerifyJob.rst.inc



.. config:option:: dir/jobdefs/VirtualFullBackupPool

   :type: RES

   .. include:: /config-directive-description/dir-jobdefs-VirtualFullBackupPool.rst.inc



.. config:option:: dir/jobdefs/Where

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-jobdefs-Where.rst.inc



.. config:option:: dir/jobdefs/WriteBootstrap

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-jobdefs-WriteBootstrap.rst.inc



.. config:option:: dir/jobdefs/WritePartAfterJob

   :type: BOOLEAN
   :version: deprecated

   .. include:: /config-directive-description/dir-jobdefs-WritePartAfterJob.rst.inc



.. config:option:: dir/jobdefs/WriteVerifyList

   :type: DIRECTORY

   .. include:: /config-directive-description/dir-jobdefs-WriteVerifyList.rst.inc



