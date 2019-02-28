FileDaemon
----------

.. config:option:: fd/filedaemon/AbsoluteJobTimeout

   :type: PINT32

   .. include:: /config-directive-description/fd-filedaemon-AbsoluteJobTimeout.rst.inc



.. config:option:: fd/filedaemon/AllowBandwidthBursting

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-filedaemon-AllowBandwidthBursting.rst.inc



.. config:option:: fd/filedaemon/AllowedJobCommand

   :type: STRING_LIST

   .. include:: /config-directive-description/fd-filedaemon-AllowedJobCommand.rst.inc



.. config:option:: fd/filedaemon/AllowedScriptDir

   :type: DIRECTORY_LIST

   .. include:: /config-directive-description/fd-filedaemon-AllowedScriptDir.rst.inc



.. config:option:: fd/filedaemon/AlwaysUseLmdb

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-filedaemon-AlwaysUseLmdb.rst.inc



.. config:option:: fd/filedaemon/Compatible

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-filedaemon-Compatible.rst.inc



.. config:option:: fd/filedaemon/Description

   :type: STRING

   .. include:: /config-directive-description/fd-filedaemon-Description.rst.inc



.. config:option:: fd/filedaemon/FdAddress

   :type: ADDRESS
   :default: 8102

   .. include:: /config-directive-description/fd-filedaemon-FdAddress.rst.inc



.. config:option:: fd/filedaemon/FdAddresses

   :type: ADDRESSES
   :default: 8102

   .. include:: /config-directive-description/fd-filedaemon-FdAddresses.rst.inc



.. config:option:: fd/filedaemon/FdPort

   :type: PORT
   :default: 8102

   .. include:: /config-directive-description/fd-filedaemon-FdPort.rst.inc



.. config:option:: fd/filedaemon/FdSourceAddress

   :type: ADDRESS
   :default: 0

   .. include:: /config-directive-description/fd-filedaemon-FdSourceAddress.rst.inc



.. config:option:: fd/filedaemon/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/fd-filedaemon-HeartbeatInterval.rst.inc



.. config:option:: fd/filedaemon/LmdbThreshold

   :type: PINT32

   .. include:: /config-directive-description/fd-filedaemon-LmdbThreshold.rst.inc



.. config:option:: fd/filedaemon/LogTimestampFormat

   :type: STRING
   :version: 15.2.3

   .. include:: /config-directive-description/fd-filedaemon-LogTimestampFormat.rst.inc



.. config:option:: fd/filedaemon/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/fd-filedaemon-MaximumBandwidthPerJob.rst.inc



.. config:option:: fd/filedaemon/MaximumConcurrentJobs

   :type: PINT32
   :default: 20

   .. include:: /config-directive-description/fd-filedaemon-MaximumConcurrentJobs.rst.inc



.. config:option:: fd/filedaemon/MaximumConnections

   :type: PINT32
   :default: 42
   :version: 15.2.3

   .. include:: /config-directive-description/fd-filedaemon-MaximumConnections.rst.inc



.. config:option:: fd/filedaemon/MaximumNetworkBufferSize

   :type: PINT32

   .. include:: /config-directive-description/fd-filedaemon-MaximumNetworkBufferSize.rst.inc



.. config:option:: fd/filedaemon/Messages

   :type: CommonResourceHeader

   .. include:: /config-directive-description/fd-filedaemon-Messages.rst.inc



.. config:option:: fd/filedaemon/Name

   :required: True
   :type: NAME

   The name of this resource. It is used to reference to it.

   .. include:: /config-directive-description/fd-filedaemon-Name.rst.inc



.. config:option:: fd/filedaemon/PidDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/fd-filedaemon-PidDirectory.rst.inc



.. config:option:: fd/filedaemon/PkiCipher

   :type: ENCRYPTION_CIPHER
   :default: aes128

   PKI Cipher used for data encryption.

   .. include:: /config-directive-description/fd-filedaemon-PkiCipher.rst.inc



