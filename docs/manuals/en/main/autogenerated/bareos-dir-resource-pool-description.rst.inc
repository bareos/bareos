.. config:option:: dir/pool/ActionOnPurge

   :type: ACTION_ON_PURGE

   .. include:: /config-directive-description/dir-pool-ActionOnPurge.rst.inc



.. config:option:: dir/pool/AutoPrune

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-pool-AutoPrune.rst.inc



.. config:option:: dir/pool/Catalog

   :type: RES

   .. include:: /config-directive-description/dir-pool-Catalog.rst.inc



.. config:option:: dir/pool/CatalogFiles

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-pool-CatalogFiles.rst.inc



.. config:option:: dir/pool/CleaningPrefix

   :type: STRNAME
   :default: CLN

   .. include:: /config-directive-description/dir-pool-CleaningPrefix.rst.inc



.. config:option:: dir/pool/Description

   :type: STRING

   .. include:: /config-directive-description/dir-pool-Description.rst.inc



.. config:option:: dir/pool/FileRetention

   :type: TIME

   .. include:: /config-directive-description/dir-pool-FileRetention.rst.inc



.. config:option:: dir/pool/JobRetention

   :type: TIME

   .. include:: /config-directive-description/dir-pool-JobRetention.rst.inc



.. config:option:: dir/pool/LabelFormat

   :type: STRNAME

   .. include:: /config-directive-description/dir-pool-LabelFormat.rst.inc



.. config:option:: dir/pool/LabelType

   :type: LABEL

   .. include:: /config-directive-description/dir-pool-LabelType.rst.inc



.. config:option:: dir/pool/MaximumBlockSize

   :type: SIZE32
   :version: 14.2.0

   .. include:: /config-directive-description/dir-pool-MaximumBlockSize.rst.inc



.. config:option:: dir/pool/MaximumVolumeBytes

   :type: SIZE64

   .. include:: /config-directive-description/dir-pool-MaximumVolumeBytes.rst.inc



.. config:option:: dir/pool/MaximumVolumeFiles

   :type: PINT32

   .. include:: /config-directive-description/dir-pool-MaximumVolumeFiles.rst.inc



.. config:option:: dir/pool/MaximumVolumeJobs

   :type: PINT32

   .. include:: /config-directive-description/dir-pool-MaximumVolumeJobs.rst.inc



.. config:option:: dir/pool/MaximumVolumes

   :type: PINT32

   .. include:: /config-directive-description/dir-pool-MaximumVolumes.rst.inc



.. config:option:: dir/pool/MigrationHighBytes

   :type: SIZE64

   .. include:: /config-directive-description/dir-pool-MigrationHighBytes.rst.inc



.. config:option:: dir/pool/MigrationLowBytes

   :type: SIZE64

   .. include:: /config-directive-description/dir-pool-MigrationLowBytes.rst.inc



.. config:option:: dir/pool/MigrationTime

   :type: TIME

   .. include:: /config-directive-description/dir-pool-MigrationTime.rst.inc



.. config:option:: dir/pool/MinimumBlockSize

   :type: SIZE32

   .. include:: /config-directive-description/dir-pool-MinimumBlockSize.rst.inc



.. config:option:: dir/pool/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-pool-Name.rst.inc



.. config:option:: dir/pool/NextPool

   :type: RES

   .. include:: /config-directive-description/dir-pool-NextPool.rst.inc



.. config:option:: dir/pool/PoolType

   :type: POOLTYPE
   :default: Backup

   .. include:: /config-directive-description/dir-pool-PoolType.rst.inc



.. config:option:: dir/pool/PurgeOldestVolume

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-pool-PurgeOldestVolume.rst.inc



.. config:option:: dir/pool/Recycle

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-pool-Recycle.rst.inc



.. config:option:: dir/pool/RecycleCurrentVolume

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-pool-RecycleCurrentVolume.rst.inc



.. config:option:: dir/pool/RecycleOldestVolume

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-pool-RecycleOldestVolume.rst.inc



.. config:option:: dir/pool/RecyclePool

   :type: RES

   .. include:: /config-directive-description/dir-pool-RecyclePool.rst.inc



.. config:option:: dir/pool/ScratchPool

   :type: RES

   .. include:: /config-directive-description/dir-pool-ScratchPool.rst.inc



.. config:option:: dir/pool/Storage

   :type: RESOURCE_LIST

   .. include:: /config-directive-description/dir-pool-Storage.rst.inc



.. config:option:: dir/pool/UseCatalog

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-pool-UseCatalog.rst.inc



.. config:option:: dir/pool/UseVolumeOnce

   :type: BOOLEAN
   :version: deprecated

   .. include:: /config-directive-description/dir-pool-UseVolumeOnce.rst.inc



.. config:option:: dir/pool/VolumeRetention

   :type: TIME
   :default: 31536000

   .. include:: /config-directive-description/dir-pool-VolumeRetention.rst.inc



.. config:option:: dir/pool/VolumeUseDuration

   :type: TIME

   .. include:: /config-directive-description/dir-pool-VolumeUseDuration.rst.inc



