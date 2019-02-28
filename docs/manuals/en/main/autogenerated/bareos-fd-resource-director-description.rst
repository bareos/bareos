Director
--------

.. config:option:: fd/director/Address

   :type: STRING

   Director Network Address. Only required if "Connection From Client To Director" is enabled.

   .. include:: /config-directive-description/fd-director-Address.rst.inc



.. config:option:: fd/director/AllowedJobCommand

   :type: STRING_LIST

   .. include:: /config-directive-description/fd-director-AllowedJobCommand.rst.inc



.. config:option:: fd/director/AllowedScriptDir

   :type: DIRECTORY_LIST

   .. include:: /config-directive-description/fd-director-AllowedScriptDir.rst.inc



.. config:option:: fd/director/ConnectionFromClientToDirector

   :type: BOOLEAN
   :default: no
   :version: 16.2.2

   Let the Filedaemon initiate network connections to the Director.

   .. include:: /config-directive-description/fd-director-ConnectionFromClientToDirector.rst.inc



.. config:option:: fd/director/ConnectionFromDirectorToClient

   :type: BOOLEAN
   :default: yes
   :version: 16.2.2

   This Client will accept incoming network connection from this Director.

   .. include:: /config-directive-description/fd-director-ConnectionFromDirectorToClient.rst.inc



.. config:option:: fd/director/Description

   :type: STRING

   .. include:: /config-directive-description/fd-director-Description.rst.inc



.. config:option:: fd/director/MaximumBandwidthPerJob

   :type: SPEED

   .. include:: /config-directive-description/fd-director-MaximumBandwidthPerJob.rst.inc



.. config:option:: fd/director/Monitor

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/fd-director-Monitor.rst.inc



.. config:option:: fd/director/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/fd-director-Name.rst.inc



.. config:option:: fd/director/Password

   :required: True
   :type: MD5PASSWORD

   .. include:: /config-directive-description/fd-director-Password.rst.inc



.. config:option:: fd/director/Port

   :type: PINT32
   :default: 8101
   :version: 16.2.2

   Director Network Port. Only used if "Connection From Client To Director" is enabled.

   .. include:: /config-directive-description/fd-director-Port.rst.inc



.. config:option:: fd/director/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/fd-director-TlsAllowedCn.rst.inc



.. config:option:: fd/director/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/fd-director-TlsAuthenticate.rst.inc



.. config:option:: fd/director/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/fd-director-TlsCaCertificateDir.rst.inc



.. config:option:: fd/director/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/fd-director-TlsCaCertificateFile.rst.inc



.. config:option:: fd/director/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/fd-director-TlsCertificate.rst.inc



.. config:option:: fd/director/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/fd-director-TlsCertificateRevocationList.rst.inc



.. config:option:: fd/director/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/fd-director-TlsCipherList.rst.inc



.. config:option:: fd/director/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/fd-director-TlsDhFile.rst.inc



.. config:option:: fd/director/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/fd-director-TlsEnable.rst.inc



.. config:option:: fd/director/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/fd-director-TlsKey.rst.inc



.. config:option:: fd/director/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/fd-director-TlsPskEnable.rst.inc



.. config:option:: fd/director/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/fd-director-TlsPskRequire.rst.inc



.. config:option:: fd/director/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/fd-director-TlsRequire.rst.inc



.. config:option:: fd/director/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/fd-director-TlsVerifyPeer.rst.inc



