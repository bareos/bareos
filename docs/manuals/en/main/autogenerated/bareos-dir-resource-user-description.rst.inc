.. config:option:: dir/user/CatalogACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-CatalogACL.rst.inc



.. config:option:: dir/user/ClientACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-ClientACL.rst.inc



.. config:option:: dir/user/CommandACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-CommandACL.rst.inc



.. config:option:: dir/user/Description

   :type: STRING

   .. include:: /config-directive-description/dir-user-Description.rst.inc



.. config:option:: dir/user/FileSetACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-FileSetACL.rst.inc



.. config:option:: dir/user/JobACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-JobACL.rst.inc



.. config:option:: dir/user/Name

   :required: True
   :type: NAME

   .. include:: /config-directive-description/dir-user-Name.rst.inc



.. config:option:: dir/user/Password

   :required: True
   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-user-Password.rst.inc



.. config:option:: dir/user/PluginOptionsACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-PluginOptionsACL.rst.inc



.. config:option:: dir/user/PoolACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-PoolACL.rst.inc



.. config:option:: dir/user/Profile

   :type: RESOURCE_LIST
   :version: 14.2.3

   Profiles can be assigned to a Console. ACL are checked until either a deny ACL is found or an allow ACL. First the console ACL is checked then any profile the console is linked to.

   .. include:: /config-directive-description/dir-user-Profile.rst.inc



.. config:option:: dir/user/RunACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-RunACL.rst.inc



.. config:option:: dir/user/ScheduleACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-ScheduleACL.rst.inc



.. config:option:: dir/user/StorageACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-StorageACL.rst.inc



.. config:option:: dir/user/TlsAllowedCn

   :type: STRING_LIST

   "Common Name"s (CNs) of the allowed peer certificates.

   .. include:: /config-directive-description/dir-user-TlsAllowedCn.rst.inc



.. config:option:: dir/user/TlsAuthenticate

   :type: BOOLEAN
   :default: no

   Use TLS only to authenticate, not for encryption.

   .. include:: /config-directive-description/dir-user-TlsAuthenticate.rst.inc



.. config:option:: dir/user/TlsCaCertificateDir

   :type: DIRECTORY

   Path of a TLS CA certificate directory.

   .. include:: /config-directive-description/dir-user-TlsCaCertificateDir.rst.inc



.. config:option:: dir/user/TlsCaCertificateFile

   :type: DIRECTORY

   Path of a PEM encoded TLS CA certificate(s) file.

   .. include:: /config-directive-description/dir-user-TlsCaCertificateFile.rst.inc



.. config:option:: dir/user/TlsCertificate

   :type: DIRECTORY

   Path of a PEM encoded TLS certificate.

   .. include:: /config-directive-description/dir-user-TlsCertificate.rst.inc



.. config:option:: dir/user/TlsCertificateRevocationList

   :type: DIRECTORY

   Path of a Certificate Revocation List file.

   .. include:: /config-directive-description/dir-user-TlsCertificateRevocationList.rst.inc



.. config:option:: dir/user/TlsCipherList

   :type: DIRECTORY

   List of valid TLS Ciphers.

   .. include:: /config-directive-description/dir-user-TlsCipherList.rst.inc



.. config:option:: dir/user/TlsDhFile

   :type: DIRECTORY

   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications.

   .. include:: /config-directive-description/dir-user-TlsDhFile.rst.inc



.. config:option:: dir/user/TlsEnable

   :type: BOOLEAN
   :default: yes

   Enable TLS support.

   .. include:: /config-directive-description/dir-user-TlsEnable.rst.inc



.. config:option:: dir/user/TlsKey

   :type: DIRECTORY

   Path of a PEM encoded private key. It must correspond to the specified "TLS Certificate".

   .. include:: /config-directive-description/dir-user-TlsKey.rst.inc



.. config:option:: dir/user/TlsRequire

   :type: BOOLEAN
   :default: no

   Without setting this to yes, Bareos can fall back to use unencrypted connections. Enabling this implicitly sets "TLS Enable = yes".

   .. include:: /config-directive-description/dir-user-TlsRequire.rst.inc



.. config:option:: dir/user/TlsVerifyPeer

   :type: BOOLEAN
   :default: no

   If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the "TLS Allowed CN" list.

   .. include:: /config-directive-description/dir-user-TlsVerifyPeer.rst.inc



.. config:option:: dir/user/UsePamAuthentication

   :type: BOOLEAN
   :default: no
   :version: 18.2.4

   .. include:: /config-directive-description/dir-user-UsePamAuthentication.rst.inc



.. config:option:: dir/user/WhereACL

   :type: ACL

   .. include:: /config-directive-description/dir-user-WhereACL.rst.inc



