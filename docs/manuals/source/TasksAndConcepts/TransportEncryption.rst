.. _CommEncryption:

.. _section-TransportEncryption:

Transport Encryption
====================

.. index:: Communications Encryption
.. index:: Transport Encryption
.. index:: Encryption; Transport
.. index:: TLS
.. index:: TLS-PSK
.. index:: SSL

Bareos uses TLS (Transport Layer Security) to provide secure network transport. For data encryption in contrast, please see the :ref:`DataEncryption` chapter. The initial Bacula encryption implementation has been written by Landon Fuller.

With :sinceVersion:`18.2.4: TLS-PSK` the TLS code has been enhanced by the TLS-PSK (Pre Shared Keys) feature which allows the daemons to setup an encrypted connection directly without using certificates. The library used for TLS is openSSL.

.. _TlsDirectives:

TLS Configuration Directives
----------------------------

Additional configuration directives have been added to all the daemons (Director, File daemon, and Storage daemon) as well as the various different Console programs. These directives are defined as follows:

:config:option:`dir/director/TlsEnable`\
   Enable TLS support. This is by default enabled. If no certificates are configured PSK (Pre Shared Keys) ciphers will be used. If the other side does not support TLS, or cleartext is configured the connection will be aborted. However, for downward compatibility with clients before Bareos-18.2 the daemons can omit transport encryption and cleartext will be sent.

:config:option:`dir/director/TlsRequire`\
   Require TLS connection, for downward compatibility. This is by default disabled. However, if :strong:`TlsRequire`\ =yes, clients with a version before Bareos-18.2 will be denied if configured to use cleartext.

:config:option:`dir/director/TlsCertificate`\
   The full path and filename of a PEM encoded TLS certificate. It can be used as either a client or server certificate. It is used because PEM files are base64 encoded and hence ASCII text based rather than binary. They may also contain encrypted information.

:config:option:`dir/director/TlsKey`\
   The full path and filename of a PEM encoded TLS private key. It must correspond to the certificate specified in the :strong:`TLS Certificate`\  configuration directive.

:config:option:`dir/director/TlsVerifyPeer`\
   Request and verify the peers certificate.

   In server context, unless the :strong:`TLS Allowed CN`\  configuration directive is specified, any client certificate signed by a known-CA will be accepted.

   In client context, the server certificate CommonName attribute is checked against the :strong:`Address`\  and :strong:`TLS Allowed CN`\  configuration directives.

:config:option:`dir/director/TlsAllowedCn`\
   Common name attribute of allowed peer certificates. If :strong:`TLS Verify Peer`\ =yes, all connection request certificates will be checked against this list.

   This directive may be specified more than once as all parameters will we concatenated.

:config:option:`dir/director/TlsCaCertificateFile`\
   The full path and filename specifying a PEM encoded TLS CA certificate(s). Multiple certificates are permitted in the file.

   In a client context, one of :strong:`TLS CA Certificate File`\  or :strong:`TLS CA Certificate Dir`\  is required.

   In a server context, it is only required if :strong:`TLS Verify Peer`\  is used.

:config:option:`dir/director/TlsCaCertificateDir`\
   Full path to TLS CA certificate directory. In the current implementation, certificates must be stored PEM encoded with OpenSSL-compatible hashes, which is the subject name’s hash and an extension of .0.

   In a client context, one of :strong:`TLS CA Certificate File`\  or :strong:`TLS CA Certificate Dir`\  is required.

   In a server context, it is only required if :strong:`TLS Verify Peer`\  is used.

:config:option:`dir/director/TlsDhFile`\
   Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications. DH key exchange adds an additional level of security because the key used for encryption/decryption by the server and the client is computed on each end and thus is never passed over the network if Diffie-Hellman key exchange is used. Even if DH key exchange is not used, the encryption/decryption key is always passed encrypted. This directive is only valid within a server context.

   To generate the parameter file, you may use openssl:

   .. code-block:: shell-session
      :caption: create DH key

      openssl dhparam -out dh1024.pem -5 1024

Getting TLS Certificates
------------------------

To get a trusted certificate (CA or Certificate Authority signed certificate), you will either need to purchase certificates signed by a commercial CA or become a CA yourself, and thus you can sign all your own certificates.

Bareos is known to work well with RSA certificates.

You can use programs like `xca <https://github.com/chris2511/xca/>`_ or TinyCA to easily manage your own CA with a Graphical User Interface.

Example TLS Configuration Files
-------------------------------

.. index:: Example; TLS Configuration Files
.. index:: TLS Configuration Files

Examples of the TLS portions of the configuration files are listed below.

Bareos Director
~~~~~~~~~~~~~~~

