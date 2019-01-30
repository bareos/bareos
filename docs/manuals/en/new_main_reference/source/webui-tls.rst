
Transport encryption between |bareosWebui| and |bareosDir|
==========================================================

Transport encryption between |bareosWebui| and a |bareosDir| can be configured on a per restricted named console basis.


Please check the following configuration examples.  A complete table of the directives in the :file:`directors.ini` file see: :ref:`directors-ini-directives`

.. note::

   For |bareosWebui| the certificate file given by configuration parameter cert_file in directors.ini has to contain the certificate and the key in PEM encoding.


Configuration example for Bareos 17.2
-------------------------------------

.. code-block:: ini
   :caption: :file:`/etc/bareos-webui/directors.ini`

   ;------------------------------------------------------------------------------
   ; Section backup.example.com
   ;------------------------------------------------------------------------------
   [backup.example.com]
   enabled = "yes"
   diraddress = "backup.example.com"
   dirport = 9101
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
   :caption: :file:`/etc/bareos/bareos-dir.d/console/admin.conf`

   #
   # Restricted console used by bareos-webui
   #
   Console {
     Name = admin
     Password = "123456"
     Profile = "webui-admin"
     TLS Enable = yes
     TLS Require = no
     TLS Verify Peer = no
     TLS CA Certificate File = /etc/bareos/tls/ca.crt
     TLS Certificate = /etc/bareos/tls/server.crt
     TLS Key = /etc/bareos/tls/server.pem
   }



Configuration example for Bareos 18.2
---------------------

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
   diraddress = "backup.example.com"
   dirport = 9101
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

      # remove comment in next line to load dynamic backends from specified directory
      # Backend Directory = /usr/lib64/bareos/backends

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

Overview of the settings in the |bareosWebui| :file:`directors.ini` file.
-----------------------------------------------------------

.. csv-table:: TLS settings for |bareosWebui|
   :header-rows: 1

   Directive            , Type    ,  Default value , Remark   , Description
   tls_verify_peer      , boolean ,  false         , Optional , TLS verif peer
   server_can_do_tls    , boolean ,  false         , Required , Server can do TLS
   server_requires_tls  , boolean ,  false         , Required , Server requires TLS
   client_can_do_tls    , boolean ,  false         , Required , Client can do TLS
   client_requires_tls  , boolean ,  false         , Required , Client requires TLS
   ca_file              , string  ,                , Required , Certificate authority file
   cert_file            , string  ,                , Required , Path to the certificate file which needs to contain the client certificate and the key in PEM encoding
   cert_file_passphrase , string  ,                , Optional , Passphrase to unlock the certificate file given by cert_file
   allowed_cns          , string  ,                , Optional , Allowed common names

