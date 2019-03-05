.. config:option:: dir/storage/Address

   :required: True
   :type: STRING

   .. include:: /config-directive-description/dir-storage-Address.rst.inc



.. config:option:: dir/storage/AllowCompression

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-storage-AllowCompression.rst.inc



.. config:option:: dir/storage/AuthType

   :type: AUTH_TYPE
   :default: None

   .. include:: /config-directive-description/dir-storage-AuthType.rst.inc



.. config:option:: dir/storage/AutoChanger

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-storage-AutoChanger.rst.inc



.. config:option:: dir/storage/CacheStatusInterval

   :type: TIME
   :default: 30

   .. include:: /config-directive-description/dir-storage-CacheStatusInterval.rst.inc



.. config:option:: dir/storage/CollectStatistics

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-storage-CollectStatistics.rst.inc



.. config:option:: dir/storage/Description

   :type: STRING

   .. include:: /config-directive-description/dir-storage-Description.rst.inc



.. config:option:: dir/storage/Device

   :required: True
   :type: DEVICE

   .. include:: /config-directive-description/dir-storage-Device.rst.inc



.. config:option:: dir/storage/Enabled

   :type: BOOLEAN
   :default: yes

   En- or disable this resource.

   .. include:: /config-directive-description/dir-storage-Enabled.rst.inc



.. config:option:: dir/storage/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/dir-storage-HeartbeatInterval.rst.inc



.. config:option:: dir/storage/LanAddress

   :type: STRING
   :version: 16.2.6

   Sets additional address used for connections between Client and Storage Daemon inside separate network.

   .. include:: /config-directive-description/dir-storage-LanAddress.rst.inc



.. config:option:: dir/storage/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/dir-storage-MaximumBandwidthPerJob.rst.inc



.. config:option:: dir/storage/MaximumConcurrentJobs

   :type: PINT32
   :default: 1

   .. include:: /config-directive-description/dir-storage-MaximumConcurrentJobs.rst.inc



.. config:option:: dir/storage/MaximumConcurrentReadJobs

   :type: PINT32
   :default: 0

   .. include:: /config-directive-description/dir-storage-MaximumConcurrentReadJobs.rst.inc



.. config:option:: dir/storage/MediaType

   :required: True
   :type: STRNAME

   .. include:: /config-directive-description/dir-storage-MediaType.rst.inc



.. config:option:: dir/storage/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-storage-Name.rst.inc



.. config:option:: dir/storage/NdmpChangerDevice

   :type: STRNAME
   :version: 16.2.4

   Allows direct control of a Storage Daemon Auto Changer device by the Director. Only used in NDMP_NATIVE environments.

   .. include:: /config-directive-description/dir-storage-NdmpChangerDevice.rst.inc



.. config:option:: dir/storage/PairedStorage

   :type: RES

   .. include:: /config-directive-description/dir-storage-PairedStorage.rst.inc



.. config:option:: dir/storage/Password

   :required: True
   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-storage-Password.rst.inc



.. config:option:: dir/storage/Port

   :type: PINT32
   :default: 9103

   .. include:: /config-directive-description/dir-storage-Port.rst.inc



.. config:option:: dir/storage/Protocol

   :type: AUTH_PROTOCOL_TYPE
   :default: Native

   .. include:: /config-directive-description/dir-storage-Protocol.rst.inc



.. config:option:: dir/storage/SdAddress

   :type: STRING

   Alias for Address.

   .. include:: /config-directive-description/dir-storage-SdAddress.rst.inc



.. config:option:: dir/storage/SdPassword

   :type: AUTOPASSWORD

   Alias for Password.

   .. include:: /config-directive-description/dir-storage-SdPassword.rst.inc



.. config:option:: dir/storage/SdPort

   :type: PINT32
   :default: 9103

   Alias for Port.

   .. include:: /config-directive-description/dir-storage-SdPort.rst.inc



.. config:option:: dir/storage/SddPort

   :type: PINT32
   :version: deprecated

   .. include:: /config-directive-description/dir-storage-SddPort.rst.inc



.. config:option:: dir/storage/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/dir-storage-TlsAllowedCn.rst.inc



.. config:option:: dir/storage/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/dir-storage-TlsAuthenticate.rst.inc



.. config:option:: dir/storage/TlsCaCertificateDir

   :type: DIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/dir-storage-TlsCaCertificateDir.rst.inc



.. config:option:: dir/storage/TlsCaCertificateFile

   :type: DIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/dir-storage-TlsCaCertificateFile.rst.inc



.. config:option:: dir/storage/TlsCertificate

   :type: DIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/dir-storage-TlsCertificate.rst.inc



.. config:option:: dir/storage/TlsCertificateRevocationList

   :type: DIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/dir-storage-TlsCertificateRevocationList.rst.inc



.. config:option:: dir/storage/TlsCipherList

   :type: DIRECTORY

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/dir-storage-TlsCipherList.rst.inc



.. config:option:: dir/storage/TlsDhFile

   :type: DIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/dir-storage-TlsDhFile.rst.inc



.. config:option:: dir/storage/TlsEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS support.

   .. include:: /config-directive-description/dir-storage-TlsEnable.rst.inc



.. config:option:: dir/storage/TlsKey

   :type: DIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/dir-storage-TlsKey.rst.inc



.. config:option:: dir/storage/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/dir-storage-TlsRequire.rst.inc



.. config:option:: dir/storage/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/dir-storage-TlsVerifyPeer.rst.inc



.. config:option:: dir/storage/Username

   :type: STRING

   .. include:: /config-directive-description/dir-storage-Username.rst.inc



