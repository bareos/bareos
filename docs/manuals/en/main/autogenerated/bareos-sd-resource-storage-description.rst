Storage
-------

.. config:option:: sd/storage/AbsoluteJobTimeout

   :type: PINT32

   .. include:: /config-directive-description/sd-storage-AbsoluteJobTimeout.rst.inc



.. config:option:: sd/storage/AllowBandwidthBursting

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-AllowBandwidthBursting.rst.inc



.. config:option:: sd/storage/AutoXFlateOnReplication

   :type: BOOLEAN
   :default: no
   :version: 13.4.0

   .. include:: /config-directive-description/sd-storage-AutoXFlateOnReplication.rst.inc



.. config:option:: sd/storage/BackendDirectory

   :type: DIRECTORY_LIST
   :default: /home/joergs/git/bareos/bareos-18.2/regress/usr/lib/bareos/backends *(platform specific)*

   .. include:: /config-directive-description/sd-storage-BackendDirectory.rst.inc



.. config:option:: sd/storage/ClientConnectWait

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/sd-storage-ClientConnectWait.rst.inc



.. config:option:: sd/storage/CollectDeviceStatistics

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-CollectDeviceStatistics.rst.inc



.. config:option:: sd/storage/CollectJobStatistics

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-CollectJobStatistics.rst.inc



.. config:option:: sd/storage/Compatible

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-Compatible.rst.inc



.. config:option:: sd/storage/Description

   :type: STRING

   .. include:: /config-directive-description/sd-storage-Description.rst.inc



.. config:option:: sd/storage/DeviceReserveByMediaType

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-DeviceReserveByMediaType.rst.inc



.. config:option:: sd/storage/FdConnectTimeout

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/sd-storage-FdConnectTimeout.rst.inc



.. config:option:: sd/storage/FileDeviceConcurrentRead

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-FileDeviceConcurrentRead.rst.inc



.. config:option:: sd/storage/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/sd-storage-HeartbeatInterval.rst.inc



.. config:option:: sd/storage/LogTimestampFormat

   :type: STRING
   :version: 15.2.3

   .. include:: /config-directive-description/sd-storage-LogTimestampFormat.rst.inc



.. config:option:: sd/storage/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/sd-storage-MaximumBandwidthPerJob.rst.inc



.. config:option:: sd/storage/MaximumConcurrentJobs

   :type: PINT32
   :default: 20

   .. include:: /config-directive-description/sd-storage-MaximumConcurrentJobs.rst.inc



.. config:option:: sd/storage/MaximumConnections

   :type: PINT32
   :default: 42
   :version: 15.2.3

   .. include:: /config-directive-description/sd-storage-MaximumConnections.rst.inc



.. config:option:: sd/storage/MaximumNetworkBufferSize

   :type: PINT32

   .. include:: /config-directive-description/sd-storage-MaximumNetworkBufferSize.rst.inc



.. config:option:: sd/storage/Messages

   :type: CommonResourceHeader

   .. include:: /config-directive-description/sd-storage-Messages.rst.inc



.. config:option:: sd/storage/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/sd-storage-Name.rst.inc



.. config:option:: sd/storage/NdmpAddress

   :type: ADDRESS
   :default: 10000

   .. include:: /config-directive-description/sd-storage-NdmpAddress.rst.inc



.. config:option:: sd/storage/NdmpAddresses

   :type: ADDRESSES
   :default: 10000

   .. include:: /config-directive-description/sd-storage-NdmpAddresses.rst.inc



.. config:option:: sd/storage/NdmpEnable

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-NdmpEnable.rst.inc



.. config:option:: sd/storage/NdmpLogLevel

   :type: PINT32
   :default: 4

   .. include:: /config-directive-description/sd-storage-NdmpLogLevel.rst.inc



.. config:option:: sd/storage/NdmpPort

   :type: PORT
   :default: 10000

   .. include:: /config-directive-description/sd-storage-NdmpPort.rst.inc



.. config:option:: sd/storage/NdmpSnooping

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/sd-storage-NdmpSnooping.rst.inc



.. config:option:: sd/storage/PidDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/sd-storage-PidDirectory.rst.inc



.. config:option:: sd/storage/PluginDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/sd-storage-PluginDirectory.rst.inc



.. config:option:: sd/storage/PluginNames

   :type: PLUGIN_NAMES

   .. include:: /config-directive-description/sd-storage-PluginNames.rst.inc



.. config:option:: sd/storage/ScriptsDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/sd-storage-ScriptsDirectory.rst.inc



.. config:option:: sd/storage/SdAddress

   :type: ADDRESS
   :default: 8103

   .. include:: /config-directive-description/sd-storage-SdAddress.rst.inc



.. config:option:: sd/storage/SdAddresses

   :type: ADDRESSES
   :default: 8103

   .. include:: /config-directive-description/sd-storage-SdAddresses.rst.inc



.. config:option:: sd/storage/SdConnectTimeout

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/sd-storage-SdConnectTimeout.rst.inc



.. config:option:: sd/storage/SdPort

   :type: PORT
   :default: 8103

   .. include:: /config-directive-description/sd-storage-SdPort.rst.inc



.. config:option:: sd/storage/SdSourceAddress

   :type: ADDRESS
   :default: 0

   .. include:: /config-directive-description/sd-storage-SdSourceAddress.rst.inc



.. config:option:: sd/storage/SecureEraseCommand

   :type: STRING
   :version: 15.2.1

   Specify command that will be called when bareos unlinks files.

   .. include:: /config-directive-description/sd-storage-SecureEraseCommand.rst.inc



.. config:option:: sd/storage/StatisticsCollectInterval

   :type: PINT32
   :default: 30

   .. include:: /config-directive-description/sd-storage-StatisticsCollectInterval.rst.inc



.. config:option:: sd/storage/SubSysDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/sd-storage-SubSysDirectory.rst.inc



.. config:option:: sd/storage/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/sd-storage-TlsAllowedCn.rst.inc



.. config:option:: sd/storage/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/sd-storage-TlsAuthenticate.rst.inc



.. config:option:: sd/storage/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/sd-storage-TlsCaCertificateDir.rst.inc



.. config:option:: sd/storage/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/sd-storage-TlsCaCertificateFile.rst.inc



.. config:option:: sd/storage/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/sd-storage-TlsCertificate.rst.inc



.. config:option:: sd/storage/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/sd-storage-TlsCertificateRevocationList.rst.inc



.. config:option:: sd/storage/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/sd-storage-TlsCipherList.rst.inc



.. config:option:: sd/storage/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/sd-storage-TlsDhFile.rst.inc



.. config:option:: sd/storage/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/sd-storage-TlsEnable.rst.inc



.. config:option:: sd/storage/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/sd-storage-TlsKey.rst.inc



.. config:option:: sd/storage/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/sd-storage-TlsPskEnable.rst.inc



.. config:option:: sd/storage/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/sd-storage-TlsPskRequire.rst.inc



.. config:option:: sd/storage/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/sd-storage-TlsRequire.rst.inc



.. config:option:: sd/storage/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/sd-storage-TlsVerifyPeer.rst.inc



.. config:option:: sd/storage/VerId

   :type: STRING

   .. include:: /config-directive-description/sd-storage-VerId.rst.inc



.. config:option:: sd/storage/WorkingDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/sd-storage-WorkingDirectory.rst.inc



