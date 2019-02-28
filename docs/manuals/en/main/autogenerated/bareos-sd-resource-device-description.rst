Device
------

.. config:option:: sd/device/AlertCommand

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-AlertCommand.rst.inc



.. config:option:: sd/device/AlwaysOpen

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-AlwaysOpen.rst.inc



.. config:option:: sd/device/ArchiveDevice

   :required: True
   :type: STRNAME

   .. include:: /config-directive-description/sd-device-ArchiveDevice.rst.inc



.. config:option:: sd/device/AutoDeflate

   :type: IO_DIRECTION
   :version: 13.4.0

   .. include:: /config-directive-description/sd-device-AutoDeflate.rst.inc



.. config:option:: sd/device/AutoDeflateAlgorithm

   :type: COMPRESSION_ALGORITHM
   :version: 13.4.0

   .. include:: /config-directive-description/sd-device-AutoDeflateAlgorithm.rst.inc



.. config:option:: sd/device/AutoDeflateLevel

   :type: PINT16
   :default: 6
   :version: 13.4.0

   .. include:: /config-directive-description/sd-device-AutoDeflateLevel.rst.inc



.. config:option:: sd/device/AutoInflate

   :type: IO_DIRECTION
   :version: 13.4.0

   .. include:: /config-directive-description/sd-device-AutoInflate.rst.inc



.. config:option:: sd/device/AutoSelect

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/sd-device-AutoSelect.rst.inc



.. config:option:: sd/device/Autochanger

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-Autochanger.rst.inc



.. config:option:: sd/device/AutomaticMount

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-AutomaticMount.rst.inc



.. config:option:: sd/device/BackwardSpaceFile

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-BackwardSpaceFile.rst.inc



.. config:option:: sd/device/BackwardSpaceRecord

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-BackwardSpaceRecord.rst.inc



.. config:option:: sd/device/BlockChecksum

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-BlockChecksum.rst.inc



.. config:option:: sd/device/BlockPositioning

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-BlockPositioning.rst.inc



.. config:option:: sd/device/BsfAtEom

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-BsfAtEom.rst.inc



.. config:option:: sd/device/ChangerCommand

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-ChangerCommand.rst.inc



.. config:option:: sd/device/ChangerDevice

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-ChangerDevice.rst.inc



.. config:option:: sd/device/CheckLabels

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-CheckLabels.rst.inc



.. config:option:: sd/device/CloseOnPoll

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-CloseOnPoll.rst.inc



.. config:option:: sd/device/CollectStatistics

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/sd-device-CollectStatistics.rst.inc



.. config:option:: sd/device/Description

   :type: STRING

   The Description directive provides easier human recognition, but is not used by Bareos directly.

   .. include:: /config-directive-description/sd-device-Description.rst.inc



.. config:option:: sd/device/DeviceOptions

   :type: STRING
   :version: 15.2.0

   .. include:: /config-directive-description/sd-device-DeviceOptions.rst.inc



.. config:option:: sd/device/DeviceType

   :type: DEVICE_TYPE

   .. include:: /config-directive-description/sd-device-DeviceType.rst.inc



.. config:option:: sd/device/DiagnosticDevice

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-DiagnosticDevice.rst.inc



.. config:option:: sd/device/DriveCryptoEnabled

   :type: BOOLEAN

   .. include:: /config-directive-description/sd-device-DriveCryptoEnabled.rst.inc



.. config:option:: sd/device/DriveIndex

   :type: PINT16

   .. include:: /config-directive-description/sd-device-DriveIndex.rst.inc



.. config:option:: sd/device/DriveTapeAlertEnabled

   :type: BOOLEAN

   .. include:: /config-directive-description/sd-device-DriveTapeAlertEnabled.rst.inc



.. config:option:: sd/device/FastForwardSpaceFile

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-FastForwardSpaceFile.rst.inc



.. config:option:: sd/device/ForwardSpaceFile

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-ForwardSpaceFile.rst.inc



.. config:option:: sd/device/ForwardSpaceRecord

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-ForwardSpaceRecord.rst.inc



.. config:option:: sd/device/FreeSpaceCommand

   :type: STRNAME
   :version: deprecated

   .. include:: /config-directive-description/sd-device-FreeSpaceCommand.rst.inc



.. config:option:: sd/device/HardwareEndOfFile

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-HardwareEndOfFile.rst.inc



.. config:option:: sd/device/HardwareEndOfMedium

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-HardwareEndOfMedium.rst.inc



.. config:option:: sd/device/LabelBlockSize

   :type: PINT32
   :default: 64512

   .. include:: /config-directive-description/sd-device-LabelBlockSize.rst.inc



.. config:option:: sd/device/LabelMedia

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-LabelMedia.rst.inc



