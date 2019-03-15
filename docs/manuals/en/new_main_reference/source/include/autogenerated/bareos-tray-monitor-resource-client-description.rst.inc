.. config:option:: console/client/Address

   :required: True
   :type: STRING

   .. include:: /config-directive-description/console-client-Address.rst.inc



.. config:option:: console/client/Description

   :type: STRING

   .. include:: /config-directive-description/console-client-Description.rst.inc



.. config:option:: console/client/FdPort

   :type: PINT32
   :default: 9102

   .. include:: /config-directive-description/console-client-FdPort.rst.inc



.. config:option:: console/client/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/console-client-Name.rst.inc



.. config:option:: console/client/Password

   :required: True
   :type: MD5PASSWORD

   .. include:: /config-directive-description/console-client-Password.rst.inc



.. config:option:: console/client/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/console-client-TlsAllowedCn.rst.inc



.. config:option:: console/client/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/console-client-TlsAuthenticate.rst.inc



.. config:option:: console/client/TlsCaCertificateDir

   :type: DIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/console-client-TlsCaCertificateDir.rst.inc



.. config:option:: console/client/TlsCaCertificateFile

   :type: DIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/console-client-TlsCaCertificateFile.rst.inc



.. config:option:: console/client/TlsCertificate

   :type: DIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/console-client-TlsCertificate.rst.inc



.. config:option:: console/client/TlsCertificateRevocationList

   :type: DIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/console-client-TlsCertificateRevocationList.rst.inc



.. config:option:: console/client/TlsCipherList

   :type: DIRECTORY

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/console-client-TlsCipherList.rst.inc



.. config:option:: console/client/TlsDhFile

   :type: DIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/console-client-TlsDhFile.rst.inc



.. config:option:: console/client/TlsEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS support.

   .. include:: /config-directive-description/console-client-TlsEnable.rst.inc



.. config:option:: console/client/TlsKey

   :type: DIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/console-client-TlsKey.rst.inc



.. config:option:: console/client/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/console-client-TlsRequire.rst.inc



.. config:option:: console/client/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/console-client-TlsVerifyPeer.rst.inc



