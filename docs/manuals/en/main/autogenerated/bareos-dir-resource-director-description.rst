Director
--------

.. config:option:: dir/director/AbsoluteJobTimeout

   :type: PINT32
   :version: 14.2.0

   .. include:: /config-directive-description/dir-director-AbsoluteJobTimeout.rst.inc



.. config:option:: dir/director/AuditEvents

   :type: AUDIT_COMMAND_LIST
   :version: 14.2.0

   .. include:: /config-directive-description/dir-director-AuditEvents.rst.inc



.. config:option:: dir/director/Auditing

   :type: BOOLEAN
   :default: no
   :version: 14.2.0

   .. include:: /config-directive-description/dir-director-Auditing.rst.inc



.. config:option:: dir/director/BackendDirectory

   :type: DIRECTORY_LIST
   :default: /home/joergs/git/bareos/bareos-18.2/regress/usr/lib/bareos/backends *(platform specific)*

   .. include:: /config-directive-description/dir-director-BackendDirectory.rst.inc



.. config:option:: dir/director/Description

   :type: STRING

   .. include:: /config-directive-description/dir-director-Description.rst.inc



.. config:option:: dir/director/DirAddress

   :type: ADDRESS
   :default: 8101

   .. include:: /config-directive-description/dir-director-DirAddress.rst.inc



.. config:option:: dir/director/DirAddresses

   :type: ADDRESSES
   :default: 8101

   .. include:: /config-directive-description/dir-director-DirAddresses.rst.inc



.. config:option:: dir/director/DirPort

   :type: PORT
   :default: 8101

   .. include:: /config-directive-description/dir-director-DirPort.rst.inc



.. config:option:: dir/director/DirSourceAddress

   :type: ADDRESS
   :default: 0

   .. include:: /config-directive-description/dir-director-DirSourceAddress.rst.inc



.. config:option:: dir/director/FdConnectTimeout

   :type: TIME
   :default: 180

   .. include:: /config-directive-description/dir-director-FdConnectTimeout.rst.inc



.. config:option:: dir/director/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/dir-director-HeartbeatInterval.rst.inc



.. config:option:: dir/director/KeyEncryptionKey

   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-director-KeyEncryptionKey.rst.inc



.. config:option:: dir/director/LogTimestampFormat

   :type: STRING
   :version: 15.2.3

   .. include:: /config-directive-description/dir-director-LogTimestampFormat.rst.inc



.. config:option:: dir/director/MaximumConcurrentJobs

   :type: PINT32
   :default: 1

   .. include:: /config-directive-description/dir-director-MaximumConcurrentJobs.rst.inc



.. config:option:: dir/director/MaximumConnections

   :type: PINT32
   :default: 30

   .. include:: /config-directive-description/dir-director-MaximumConnections.rst.inc



.. config:option:: dir/director/MaximumConsoleConnections

   :type: PINT32
   :default: 20

   .. include:: /config-directive-description/dir-director-MaximumConsoleConnections.rst.inc



.. config:option:: dir/director/Messages

   :type: CommonResourceHeader

   .. include:: /config-directive-description/dir-director-Messages.rst.inc



.. config:option:: dir/director/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-director-Name.rst.inc



.. config:option:: dir/director/NdmpLogLevel

   :type: PINT32
   :default: 4
   :version: 13.2.0

   .. include:: /config-directive-description/dir-director-NdmpLogLevel.rst.inc



.. config:option:: dir/director/NdmpSnooping

   :type: BOOLEAN
   :version: 13.2.0

   .. include:: /config-directive-description/dir-director-NdmpSnooping.rst.inc



.. config:option:: dir/director/OmitDefaults

   :type: BOOLEAN
   :default: yes
   :version: deprecated

   Omit config variables with default values when dumping the config.

   .. include:: /config-directive-description/dir-director-OmitDefaults.rst.inc



.. config:option:: dir/director/OptimizeForSize

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-director-OptimizeForSize.rst.inc



.. config:option:: dir/director/OptimizeForSpeed

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-director-OptimizeForSpeed.rst.inc