.. config:option:: sd/device/LabelType

   :type: LABEL

   .. include:: /config-directive-description/sd-device-LabelType.rst.inc



.. config:option:: sd/device/MaximumBlockSize

   :type: MAX_BLOCKSIZE

   .. include:: /config-directive-description/sd-device-MaximumBlockSize.rst.inc



.. config:option:: sd/device/MaximumChangerWait

   :type: TIME
   :default: 300

   .. include:: /config-directive-description/sd-device-MaximumChangerWait.rst.inc



.. config:option:: sd/device/MaximumConcurrentJobs

   :type: PINT32

   .. include:: /config-directive-description/sd-device-MaximumConcurrentJobs.rst.inc



.. config:option:: sd/device/MaximumFileSize

   :type: SIZE64
   :default: 1000000000

   .. include:: /config-directive-description/sd-device-MaximumFileSize.rst.inc



.. config:option:: sd/device/MaximumJobSpoolSize

   :type: SIZE64

   .. include:: /config-directive-description/sd-device-MaximumJobSpoolSize.rst.inc



.. config:option:: sd/device/MaximumNetworkBufferSize

   :type: PINT32

   .. include:: /config-directive-description/sd-device-MaximumNetworkBufferSize.rst.inc



.. config:option:: sd/device/MaximumOpenVolumes

   :type: PINT32
   :default: 1

   .. include:: /config-directive-description/sd-device-MaximumOpenVolumes.rst.inc



.. config:option:: sd/device/MaximumOpenWait

   :type: TIME
   :default: 300

   .. include:: /config-directive-description/sd-device-MaximumOpenWait.rst.inc



.. config:option:: sd/device/MaximumPartSize

   :type: SIZE64
   :version: deprecated

   .. include:: /config-directive-description/sd-device-MaximumPartSize.rst.inc



.. config:option:: sd/device/MaximumRewindWait

   :type: TIME
   :default: 300

   .. include:: /config-directive-description/sd-device-MaximumRewindWait.rst.inc



.. config:option:: sd/device/MaximumSpoolSize

   :type: SIZE64

   .. include:: /config-directive-description/sd-device-MaximumSpoolSize.rst.inc



.. config:option:: sd/device/MaximumVolumeSize

   :type: SIZE64
   :version: deprecated

   .. include:: /config-directive-description/sd-device-MaximumVolumeSize.rst.inc



.. config:option:: sd/device/MediaType

   :required: True
   :type: STRNAME

   .. include:: /config-directive-description/sd-device-MediaType.rst.inc



.. config:option:: sd/device/MinimumBlockSize

   :type: PINT32

   .. include:: /config-directive-description/sd-device-MinimumBlockSize.rst.inc



.. config:option:: sd/device/MountCommand

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-MountCommand.rst.inc



.. config:option:: sd/device/MountPoint

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-MountPoint.rst.inc



.. config:option:: sd/device/Name

   :required: True
   :type: NAME

   Unique identifier of the resource.

   .. include:: /config-directive-description/sd-device-Name.rst.inc



.. config:option:: sd/device/NoRewindOnClose

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/sd-device-NoRewindOnClose.rst.inc



.. config:option:: sd/device/OfflineOnUnmount

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-OfflineOnUnmount.rst.inc



.. config:option:: sd/device/QueryCryptoStatus

   :type: BOOLEAN

   .. include:: /config-directive-description/sd-device-QueryCryptoStatus.rst.inc



.. config:option:: sd/device/RandomAccess

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-RandomAccess.rst.inc



.. config:option:: sd/device/RemovableMedia

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-RemovableMedia.rst.inc



.. config:option:: sd/device/RequiresMount

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-RequiresMount.rst.inc



.. config:option:: sd/device/SpoolDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/sd-device-SpoolDirectory.rst.inc



.. config:option:: sd/device/TwoEof

   :type: BIT
   :default: no

   .. include:: /config-directive-description/sd-device-TwoEof.rst.inc



.. config:option:: sd/device/UnmountCommand

   :type: STRNAME

   .. include:: /config-directive-description/sd-device-UnmountCommand.rst.inc



.. config:option:: sd/device/UseMtiocget

   :type: BIT
   :default: yes

   .. include:: /config-directive-description/sd-device-UseMtiocget.rst.inc



.. config:option:: sd/device/VolumeCapacity

   :type: SIZE64

   .. include:: /config-directive-description/sd-device-VolumeCapacity.rst.inc



.. config:option:: sd/device/VolumePollInterval

   :type: TIME
   :default: 300

   .. include:: /config-directive-description/sd-device-VolumePollInterval.rst.inc



.. config:option:: sd/device/WritePartCommand

   :type: STRNAME
   :version: deprecated

   .. include:: /config-directive-description/sd-device-WritePartCommand.rst.inc



