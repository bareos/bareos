.. config:option:: console/monitor/Description

   :type: STRING

   .. include:: /config-directive-description/console-monitor-Description.rst.inc



.. config:option:: console/monitor/DirConnectTimeout

   :type: TIME
   :default: 10

   .. include:: /config-directive-description/console-monitor-DirConnectTimeout.rst.inc



.. config:option:: console/monitor/FdConnectTimeout

   :type: TIME
   :default: 10

   .. include:: /config-directive-description/console-monitor-FdConnectTimeout.rst.inc



.. config:option:: console/monitor/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/console-monitor-Name.rst.inc



.. config:option:: console/monitor/Password

   :required: True
   :type: MD5PASSWORD

   .. include:: /config-directive-description/console-monitor-Password.rst.inc



.. config:option:: console/monitor/RefreshInterval

   :type: TIME
   :default: 60

   .. include:: /config-directive-description/console-monitor-RefreshInterval.rst.inc



.. config:option:: console/monitor/SdConnectTimeout

   :type: TIME
   :default: 10

   .. include:: /config-directive-description/console-monitor-SdConnectTimeout.rst.inc



.. config:option:: console/monitor/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/console-monitor-TlsAllowedCn.rst.inc



.. config:option:: console/monitor/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/console-monitor-TlsAuthenticate.rst.inc



.. config:option:: console/monitor/TlsCaCertificateDir

   :type: DIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/console-monitor-TlsCaCertificateDir.rst.inc



.. config:option:: console/monitor/TlsCaCertificateFile

   :type: DIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/console-monitor-TlsCaCertificateFile.rst.inc



.. config:option:: console/monitor/TlsCertificate

   :type: DIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/console-monitor-TlsCertificate.rst.inc



.. config:option:: console/monitor/TlsCertificateRevocationList

   :type: DIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/console-monitor-TlsCertificateRevocationList.rst.inc



.. config:option:: console/monitor/TlsCipherList

   :type: DIRECTORY

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/console-monitor-TlsCipherList.rst.inc



.. config:option:: console/monitor/TlsDhFile

   :type: DIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/console-monitor-TlsDhFile.rst.inc



.. config:option:: console/monitor/TlsEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS support.

   .. include:: /config-directive-description/console-monitor-TlsEnable.rst.inc



.. config:option:: console/monitor/TlsKey

   :type: DIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/console-monitor-TlsKey.rst.inc



.. config:option:: console/monitor/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/console-monitor-TlsRequire.rst.inc



.. config:option:: console/monitor/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/console-monitor-TlsVerifyPeer.rst.inc



