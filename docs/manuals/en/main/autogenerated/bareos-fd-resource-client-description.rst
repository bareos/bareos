Client
------

.. config:option:: fd/client/AbsoluteJobTimeout

   :type: PINT32

   .. include:: /config-directive-description/fd-client-AbsoluteJobTimeout.rst.inc



.. config:option:: fd/client/AllowBandwidthBursting

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-client-AllowBandwidthBursting.rst.inc



.. config:option:: fd/client/AllowedJobCommand

   :type: STRING_LIST

   .. include:: /config-directive-description/fd-client-AllowedJobCommand.rst.inc



.. config:option:: fd/client/AllowedScriptDir

   :type: DIRECTORY_LIST

   .. include:: /config-directive-description/fd-client-AllowedScriptDir.rst.inc



.. config:option:: fd/client/AlwaysUseLmdb

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-client-AlwaysUseLmdb.rst.inc



.. config:option:: fd/client/Compatible

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-client-Compatible.rst.inc



.. config:option:: fd/client/Description

   :type: STRING

   .. include:: /config-directive-description/fd-client-Description.rst.inc



.. config:option:: fd/client/FdAddress

   :type: ADDRESS
   :default: 8102

   .. include:: /config-directive-description/fd-client-FdAddress.rst.inc



.. config:option:: fd/client/FdAddresses

   :type: ADDRESSES
   :default: 8102

   .. include:: /config-directive-description/fd-client-FdAddresses.rst.inc



.. config:option:: fd/client/FdPort

   :type: PORT
   :default: 8102

   .. include:: /config-directive-description/fd-client-FdPort.rst.inc



.. config:option:: fd/client/FdSourceAddress

   :type: ADDRESS
   :default: 0

   .. include:: /config-directive-description/fd-client-FdSourceAddress.rst.inc



.. config:option:: fd/client/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/fd-client-HeartbeatInterval.rst.inc



.. config:option:: fd/client/LmdbThreshold

   :type: PINT32

   .. include:: /config-directive-description/fd-client-LmdbThreshold.rst.inc



.. config:option:: fd/client/LogTimestampFormat

   :type: STRING
   :version: 15.2.3

   .. include:: /config-directive-description/fd-client-LogTimestampFormat.rst.inc



.. config:option:: fd/client/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/fd-client-MaximumBandwidthPerJob.rst.inc



.. config:option:: fd/client/MaximumConcurrentJobs

   :type: PINT32
   :default: 20

   .. include:: /config-directive-description/fd-client-MaximumConcurrentJobs.rst.inc



.. config:option:: fd/client/MaximumConnections

   :type: PINT32
   :default: 42
   :version: 15.2.3

   .. include:: /config-directive-description/fd-client-MaximumConnections.rst.inc



.. config:option:: fd/client/MaximumNetworkBufferSize

   :type: PINT32

   .. include:: /config-directive-description/fd-client-MaximumNetworkBufferSize.rst.inc



.. config:option:: fd/client/Messages

   :type: CommonResourceHeader

   .. include:: /config-directive-description/fd-client-Messages.rst.inc



.. config:option:: fd/client/Name

   :required: True
   :type: NAME

   The name of this resource. It is used to reference to it.

   .. include:: /config-directive-description/fd-client-Name.rst.inc



.. config:option:: fd/client/PidDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/fd-client-PidDirectory.rst.inc



.. config:option:: fd/client/PkiCipher

   :type: ENCRYPTION_CIPHER
   :default: aes128

   PKI Cipher used for data encryption.

   .. include:: /config-directive-description/fd-client-PkiCipher.rst.inc



.. config:option:: fd/client/PkiEncryption

   :type: BOOLEAN
   :default: no

   Enable Data Encryption.

   .. include:: /config-directive-description/fd-client-PkiEncryption.rst.inc



.. config:option:: fd/client/PkiKeyPair

   :type: DIRECTORY

   File with public and private key to sign, encrypt (backup) and decrypt (restore) the data.

   .. include:: /config-directive-description/fd-client-PkiKeyPair.rst.inc



.. config:option:: fd/client/PkiMasterKey

   :type: DIRECTORY_LIST

   List of public key files. Data will be decryptable via the corresponding private keys.

   .. include:: /config-directive-description/fd-client-PkiMasterKey.rst.inc



.. config:option:: fd/client/PkiSignatures

   :type: BOOLEAN
   :default: no

   Enable Data Signing.

   .. include:: /config-directive-description/fd-client-PkiSignatures.rst.inc



.. config:option:: fd/client/PkiSigner

   :type: DIRECTORY_LIST

   Additional public/private key files to sign or verify the data.

   .. include:: /config-directive-description/fd-client-PkiSigner.rst.inc



.. config:option:: fd/client/PluginDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/fd-client-PluginDirectory.rst.inc



.. config:option:: fd/client/PluginNames

   :type: PLUGIN_NAMES

   .. include:: /config-directive-description/fd-client-PluginNames.rst.inc



.. config:option:: fd/client/ScriptsDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/fd-client-ScriptsDirectory.rst.inc



.. config:option:: fd/client/SdConnectTimeout

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/fd-client-SdConnectTimeout.rst.inc



.. config:option:: fd/client/SecureEraseCommand

   :type: STRING
   :version: 15.2.1

   Specify command that will be called when bareos unlinks files.

   .. include:: /config-directive-description/fd-client-SecureEraseCommand.rst.inc



.. config:option:: fd/client/SubSysDirectory

   :type: DIRECTORY
   :version: deprecated

   .. include:: /config-directive-description/fd-client-SubSysDirectory.rst.inc



.. config:option:: fd/client/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/fd-client-TlsAllowedCn.rst.inc



.. config:option:: fd/client/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/fd-client-TlsAuthenticate.rst.inc



.. config:option:: fd/client/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/fd-client-TlsCaCertificateDir.rst.inc



.. config:option:: fd/client/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/fd-client-TlsCaCertificateFile.rst.inc



.. config:option:: fd/client/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/fd-client-TlsCertificate.rst.inc



.. config:option:: fd/client/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/fd-client-TlsCertificateRevocationList.rst.inc



.. config:option:: fd/client/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/fd-client-TlsCipherList.rst.inc



.. config:option:: fd/client/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/fd-client-TlsDhFile.rst.inc



.. config:option:: fd/client/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/fd-client-TlsEnable.rst.inc



.. config:option:: fd/client/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/fd-client-TlsKey.rst.inc



.. config:option:: fd/client/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/fd-client-TlsPskEnable.rst.inc



.. config:option:: fd/client/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/fd-client-TlsPskRequire.rst.inc



.. config:option:: fd/client/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/fd-client-TlsRequire.rst.inc



.. config:option:: fd/client/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/fd-client-TlsVerifyPeer.rst.inc



.. config:option:: fd/client/VerId

   :type: STRING

   .. include:: /config-directive-description/fd-client-VerId.rst.inc



.. config:option:: fd/client/WorkingDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/fd-client-WorkingDirectory.rst.inc