.. code-block:: bareosconfig
   :caption: bareos-dir.d/director/bareos-dir.conf

   Director {                            # define myself
       Name = bareos-dir
       ...
       TLS Enable = yes     #yes by default
       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       # This is a server certificate, used for incoming
       # (console) connections.
       TLS Certificate = /etc/bareos/tls/bareos-dir.example.com-cert.pem
       TLS Key = /etc/bareos/tls/bareos-dir.example.com-key.pem
       TLS Verify Peer = yes
       TLS Allowed CN = "bareos@backup1.example.com"
       TLS Allowed CN = "administrator@example.com"
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/storage/File.conf

   Storage {
       Name = File
       Address = bareos-sd1.example.com
       ...
       TLS Enable = yes     #yes by default
       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       # This is a client certificate, used by the director to
       # connect to the storage daemon
       TLS Certificate = /etc/bareos/tls/bareos-dir.example.com-cert.pem
       TLS Key = /etc/bareos/tls/bareos-dir.example.com-key.pem
       TLS Allowed CN = bareos-sd1.example.com
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/client/client1-fd.conf

   Client {
       Name = client1-fd
       Address = client1.example.com
       ...
       TLS Enable = yes     #yes by default
       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       TLS Certificate = "/etc/bareos/tls/bareos-dir.example.com-cert.pem"
       TLS Key = "/etc/bareos/tls/bareos-dir.example.com-key.pem"
       TLS Allowed CN = client1.example.com
   }

Bareos Storage Daemon
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bareosconfig
   :caption: bareos-sd.d/storage/bareos-sd1.conf

   Storage {
       Name = bareos-sd1
       ...
       # These TLS configuration options are used for incoming
       # file daemon connections. Director TLS settings are handled
       # in Director resources.
       TLS Enable = yes     #yes by default
       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       # This is a server certificate. It is used by connecting
       # file daemons to verify the authenticity of this storage daemon
       TLS Certificate = /etc/bareos/tls/bareos-sd1.example.com-cert.pem
       TLS Key = /etc/bareos/tls/bareos-sd1.example.com-key.pem
       # Peer verification must be disabled,
       # or all file daemon CNs must be listed in "TLS Allowed CN".
       # Peer validity is verified by the storage connection cookie
       # provided to the File Daemon by the Director.
       TLS Verify Peer = no
   }

.. code-block:: bareosconfig
   :caption: bareos-sd.d/director/bareos-dir.conf

   Director {
       Name = bareos-dir
       ...
       TLS Enable = yes     #yes by default
       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       # This is a server certificate. It is used by the connecting
       # director to verify the authenticity of this storage daemon
       TLS Certificate = /etc/bareos/tls/bareos-sd1.example.com-cert.pem
       TLS Key = /etc/bareos/tls/bareos-sd1.example.com-key.pem
       # Require the connecting director to provide a certificate
       # with the matching CN.
       TLS Verify Peer = yes
       TLS Allowed CN = "bareos-dir.example.com"
   }

Bareos File Daemon
~~~~~~~~~~~~~~~~~~

.. code-block:: bareosconfig
   :caption: bareos-fd.d/client/myself.conf

   Client {
       Name = client1-fd
       ...
       # you need these TLS entries so the SD and FD can
       # communicate
       TLS Enable = yes     #yes by default

       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       TLS Certificate = /etc/bareos/tls/client1.example.com-cert.pem
       TLS Key = /etc/bareos/tls/client1.example.com-key.pem

       TLS Allowed CN = bareos-sd1.example.com
   }

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
       Name = bareos-dir
       ...
       TLS Enable = yes     #yes by default
       TLS CA Certificate File = /etc/bareos/tls/ca.pem
       # This is a server certificate. It is used by connecting
       # directors to verify the authenticity of this file daemon
       TLS Certificate = /etc/bareos/tls/client11.example.com-cert.pem
       TLS Key = /etc/bareos/tls/client1.example.com-key.pem
       TLS Verify Peer = yes
       # Allow only the Director to connect
       TLS Allowed CN = "bareos-dir.example.com"
   }

.. _CompatibilityWithFileDaemonsBefore182Chapter:

Compatibility with |bareosFD|
-----------------------------

|bareosFD| connection handshake probing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As from Bareos 18.2 all components by default establish a secure connection with encryption first, followed by the proprietary Bareos protocol. This is accomplished using TLS-PSK. Older components of Bareos than version 18.2 start a connection with a cleartext handshake without encryption.

For downward compatibility Bareos Director Daemons and Bareos Storage Daemons are able to connect to Bareos File Daemons older than version 18.2. In this case Director and Storage switch to the old protocol.

There are two connection modes of a File Daemon, active and passive. In contrast to a connection from an active Bareos File Daemon, the protocol version of a passive File Daemon has to be probed by the Director Daemon initially when a job is initiated. This information is stored in the configuration and immediately submitted to the Storage Daemon when the job is started.

The following sequence is used to figure out the right protocol version and to submit this information to the attached Bareos Storage Daemon:

.. uml::
  :caption: Sequence diagram of a Bareos File Daemon connection

  hide footbox

  Actor user
  participant "ConfigurationParser\nclass" as Config << C,#EEEEEE >>
  participant "Some methods in\ndirectordaemon namespace" as Dir << N,#EEEEEE >>
  participant "Client methods in\n directordaemon namespace" as F << N,#EEEEEE >>
  participant "Client methods in\n filedaemon namespace" as FC << N,#EEEEEE >>

  == Config Initialisation ==

  user -> Config: reload config
  activate Config
  Config -> Config: ParseConfigFile()
  Config -> Dir: ConfigReadyCallback()
  activate Dir
  Dir -> Config: ResetAllClientConnectionHandshakeModes
  Dir <-- Config: All handshake modes reset to\nClientConnectionHandshakeMode::kUndefined
  Config <-- Dir: ConfigReadyCallback() done
  deactivate Dir
  user <-- Config: config reloaded

  ... try to connect to a client ...

  == Client Connection to old unknown client ==

  user -> Dir: run some client command
  activate Dir

  Dir -> F: ConnectToFileDaemon()
  activate F
  note right of F: Possible modes:\nkTlsFirst (try TLS immediately),\nkCleartextFirst (old cleartext handshake)
  F ->> FC: Try to connect to Filedaemon with immediate TLS\nconnection mode (kTlsFirst)
  F ->> FC: If immediate TLS fails try cleartext handshake mode\n(kCleartextFirst, this will happen with old clients before 18.2)
  F <- FC: Connection established
  Config <- F: Save successful mode into configuration of client
  Dir <-- F: ConnectToFileDaemon() done
  ... do something with client ...
  FC <--> F: close client connection
  Dir <-- F:
  user <-- Dir : finished some client command
  deactivate F
  deactivate Dir

  ... connect to the same filedaemon again ...

  == Client Connection to a known client ==

  user -> Dir: run some client command
  activate Dir
  Dir -> F: ConnectToFileDaemon()
  activate F
  Config -> F: Load successful mode from configuration of client
  F -> FC: Connect to Filedaemon with saved connection mode from config
  F <- FC: Connection established without waiting or probing
  Dir <-- F: ConnectToFileDaemon() done
  ... do something with client ...
  FC <--> F: close client connection
  Dir <-- F:
  user <-- Dir : finished some client command
  deactivate F
  deactivate Dir

  deactivate Config

|bareosFD| 18.2 with Bareos before 18.2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

|bareosFD| >= 18.2 can be used on a Bareos system before Bareos-18.2.

The *older* |bareosDir| and |bareosSD| connect to |bareosFD| using the cleartext Bareos handshake before they can switch to TLS. If you want transport encryption then only TLS with certificates can be used. TLS-PSK is not possible with |bareosDir| and |bareosSd| before Bareos-18.2.

However, it is also possible to disable transport encryption and use cleartext transport using the following configuration changes:

|bareosDir| configuration
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-dir.d/client/bareos-fd.conf`

  Client {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-dir.d/storage/bareos-sd.conf`

  Storage {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

|bareosSD| configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-sd.d/storage/bareos-sd.conf`

  Storage {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

|bareosFD| configuration before 18.2
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/client/bareos-fd.conf`

  Client {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`

  Director {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

|bareosFD| before 18.2 with Bareos 18.2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

|bareosFD| *before* 18.2 can be used on a Bareos system 18.2 *onwards*.

The newer |bareosDir| and |bareosSD| connect to |bareosFD| using the cleartext Bareos handshake before they switch to TLS. If you want transport encryption only TLS with certificates can be used, not PSK as it is possible with Bareos 18.2.

However, it is also possible to disable transport encryption and use cleartext transport using the following configuration changes:

|bareosFD| configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/client/bareos-fd.conf`

  Client {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`

  Director {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. _TransportEncryptionWebuiBareosDirChapter:

|bareosWebui|
-------------

Transport encryption between |bareosWebui| and a |bareosDir| can be configured on a per restricted named console basis.

TLS-PSK is not available between the Bareos WebUI and the Bareos Director, in the following you will set up TLS with certificates.


Please check the following configuration examples.  A complete table of the directives in the :file:`directors.ini` file see: :ref:`directors-ini-directives`

.. note::

   For |bareosWebui| the certificate file given by configuration parameter cert_file in directors.ini has to contain the certificate and the key in PEM encoding.


Configuration example
~~~~~~~~~~~~~~~~~~~~~

.. versionchanged:: 18.2
.. warning::

   In Bareos version 18.2, the global certificates configured in the director resource in the director configuration need to be used.
   Before, the certificates configured in the console resource of the director configuration were used.


.. code-block:: ini
   :caption: :file:`/etc/bareos-webui/directors.ini`

   ;------------------------------------------------------------------------------
   ; Section backup.example.com
   ;------------------------------------------------------------------------------
   [backup.example.com]
   enabled = "yes"
   address = "backup.example.com"
   port = 9101
   ;catalog = "MyCatalog"
   tls_verify_peer = false
   server_can_do_tls = true
   server_requires_tls = false
   client_can_do_tls = true
   client_requires_tls = true
   ca_file = "/etc/bareos-webui/tls/ca.crt"
   cert_file = "/etc/bareos-webui/tls/client.pem"
   ;cert_file_passphrase = ""
   ;allowed_cns = ""

.. code-block:: ini
   :caption: :file:`/etc/bareos/bareos-dir.d/director/bareos-dir.conf`

   Director {
      Name = bareos-dir
      QueryFile = "/usr/lib/bareos/scripts/query.sql"
      Maximum Concurrent Jobs = 10
      Password = "654321"
      Messages = Daemon
      Auditing = yes

      # Enable the Heartbeat if you experience connection losses
      # (eg. because of your router or firewall configuration).
      # Additionally the Heartbeat can be enabled in bareos-sd and bareos-fd.
      #
      # Heartbeat Interval = 1 min

      # remove comment from "Plugin Directory" to load plugins from specified directory.
      # if "Plugin Names" is defined, only the specified plugins will be loaded,
      # otherwise all director plugins (*-dir.so) from the "Plugin Directory".
      #
      # Plugin Directory = "/usr/lib64/bareos/plugins"
      # Plugin Names = ""

      TLS Enable = yes
      TLS Require = no
      TLS Verify Peer = no
      TLS CA Certificate File = /etc/bareos/tls/ca.crt
      TLS Certificate = /etc/bareos/tls/server.crt
      TLS Key = /etc/bareos/tls/server.pem
   }

.. _directors-ini-directives:

Overview of the settings in the |bareosWebui| :file:`directors.ini` file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. csv-table:: TLS settings for |bareosWebui|
   :header-rows: 1

   Directive            , Type    ,  Default value , Remark   , Description
   tls_verify_peer      , boolean ,  false         , Optional , TLS verif peer
   server_can_do_tls    , boolean ,  false         , Required , Server (|dir|) can do TLS
   server_requires_tls  , boolean ,  false         , Required , Server (|dir|) requires TLS
   client_can_do_tls    , boolean ,  false         , Required , Client can do TLS
   client_requires_tls  , boolean ,  false         , Required , Client requires TLS
   ca_file              , string  ,                , Required , Certificate authority file
   cert_file            , string  ,                , Required , Path to the certificate file which needs to contain the client certificate and the key in PEM encoding
   cert_file_passphrase , string  ,                , Optional , Passphrase to unlock the certificate file given by cert_file
   allowed_cns          , string  ,                , Optional , Allowed common names


.. _TLSConfigurationReferenceChapter:

TLS Configuration Reference
---------------------------

To be able to communicate via TLS, TLS needs to be configured on both sides. In Bareos certain directives are used to set up TLS.

The following table explains the location of the relevant TLS configuration directives for all possible Bareos TCP connections. Each resource is referred to as <component>-<resource> to identify the exact configuration location. Refer to chapter :ref:`ConfigureChapter` for more details about configuration.

In Bareos Version 18.2 the relevant resources for some connections had to be changed. Affected directives are marked with the applicable version and the respective resource is written in bold letters.

*Remark: TLS-PSK is not available on Bareos components before Version 18.2.*

 .. csv-table:: TLS Configuration Reference
    :file: bareos_connection_modes_overview_1.csv
    :widths: 20 35 10 35

.. rubric:: Footnotes
.. [#number] The connection number references this table: :ref:`LegendForFullConnectionOverviewReference`
.. [#identity] From Version 18.2 onwards this is identical to the TLS-PSK Identitiy
.. [#psk] From Version 18.2 onwards this is identical to the TLS-PSK Pre-Shared Key
.. [#user_agent] The name of the default console is predefined and cannot be changed
.. [#cert] Certificate directives are: TlsVerifyPeer, TlsCaCertificateFile, TlsCaCertificateDir, TlsCertificateRevocationList, TlsCertificate, TlsKey, TlsAllowedCn


.. _TLSKernelTLS:

Enabling |ktls|
---------------

|ktls| is a feature of some operating systems in which the application may pass data directly to the kernel,
so that it performs the necessary encryption in kernel space.

.. warning::
   If |ktls| is used then the data is written **unencrypted** to the socket.


This allows the kernel to offload this work to dedicated hardware to speed up the connection.

You may set the following options to **Yes** so that the respective daemon announces to the
operating system that it wants to make use of |ktls|.

:config:option:`dir/director/EnableKtls`
:config:option:`fd/client/EnableKtls`
:config:option:`sd/storage/EnableKtls`

.. note::
   In the end the SSL implementation and the operating
   system itself will decide whether |ktls| will be used or not.
   Bareos cannot influence this decision directly.

To make it more likely that |ktls| is used, you may consult your operating system vendors manual to
see which TLS ciphers it supports and configure bareos to use those.

.. warning::
   Some SSL implementations do not correctly report whether |ktls| is used or not.
   In that case Bareos may send data completely unencrypted!  Make sure to study
   the respective manuals.

If `EnableKtls` was set to **Yes** then Bareos will emit a debug message saying whether
it thinks that |ktls| is enabled or not.

.. note::
   On some operating systems you have to prepare |ktls| before bareos can take
   advantage of it.  For example on Linux you have to load the **tls** kernel
   module.

.. code-block:: shell-session
   :caption: connecting a console with |ktls| enabled
   :emphasize-lines: 18,19

   host:~$ bconsole -d150
   [...]
   bconsole (100): lib/tls_openssl_private.cc:58-0 Construct TlsOpenSslPrivate
   bconsole (100): lib/tls_openssl_private.cc:614-0 Set tcp filedescriptor: <3>
   bconsole (100): lib/tls_openssl_private.cc:632-0 Set protocol:	<>
   bconsole (100): lib/tls_openssl_private.cc:554-0 Set ca_certfile:	<>
   bconsole (100): lib/tls_openssl_private.cc:560-0 Set ca_certdir:	<>
   bconsole (100): lib/tls_openssl_private.cc:566-0 Set crlfile_:	<>
   bconsole (100): lib/tls_openssl_private.cc:572-0 Set certfile_:	<>
   bconsole (100): lib/tls_openssl_private.cc:578-0 Set keyfile_:	<>
   bconsole (100): lib/tls_openssl_private.cc:596-0 Set dhfile_:	<>
   bconsole (100): lib/tls_openssl_private.cc:620-0 Set cipherlist:	<>
   bconsole (100): lib/tls_openssl_private.cc:626-0 Set ciphersuites:	<>
   bconsole (100): lib/tls_openssl_private.cc:602-0 Set Verify Peer:	<false>
   bconsole (100): lib/tls_openssl_private.cc:608-0 Set ktls:	<true>
   bconsole (50): lib/tls_openssl.cc:63-0 Preparing TLS_PSK CLIENT context for identity R_CONSOLE *UserAgent*
   bconsole (100): lib/tls_openssl_private.cc:541-0 psk_client_cb. identity: R_CONSOLE *UserAgent*.
   bconsole (150): lib/tls_openssl_private.cc:436-0 Ktls used for Recv: no
   bconsole (150): lib/tls_openssl_private.cc:437-0 Ktls used for Send: yes
   [...]

.. _TLSRestrictingProtocolCipherChapter:

TLS Restricting Protocol and Cipher
-----------------------------------

.. index:: Example; TLS howto limit cipher
.. index:: TLS Limit protocol, TLS Limit cipher

With TLS/PSK activated by default in Bareos since version 18, it is sometimes mandatory to achieve better performance and increase the throughput of backups and restores.
To do so, you need to fine-tune the configuration, selecting wisely the protocol and ciphers used. Syntax and parameter usage is far from evident.
To do that, we only need to change two parameters into bareos configuration files, but their syntax and the location are a bit complex.

This chapter will show you how to do that.

.. note::

   Since TLS protocols below 1.2 are considered weak, we will concentrate efforts on restricting cipher usage to protocol version 1.2 and above.


Determine available ciphers for TLSv1.2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following command :command:`openssl` helps to determine which ciphers are available for protocol 1.2 with the needed PSK extensions.


.. code-block:: shell-session
   :caption: openssl list available ciphers for tls v1.2 with psk

   openssl ciphers -tls1_2 -psk -s


Adding `-v` option will give you an output list in column mode.

Example of column output excluding SSLv3 and SHA1.

.. code-block:: shell-session
   :caption: openssl verbose cipher list for tls v1.2 with psk filtering SSLv2 and SHA1

   openssl ciphers -v -tls1_2 -psk -s | grep -v SSLv3 | grep -v Mac=SHA1


   Example output list of ciphers:

      ECDHE-ECDSA-AES256-GCM-SHA384 TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AESGCM(256) Mac=AEAD
      ECDHE-RSA-AES256-GCM-SHA384 TLSv1.2 Kx=ECDH     Au=RSA  Enc=AESGCM(256) Mac=AEAD
      ECDHE-ECDSA-CHACHA20-POLY1305 TLSv1.2 Kx=ECDH     Au=ECDSA Enc=CHACHA20/POLY1305(256) Mac=AEAD
      ECDHE-RSA-CHACHA20-POLY1305 TLSv1.2 Kx=ECDH     Au=RSA  Enc=CHACHA20/POLY1305(256) Mac=AEAD
      ECDHE-ECDSA-AES256-CCM  TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AESCCM(256) Mac=AEAD
      ECDHE-ECDSA-AES128-GCM-SHA256 TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AESGCM(128) Mac=AEAD
      ECDHE-RSA-AES128-GCM-SHA256 TLSv1.2 Kx=ECDH     Au=RSA  Enc=AESGCM(128) Mac=AEAD
      ECDHE-ECDSA-AES128-CCM  TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AESCCM(128) Mac=AEAD
      ECDHE-ECDSA-AES128-SHA256 TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AES(128)  Mac=SHA256
      ECDHE-RSA-AES128-SHA256 TLSv1.2 Kx=ECDH     Au=RSA  Enc=AES(128)  Mac=SHA256
      AES256-GCM-SHA384       TLSv1.2 Kx=RSA      Au=RSA  Enc=AESGCM(256) Mac=AEAD
      AES256-CCM              TLSv1.2 Kx=RSA      Au=RSA  Enc=AESCCM(256) Mac=AEAD
      AES128-GCM-SHA256       TLSv1.2 Kx=RSA      Au=RSA  Enc=AESGCM(128) Mac=AEAD
      AES128-CCM              TLSv1.2 Kx=RSA      Au=RSA  Enc=AESCCM(128) Mac=AEAD
      AES256-SHA256           TLSv1.2 Kx=RSA      Au=RSA  Enc=AES(256)  Mac=SHA256
      AES128-SHA256           TLSv1.2 Kx=RSA      Au=RSA  Enc=AES(128)  Mac=SHA256
      DHE-RSA-AES256-GCM-SHA384 TLSv1.2 Kx=DH       Au=RSA  Enc=AESGCM(256) Mac=AEAD
      DHE-RSA-CHACHA20-POLY1305 TLSv1.2 Kx=DH       Au=RSA  Enc=CHACHA20/POLY1305(256) Mac=AEAD
      DHE-RSA-AES256-CCM      TLSv1.2 Kx=DH       Au=RSA  Enc=AESCCM(256) Mac=AEAD
      DHE-RSA-AES128-GCM-SHA256 TLSv1.2 Kx=DH       Au=RSA  Enc=AESGCM(128) Mac=AEAD
      DHE-RSA-AES128-CCM      TLSv1.2 Kx=DH       Au=RSA  Enc=AESCCM(128) Mac=AEAD
      DHE-RSA-AES256-SHA256   TLSv1.2 Kx=DH       Au=RSA  Enc=AES(256)  Mac=SHA256
      DHE-RSA-AES128-SHA256   TLSv1.2 Kx=DH       Au=RSA  Enc=AES(128)  Mac=SHA256
      PSK-AES256-GCM-SHA384   TLSv1.2 Kx=PSK      Au=PSK  Enc=AESGCM(256) Mac=AEAD
      PSK-CHACHA20-POLY1305   TLSv1.2 Kx=PSK      Au=PSK  Enc=CHACHA20/POLY1305(256) Mac=AEAD
      PSK-AES256-CCM          TLSv1.2 Kx=PSK      Au=PSK  Enc=AESCCM(256) Mac=AEAD
      PSK-AES128-GCM-SHA256   TLSv1.2 Kx=PSK      Au=PSK  Enc=AESGCM(128) Mac=AEAD
      PSK-AES128-CCM          TLSv1.2 Kx=PSK      Au=PSK  Enc=AESCCM(128) Mac=AEAD
      PSK-AES128-CBC-SHA256   TLSv1 Kx=PSK      Au=PSK  Enc=AES(128)  Mac=SHA256
      DHE-PSK-AES256-GCM-SHA384 TLSv1.2 Kx=DHEPSK   Au=PSK  Enc=AESGCM(256) Mac=AEAD
      DHE-PSK-CHACHA20-POLY1305 TLSv1.2 Kx=DHEPSK   Au=PSK  Enc=CHACHA20/POLY1305(256) Mac=AEAD
      DHE-PSK-AES256-CCM      TLSv1.2 Kx=DHEPSK   Au=PSK  Enc=AESCCM(256) Mac=AEAD
      DHE-PSK-AES128-GCM-SHA256 TLSv1.2 Kx=DHEPSK   Au=PSK  Enc=AESGCM(128) Mac=AEAD
      DHE-PSK-AES128-CCM      TLSv1.2 Kx=DHEPSK   Au=PSK  Enc=AESCCM(128) Mac=AEAD
      DHE-PSK-AES128-CBC-SHA256 TLSv1 Kx=DHEPSK   Au=PSK  Enc=AES(128)  Mac=SHA256
      ECDHE-PSK-CHACHA20-POLY1305 TLSv1.2 Kx=ECDHEPSK Au=PSK  Enc=CHACHA20/POLY1305(256) Mac=AEAD
      ECDHE-PSK-AES128-CBC-SHA256 TLSv1 Kx=ECDHEPSK Au=PSK  Enc=AES(128)  Mac=SHA256
      RSA-PSK-AES256-GCM-SHA384 TLSv1.2 Kx=RSAPSK   Au=RSA  Enc=AESGCM(256) Mac=AEAD
      RSA-PSK-CHACHA20-POLY1305 TLSv1.2 Kx=RSAPSK   Au=RSA  Enc=CHACHA20/POLY1305(256) Mac=AEAD
      RSA-PSK-AES128-GCM-SHA256 TLSv1.2 Kx=RSAPSK   Au=RSA  Enc=AESGCM(128) Mac=AEAD
      RSA-PSK-AES128-CBC-SHA256 TLSv1 Kx=RSAPSK   Au=RSA  Enc=AES(128)  Mac=SHA256



From that list we propose to use the following ciphers list:

.. note::

   ECDHE-RSA-AES128-GCM-SHA256
   ECDHE-RSA-AES256-GCM-SHA384
   PSK-AES128-GCM-SHA256
   AES128-GCM-SHA256
   PSK-AES256-GCM-SHA384
   AES256-GCM-SHA384



Order is done by level of "most secure" label done by [ciphersuite.info](https://ciphersuite.info/) website, then by ascending strength of digest to minimize cpu impact. Note the importance to have some cipher with **PSK** in its name to support the TLS/PSK mechanism.


Just be sure they are present on all hosts you want to use with Bareos.

Determine available ciphers for TLSv1.3
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Similar to TLSv1.2 we can list the supported ciphers with

.. code-block:: shell-session
   :caption: openssl list available ciphers for tls v1.3

   openssl ciphers -tls1_3 -psk -s

Currently only SHA256 ciphers are supported by bareos.

.. code-block:: shell-session
   :caption: openssl verbose cipher list for tls v1.3 with psk filtering for SHA256

   openssl ciphers -v -tls1_3 -psk -s | grep SHA256


   Example output list of ciphers:

      TLS_CHACHA20_POLY1305_SHA256   TLSv1.3 Kx=any      Au=any   Enc=CHACHA20/POLY1305(256) Mac=AEAD
      TLS_AES_128_GCM_SHA256         TLSv1.3 Kx=any      Au=any   Enc=AESGCM(128)            Mac=AEAD
      TLS_AES_128_CCM_SHA256         TLSv1.3 Kx=any      Au=any   Enc=AESCCM(128)            Mac=AEAD

Since all available TLSv1.3 ciphers are fairly strong, we recommend to not set the ciphersuites manually unless
needed; for example to enable |ktls|.

Resources parameters to configure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We will modify the following options

On |dir|

:config:option:`dir/director/TlsProtocol`

:config:option:`dir/director/TlsCipherList`

:config:option:`dir/console/TlsProtocol`

:config:option:`dir/console/TlsCipherList`

:config:option:`dir/client/TlsProtocol`

:config:option:`dir/client/TlsCipherList`

:config:option:`dir/storage/TlsProtocol`

:config:option:`dir/storage/TlsCipherList`


On |fd|

:config:option:`fd/client/TlsProtocol`

:config:option:`fd/client/TlsCipherList`

:config:option:`fd/director/TlsProtocol`

:config:option:`fd/director/TlsCipherList`


On |sd|

:config:option:`sd/storage/TlsProtocol`

:config:option:`sd/storage/TlsCipherList`

:config:option:`sd/director/TlsProtocol`

:config:option:`sd/director/TlsCipherList`


For bconsole

:config:option:`console/console/TlsProtocol`

:config:option:`console/console/TlsCipherList`



In the following example, we will remove all protocols below 1.2 and 1.3, and activate specifically 1.2 if it is not by default.

The order of the list of ciphers that should normally be hardware accelerated and usable by kernel |ktls|, is important.

Both values need to be set as strings enclosed by double quotes.


.. code-block:: bareosconfig
   :caption: TLS Protocol and TLS Cipher List Restricted syntax values

   ...
   TLS Protocol = "-TLSv1,-TLSv1.1,-TLSv1.3,TLSv1.2"
   TLS Cipher List = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:AES128-GCM-SHA256:PSK-AES128-GCM-SHA256:AES256-GCM-SHA384:PSK-AES256-GCM-SHA384"
   ...

Those parameters have to be included in each resource located at (filenames in default installation).

.. code-block:: cfg
   :caption: List of configuration files where to apply changes

    bareos-dir.d/director/bareos-dir.conf
    bareos-dir.d/client/bareos-fd.conf
    bareos-dir.d/storage/File.conf

    bareos-fd.d/client/myself.conf
    bareos-fd.d/director/bareos-dir.conf

    bareos-sd.d/storage/bareos-sd.conf
    bareos-sd.d/director/bareos-dir.conf

    bconsole.conf


How to test protocol and ciphers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Testing the protocol in use and supported ciphers can be done with the :command:`nmap` tool.

.. code-block:: shell-session
   :caption: testing protocol and cipher with nmap

   # director
   nmap --script ssl-enum-ciphers -p 9101 -n localhost

   # fd
   nmap --script ssl-enum-ciphers -p 9102 -n localhost

   # sd
   nmap --script ssl-enum-ciphers -p 9103 -n localhost


If your daemons are using exclusively ipv6 you have to use the :command:`nmap -6` option.


.. code-block:: shell
   :caption: Example of default ciphers for bareos-fd before restricted configuration

   nmap --script ssl-enum-ciphers -p 9102 -n localhost

    Starting Nmap 7.93 ( https://nmap.org ) at 2022-11-22 14:25 CET
    Nmap scan report for localhost (::1)
    Host is up (0.000095s latency).
    Other addresses for localhost (not scanned): 127.0.0.1

    PORT     STATE SERVICE
    9102/tcp open  jetdirect
    | ssl-enum-ciphers:
    |   TLSv1.0:
    |     ciphers:
    |       TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 (secp256r1) - A
    |       TLS_PSK_WITH_AES_128_CBC_SHA - unknown
    |       TLS_PSK_WITH_AES_128_CBC_SHA256 - unknown
    |       TLS_PSK_WITH_AES_256_CBC_SHA - unknown
    |       TLS_PSK_WITH_AES_256_CBC_SHA384 - unknown
    |       TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256 - unknown
    |       TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384 - unknown
    |     compressors:
    |       NULL
    |     cipher preference: client
    |   TLSv1.1:
    |     ciphers:
    |       TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 (secp256r1) - A
    |       TLS_PSK_WITH_AES_128_CBC_SHA - unknown
    |       TLS_PSK_WITH_AES_128_CBC_SHA256 - unknown
    |       TLS_PSK_WITH_AES_256_CBC_SHA - unknown
    |       TLS_PSK_WITH_AES_256_CBC_SHA384 - unknown
    |       TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256 - unknown
    |       TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384 - unknown
    |     compressors:
    |       NULL
    |     cipher preference: client
    |   TLSv1.2:
    |     ciphers:
    |       TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 (secp256r1) - A
    |       TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256 (secp256r1) - A
    |       TLS_PSK_WITH_AES_128_CBC_SHA - unknown
    |       TLS_PSK_WITH_AES_128_CBC_SHA256 - unknown
    |       TLS_PSK_WITH_AES_128_CCM - unknown
    |       TLS_PSK_WITH_AES_128_CCM_8 - unknown
    |       TLS_PSK_WITH_AES_128_GCM_SHA256 - unknown
    |       TLS_PSK_WITH_AES_256_CBC_SHA - unknown
    |       TLS_PSK_WITH_AES_256_CBC_SHA384 - unknown
    |       TLS_PSK_WITH_AES_256_CCM - unknown
    |       TLS_PSK_WITH_AES_256_CCM_8 - unknown
    |       TLS_PSK_WITH_AES_256_GCM_SHA384 - unknown
    |       TLS_PSK_WITH_ARIA_128_GCM_SHA256 - unknown
    |       TLS_PSK_WITH_ARIA_256_GCM_SHA384 - unknown
    |       TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256 - unknown
    |       TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384 - unknown
    |       TLS_PSK_WITH_CHACHA20_POLY1305_SHA256 - unknown
    |     compressors:
    |       NULL
    |     cipher preference: client
    |_  least strength: unknown

    Nmap done: 1 IP address (1 host up) scanned in 0.23 seconds

To be compared to the following output when restricted protocol and cipher are in place.

.. code-block:: shell
   :caption: Example of TLS restricted protocol and ciphers for bareos-fd only TLS-PSK

   nmap --script ssl-enum-ciphers -p 9102 -n localhost

    Starting Nmap 7.93 ( https://nmap.org ) at 2022-11-22 14:50 CET
    Nmap scan report for localhost (127.0.0.1)
    Host is up (0.000057s latency).
    Other addresses for localhost (not scanned): ::1

    PORT     STATE SERVICE
    9101/tcp open  jetdirect
    | ssl-enum-ciphers:
    |   TLSv1.2:
    |     ciphers:
    |       TLS_PSK_WITH_AES_128_GCM_SHA256 - unknown
    |       TLS_PSK_WITH_AES_256_GCM_SHA384 - unknown
    |     compressors:
    |       NULL
    |     cipher preference: client
    |     warnings:
    |       Forward Secrecy not supported by any cipher
    |_  least strength: unknown

    Nmap done: 1 IP address (1 host up) scanned in 0.28 seconds

With the restricted configuration in place, the signature in :command:`bconsole` will reflect the changes.


.. code-block:: bconsole
   :caption: director connection with restricted TLS protocol and ciphers

   bconsole

    Connecting to Director localhost:9101
      Encryption: PSK-AES128-GCM-SHA256 TLSv1.2
    1000 OK: bareos-dir Version: 21.1.5 (09 November 2022)

Conclusion
~~~~~~~~~~

You are now able to configure TLS Protocol and cipher list to match your needs. Once this is in place we highly recommend comparing with real jobs the gain or loss of performance.
