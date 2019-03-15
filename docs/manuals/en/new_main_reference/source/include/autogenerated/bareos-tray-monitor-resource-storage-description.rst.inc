.. config:option:: console/storage/Address

   :required: True
   :type: STRING

   .. include:: /config-directive-description/console-storage-Address.rst.inc



.. config:option:: console/storage/Description

   :type: STRING

   .. include:: /config-directive-description/console-storage-Description.rst.inc



.. config:option:: console/storage/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/console-storage-Name.rst.inc



.. config:option:: console/storage/Password

   :required: True
   :type: MD5PASSWORD

   .. include:: /config-directive-description/console-storage-Password.rst.inc



.. config:option:: console/storage/SdAddress

   :type: STRING

   .. include:: /config-directive-description/console-storage-SdAddress.rst.inc



.. config:option:: console/storage/SdPassword

   :type: MD5PASSWORD

   .. include:: /config-directive-description/console-storage-SdPassword.rst.inc



.. config:option:: console/storage/SdPort

   :type: PINT32
   :default: 9103

   .. include:: /config-directive-description/console-storage-SdPort.rst.inc



.. config:option:: console/storage/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/console-storage-TlsAllowedCn.rst.inc



.. config:option:: console/storage/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/console-storage-TlsAuthenticate.rst.inc



.. config:option:: console/storage/TlsCaCertificateDir

   :type: DIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/console-storage-TlsCaCertificateDir.rst.inc



.. config:option:: console/storage/TlsCaCertificateFile

   :type: DIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/console-storage-TlsCaCertificateFile.rst.inc



.. config:option:: console/storage/TlsCertificate

   :type: DIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/console-storage-TlsCertificate.rst.inc



.. config:option:: console/storage/TlsCertificateRevocationList

   :type: DIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/console-storage-TlsCertificateRevocationList.rst.inc



.. config:option:: console/storage/TlsCipherList

   :type: DIRECTORY

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/console-storage-TlsCipherList.rst.inc



.. config:option:: console/storage/TlsDhFile

   :type: DIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/console-storage-TlsDhFile.rst.inc



.. config:option:: console/storage/TlsEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS support.

   .. include:: /config-directive-description/console-storage-TlsEnable.rst.inc



.. config:option:: console/storage/TlsKey

   :type: DIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/console-storage-TlsKey.rst.inc



.. config:option:: console/storage/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/console-storage-TlsRequire.rst.inc



.. config:option:: console/storage/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/console-storage-TlsVerifyPeer.rst.inc



