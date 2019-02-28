Console
-------

.. config:option:: console/console/Description

   :type: STRING

   .. include:: /config-directive-description/console-console-Description.rst.inc



.. config:option:: console/console/Director

   :type: STRING

   .. include:: /config-directive-description/console-console-Director.rst.inc



.. config:option:: console/console/HeartbeatInterval

   :type: TIME
   :default: 0

   .. include:: /config-directive-description/console-console-HeartbeatInterval.rst.inc



.. config:option:: console/console/HistoryFile

   :type: DIRECTORY

   .. include:: /config-directive-description/console-console-HistoryFile.rst.inc



.. config:option:: console/console/HistoryLength

   :type: PINT32
   :default: 100

   .. include:: /config-directive-description/console-console-HistoryLength.rst.inc



.. config:option:: console/console/Name

   :required: True
   :type: NAME

   The name of this resource.

   .. include:: /config-directive-description/console-console-Name.rst.inc



.. config:option:: console/console/Password

   :required: True
   :type: MD5PASSWORD

   .. include:: /config-directive-description/console-console-Password.rst.inc



.. config:option:: console/console/RcFile

   :type: DIRECTORY

   .. include:: /config-directive-description/console-console-RcFile.rst.inc



.. config:option:: console/console/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/console-console-TlsAllowedCn.rst.inc



.. config:option:: console/console/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/console-console-TlsAuthenticate.rst.inc



.. config:option:: console/console/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/console-console-TlsCaCertificateDir.rst.inc



.. config:option:: console/console/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/console-console-TlsCaCertificateFile.rst.inc



.. config:option:: console/console/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/console-console-TlsCertificate.rst.inc



.. config:option:: console/console/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/console-console-TlsCertificateRevocationList.rst.inc



.. config:option:: console/console/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/console-console-TlsCipherList.rst.inc



.. config:option:: console/console/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/console-console-TlsDhFile.rst.inc



.. config:option:: console/console/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/console-console-TlsEnable.rst.inc



.. config:option:: console/console/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/console-console-TlsKey.rst.inc



.. config:option:: console/console/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/console-console-TlsPskEnable.rst.inc



.. config:option:: console/console/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/console-console-TlsPskRequire.rst.inc



.. config:option:: console/console/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/console-console-TlsRequire.rst.inc



.. config:option:: console/console/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/console-console-TlsVerifyPeer.rst.inc