.. config:option:: fd/filedaemon/PkiEncryption

   :type: BOOLEAN
   :default: no

   Enable Data Encryption.

   .. include:: /config-directive-description/fd-filedaemon-PkiEncryption.rst.inc



.. config:option:: fd/filedaemon/PkiKeyPair

   :type: DIRECTORY

   File with public and private key to sign, encrypt (backup) and decrypt (restore) the data.

   .. include:: /config-directive-description/fd-filedaemon-PkiKeyPair.rst.inc



.. config:option:: fd/filedaemon/PkiMasterKey

   :type: DIRECTORY_LIST

   List of public key files. Data will be decryptable via the corresponding private keys.

   .. include:: /config-directive-description/fd-filedaemon-PkiMasterKey.rst.inc



.. config:option:: fd/filedaemon/PkiSignatures

   :type: BOOLEAN
   :default: no

   Enable Data Signing.

   .. include:: /config-directive-description/fd-filedaemon-PkiSignatures.rst.inc



.. config:option:: fd/filedaemon/PkiSigner

   :type: DIRECTORY_LIST

   Additional public/private key files to sign or verify the data.

   .. include:: /config-directive-description/fd-filedaemon-PkiSigner.rst.inc



.. config:option:: fd/filedaemon/PluginDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/fd-filedaemon-PluginDirectory.rst.inc



.. config:option:: fd/filedaemon/PluginNames

   :type: PLUGIN_NAMES

   .. include:: /config-directive-description/fd-filedaemon-PluginNames.rst.inc



.. config:option:: fd/filedaemon/ScriptsDirectory

   :type: DIRECTORY

   .. include:: /config-directive-description/fd-filedaemon-ScriptsDirectory.rst.inc



.. config:option:: fd/filedaemon/SdConnectTimeout

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/fd-filedaemon-SdConnectTimeout.rst.inc



.. config:option:: fd/filedaemon/SecureEraseCommand

   :type: STRING
   :version: 15.2.1

   Specify command that will be called when bareos unlinks files.

   .. include:: /config-directive-description/fd-filedaemon-SecureEraseCommand.rst.inc



.. config:option:: fd/filedaemon/SubSysDirectory

   :type: DIRECTORY
   :version: deprecated

   .. include:: /config-directive-description/fd-filedaemon-SubSysDirectory.rst.inc



.. config:option:: fd/filedaemon/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/fd-filedaemon-TlsAllowedCn.rst.inc



.. config:option:: fd/filedaemon/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/fd-filedaemon-TlsAuthenticate.rst.inc



.. config:option:: fd/filedaemon/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/fd-filedaemon-TlsCaCertificateDir.rst.inc



.. config:option:: fd/filedaemon/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/fd-filedaemon-TlsCaCertificateFile.rst.inc



.. config:option:: fd/filedaemon/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/fd-filedaemon-TlsCertificate.rst.inc



.. config:option:: fd/filedaemon/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/fd-filedaemon-TlsCertificateRevocationList.rst.inc



.. config:option:: fd/filedaemon/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/fd-filedaemon-TlsCipherList.rst.inc



.. config:option:: fd/filedaemon/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/fd-filedaemon-TlsDhFile.rst.inc



.. config:option:: fd/filedaemon/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/fd-filedaemon-TlsEnable.rst.inc



.. config:option:: fd/filedaemon/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/fd-filedaemon-TlsKey.rst.inc



.. config:option:: fd/filedaemon/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/fd-filedaemon-TlsPskEnable.rst.inc



.. config:option:: fd/filedaemon/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/fd-filedaemon-TlsPskRequire.rst.inc



.. config:option:: fd/filedaemon/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/fd-filedaemon-TlsRequire.rst.inc



.. config:option:: fd/filedaemon/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/fd-filedaemon-TlsVerifyPeer.rst.inc



.. config:option:: fd/filedaemon/VerId

   :type: STRING

   .. include:: /config-directive-description/fd-filedaemon-VerId.rst.inc



.. config:option:: fd/filedaemon/WorkingDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/fd-filedaemon-WorkingDirectory.rst.inc



