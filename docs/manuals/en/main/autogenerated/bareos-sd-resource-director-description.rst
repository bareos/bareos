Director
--------

.. config:option:: sd/director/Description

   :type: STRING

   .. include:: /config-directive-description/sd-director-Description.rst.inc



.. config:option:: sd/director/KeyEncryptionKey

   :type: AUTOPASSWORD

   .. include:: /config-directive-description/sd-director-KeyEncryptionKey.rst.inc



.. config:option:: sd/director/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/sd-director-MaximumBandwidthPerJob.rst.inc



.. config:option:: sd/director/Monitor

   :type: BOOLEAN

   .. include:: /config-directive-description/sd-director-Monitor.rst.inc



.. config:option:: sd/director/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/sd-director-Name.rst.inc



.. config:option:: sd/director/Password

   :required: True
   :type: AUTOPASSWORD

   .. include:: /config-directive-description/sd-director-Password.rst.inc



.. config:option:: sd/director/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/sd-director-TlsAllowedCn.rst.inc



.. config:option:: sd/director/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/sd-director-TlsAuthenticate.rst.inc



.. config:option:: sd/director/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/sd-director-TlsCaCertificateDir.rst.inc



.. config:option:: sd/director/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/sd-director-TlsCaCertificateFile.rst.inc



.. config:option:: sd/director/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/sd-director-TlsCertificate.rst.inc



.. config:option:: sd/director/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/sd-director-TlsCertificateRevocationList.rst.inc



.. config:option:: sd/director/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/sd-director-TlsCipherList.rst.inc



.. config:option:: sd/director/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/sd-director-TlsDhFile.rst.inc



.. config:option:: sd/director/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/sd-director-TlsEnable.rst.inc



.. config:option:: sd/director/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/sd-director-TlsKey.rst.inc



.. config:option:: sd/director/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/sd-director-TlsPskEnable.rst.inc



.. config:option:: sd/director/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/sd-director-TlsPskRequire.rst.inc



.. config:option:: sd/director/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/sd-director-TlsRequire.rst.inc



.. config:option:: sd/director/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/sd-director-TlsVerifyPeer.rst.inc



