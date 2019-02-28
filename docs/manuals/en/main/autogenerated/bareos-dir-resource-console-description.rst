Console
-------

.. config:option:: dir/console/CatalogACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-CatalogACL.rst.inc



.. config:option:: dir/console/ClientACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-ClientACL.rst.inc



.. config:option:: dir/console/CommandACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-CommandACL.rst.inc



.. config:option:: dir/console/Description

   :type: STRING

   .. include:: /config-directive-description/dir-console-Description.rst.inc



.. config:option:: dir/console/FileSetACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-FileSetACL.rst.inc



.. config:option:: dir/console/JobACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-JobACL.rst.inc



.. config:option:: dir/console/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/dir-console-Name.rst.inc



.. config:option:: dir/console/Password

   :required: True
   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-console-Password.rst.inc



.. config:option:: dir/console/PluginOptionsACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-PluginOptionsACL.rst.inc



.. config:option:: dir/console/PoolACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-PoolACL.rst.inc



.. config:option:: dir/console/Profile

   :type: RESOURCE_LIST
   :version: 14.2.3

   Profiles can be assigned to a Console. ACL are checked until either a deny ACL is found or an allow ACL. First the console ACL is checked then any profile the console is linked to.

   .. include:: /config-directive-description/dir-console-Profile.rst.inc



.. config:option:: dir/console/RunACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-RunACL.rst.inc



.. config:option:: dir/console/ScheduleACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-ScheduleACL.rst.inc



.. config:option:: dir/console/StorageACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-StorageACL.rst.inc



.. config:option:: dir/console/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/dir-console-TlsAllowedCn.rst.inc



.. config:option:: dir/console/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/dir-console-TlsAuthenticate.rst.inc



.. config:option:: dir/console/TlsCaCertificateDir

   :type: STDDIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/dir-console-TlsCaCertificateDir.rst.inc



.. config:option:: dir/console/TlsCaCertificateFile

   :type: STDDIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/dir-console-TlsCaCertificateFile.rst.inc



.. config:option:: dir/console/TlsCertificate

   :type: STDDIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/dir-console-TlsCertificate.rst.inc



.. config:option:: dir/console/TlsCertificateRevocationList

   :type: STDDIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/dir-console-TlsCertificateRevocationList.rst.inc



.. config:option:: dir/console/TlsCipherList

   :type: STRING

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/dir-console-TlsCipherList.rst.inc



.. config:option:: dir/console/TlsDhFile

   :type: STDDIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/dir-console-TlsDhFile.rst.inc



.. config:option:: dir/console/TlsEnable

   :type: BOOLEAN
   :default: no

   Enable TLS support.

   .. include:: /config-directive-description/dir-console-TlsEnable.rst.inc



.. config:option:: dir/console/TlsKey

   :type: STDDIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/dir-console-TlsKey.rst.inc



.. config:option:: dir/console/TlsPskEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS-PSK support.

   .. include:: /config-directive-description/dir-console-TlsPskEnable.rst.inc



.. config:option:: dir/console/TlsPskRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencryption connections. Enabling this implicitly sets "TLS-PSK Enable = yes".

   .. include:: /config-directive-description/dir-console-TlsPskRequire.rst.inc



.. config:option:: dir/console/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/dir-console-TlsRequire.rst.inc



.. config:option:: dir/console/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/dir-console-TlsVerifyPeer.rst.inc



.. config:option:: dir/console/WhereACL

   :type: ACL

   .. include:: /config-directive-description/dir-console-WhereACL.rst.inc