.. config:option:: dir/director/Password

   :required: True
   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-director-Password.rst.inc



.. config:option:: dir/director/PidDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/dir-director-PidDirectory.rst.inc



.. config:option:: dir/director/PluginDirectory

   :type: DIRECTORY
   :version: 14.2.0

   Plugins are loaded from this directory. To load only specific plugins, use 'Plugin Names'.

   .. include:: /config-directive-description/dir-director-PluginDirectory.rst.inc



.. config:option:: dir/director/PluginNames

   :type: PLUGIN_NAMES
   :version: 14.2.0

   List of plugins, that should get loaded from 'Plugin Directory' (only basenames, '-dir.so' is added automatically). If empty, all plugins will get loaded.

   .. include:: /config-directive-description/dir-director-PluginNames.rst.inc



.. config:option:: dir/director/QueryFile

   :required: True
   :type: DIRECTORY

   .. include:: /config-directive-description/dir-director-QueryFile.rst.inc



.. config:option:: dir/director/ScriptsDirectory

   :type: DIRECTORY

   This directive is currently unused.

   .. include:: /config-directive-description/dir-director-ScriptsDirectory.rst.inc



.. config:option:: dir/director/SdConnectTimeout

   :type: TIME
   :default: 1800

   .. include:: /config-directive-description/dir-director-SdConnectTimeout.rst.inc



.. config:option:: dir/director/SecureEraseCommand

   :type: STRING
   :version: 15.2.1

   Specify command that will be called when bareos unlinks files.

   .. include:: /config-directive-description/dir-director-SecureEraseCommand.rst.inc



.. config:option:: dir/director/StatisticsCollectInterval

   :type: PINT32
   :default: 150
   :version: 14.2.0

   .. include:: /config-directive-description/dir-director-StatisticsCollectInterval.rst.inc



.. config:option:: dir/director/StatisticsRetention

   :type: TIME
   :default: 160704000

   .. include:: /config-directive-description/dir-director-StatisticsRetention.rst.inc



.. config:option:: dir/director/SubSysDirectory

   :type: DIRECTORY
   :version: deprecated

   .. include:: /config-directive-description/dir-director-SubSysDirectory.rst.inc



.. config:option:: dir/director/Subscriptions

   :type: PINT32
   :default: 0
   :version: 12.4.4

   .. include:: /config-directive-description/dir-director-Subscriptions.rst.inc



.. config:option:: dir/director/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/dir-director-TlsAllowedCn.rst.inc



.. config:option:: dir/director/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/dir-director-TlsAuthenticate.rst.inc



.. config:option:: dir/director/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/dir-director-TlsCaCertificateDir.rst.inc



.. config:option:: dir/director/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/dir-director-TlsCaCertificateFile.rst.inc



.. config:option:: dir/director/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/dir-director-TlsCertificate.rst.inc



.. config:option:: dir/director/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/dir-director-TlsCertificateRevocationList.rst.inc



.. config:option:: dir/director/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/dir-director-TlsCipherList.rst.inc



.. config:option:: dir/director/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/dir-director-TlsDhFile.rst.inc



.. config:option:: dir/director/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/dir-director-TlsEnable.rst.inc



.. config:option:: dir/director/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/dir-director-TlsKey.rst.inc



.. config:option:: dir/director/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/dir-director-TlsPskEnable.rst.inc



.. config:option:: dir/director/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/dir-director-TlsPskRequire.rst.inc



.. config:option:: dir/director/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/dir-director-TlsRequire.rst.inc



.. config:option:: dir/director/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/dir-director-TlsVerifyPeer.rst.inc



.. config:option:: dir/director/UsePamAuthentication

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-director-UsePamAuthentication.rst.inc



.. config:option:: dir/director/VerId

   :type: STRING

   .. include:: /config-directive-description/dir-director-VerId.rst.inc



.. config:option:: dir/director/WorkingDirectory

   :type: DIRECTORY
   :default: /home/joergs/git/bareos/bareos-18.2/regress/working *(platform specific)*

   .. include:: /config-directive-description/dir-director-WorkingDirectory.rst.inc



