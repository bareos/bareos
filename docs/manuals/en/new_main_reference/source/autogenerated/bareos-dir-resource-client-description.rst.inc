.. config:option:: dir/client/Address

   :required: True
   :type: STRING

   .. include:: /config-directive-description/dir-client-Address.rst.inc



.. config:option:: dir/client/AllowClientConnect

   :type: BOOLEAN
   :version: deprecated

   Alias of "Connection From Client To Director".

   .. include:: /config-directive-description/dir-client-AllowClientConnect.rst.inc



.. config:option:: dir/client/AuthType

   :type: AUTH_TYPE
   :default: None

   .. include:: /config-directive-description/dir-client-AuthType.rst.inc



.. config:option:: dir/client/AutoPrune

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-client-AutoPrune.rst.inc



.. config:option:: dir/client/Catalog

   :type: RES

   .. include:: /config-directive-description/dir-client-Catalog.rst.inc



.. config:option:: dir/client/ConnectionFromClientToDirector

   :type: BOOLEAN
   :default: no
   :version: 16.2.2

   The Director will accept incoming network connection from this Client.

   .. include:: /config-directive-description/dir-client-ConnectionFromClientToDirector.rst.inc



.. config:option:: dir/client/ConnectionFromDirectorToClient

   :type: BOOLEAN
   :default: yes
   :version: 16.2.2

   Let the Director initiate the network connection to the Client.

   .. include:: /config-directive-description/dir-client-ConnectionFromDirectorToClient.rst.inc



.. config:option:: dir/client/Description

   :type: STRING

   .. include:: /config-directive-description/dir-client-Description.rst.inc



.. config:option:: dir/client/Enabled

   :type: BOOLEAN
   :default: yes

   En- or disable this resource.

   .. include:: /config-directive-description/dir-client-Enabled.rst.inc



.. config:option:: dir/client/FdAddress

   :type: STRING

   Alias for Address.

   .. include:: /config-directive-description/dir-client-FdAddress.rst.inc



.. config:option:: dir/client/FdPassword

   :type: AUTOPASSWORD

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-client-FdPassword.rst.inc



.. config:option:: dir/client/FdPort

   :type: PINT32
   :default: 9102

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-client-FdPort.rst.inc



.. config:option:: dir/client/FileRetention

   :type: TIME
   :default: 5184000

   .. include:: /config-directive-description/dir-client-FileRetention.rst.inc



.. config:option:: dir/client/HardQuota

   :type: SIZE64
   :default: 0

   .. include:: /config-directive-description/dir-client-HardQuota.rst.inc



.. config:option:: dir/client/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/dir-client-HeartbeatInterval.rst.inc



.. config:option:: dir/client/JobRetention

   :type: TIME
   :default: 15552000

   .. include:: /config-directive-description/dir-client-JobRetention.rst.inc



.. config:option:: dir/client/LanAddress

   :type: STRING
   :version: 16.2.6

   Sets additional address used for connections between Client and Storage Daemon inside separate network.

   .. include:: /config-directive-description/dir-client-LanAddress.rst.inc



.. config:option:: dir/client/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/dir-client-MaximumBandwidthPerJob.rst.inc



.. config:option:: dir/client/MaximumConcurrentJobs

   :type: PINT32
   :default: 1

   .. include:: /config-directive-description/dir-client-MaximumConcurrentJobs.rst.inc



.. config:option:: dir/client/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-client-Name.rst.inc



.. config:option:: dir/client/NdmpBlockSize

   :type: SIZE32
   :default: 64512

   .. include:: /config-directive-description/dir-client-NdmpBlockSize.rst.inc



.. config:option:: dir/client/NdmpLogLevel

   :type: PINT32
   :default: 4

   .. include:: /config-directive-description/dir-client-NdmpLogLevel.rst.inc



.. config:option:: dir/client/NdmpUseLmdb

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-client-NdmpUseLmdb.rst.inc



.. config:option:: dir/client/Passive

   :type: BOOLEAN
   :default: no
   :version: 13.2.0

   If enabled, the Storage Daemon will initiate the network connection to the Client. If disabled, the Client will initiate the network connection to the Storage Daemon.

   .. include:: /config-directive-description/dir-client-Passive.rst.inc



.. config:option:: dir/client/Password

   :required: True
   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-client-Password.rst.inc



.. config:option:: dir/client/Port

   :type: PINT32
   :default: 9102

   .. include:: /config-directive-description/dir-client-Port.rst.inc



.. config:option:: dir/client/Protocol

   :type: AUTH_PROTOCOL_TYPE
   :default: Native
   :version: 13.2.0

   .. include:: /config-directive-description/dir-client-Protocol.rst.inc



.. config:option:: dir/client/QuotaIncludeFailedJobs

   :type: BOOLEAN
   :default: yes

   .. include:: /config-directive-description/dir-client-QuotaIncludeFailedJobs.rst.inc



.. config:option:: dir/client/SoftQuota

   :type: SIZE64
   :default: 0

   .. include:: /config-directive-description/dir-client-SoftQuota.rst.inc



.. config:option:: dir/client/SoftQuotaGracePeriod

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/dir-client-SoftQuotaGracePeriod.rst.inc



.. config:option:: dir/client/StrictQuotas

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-client-StrictQuotas.rst.inc



.. config:option:: dir/client/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/dir-client-TlsAllowedCn.rst.inc



.. config:option:: dir/client/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/dir-client-TlsAuthenticate.rst.inc



.. config:option:: dir/client/TlsCaCertificateDir

   :type: DIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/dir-client-TlsCaCertificateDir.rst.inc



.. config:option:: dir/client/TlsCaCertificateFile

   :type: DIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/dir-client-TlsCaCertificateFile.rst.inc



.. config:option:: dir/client/TlsCertificate

   :type: DIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/dir-client-TlsCertificate.rst.inc



.. config:option:: dir/client/TlsCertificateRevocationList

   :type: DIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/dir-client-TlsCertificateRevocationList.rst.inc



.. config:option:: dir/client/TlsCipherList

   :type: DIRECTORY

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/dir-client-TlsCipherList.rst.inc



.. config:option:: dir/client/TlsDhFile

   :type: DIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/dir-client-TlsDhFile.rst.inc



.. config:option:: dir/client/TlsEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS support.

   .. include:: /config-directive-description/dir-client-TlsEnable.rst.inc



.. config:option:: dir/client/TlsKey

   :type: DIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/dir-client-TlsKey.rst.inc



.. config:option:: dir/client/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/dir-client-TlsRequire.rst.inc



.. config:option:: dir/client/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/dir-client-TlsVerifyPeer.rst.inc



.. config:option:: dir/client/Username

   :type: STRING

   .. include:: /config-directive-description/dir-client-Username.rst.inc



