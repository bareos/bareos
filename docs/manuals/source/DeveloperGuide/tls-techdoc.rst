.. _section-TlsTechnicalDocumentation:

TLS
===

Introduction
------------

Bareos uses TLS to ensure data encryption for all TCP connections between Bareos components. Implemented and working is only OpenSSL.

Starting from Bareos 18.2 every BareosSocket TCP connection has its own SSL_CTX and SSL object. In other words, every time when establishing a new connection a new SSL_CTX object is initialized to create a new SSL object.

For a first overview the following diagram shows the connection sequence of a Bareos Console to a Bareos Director.

.. uml::
  :caption: Initiation of a TLS connection

  hide footbox

  actor user
  participant "B-Console" as console
  participant "Director" as director

  user -> console: start bconsole
  console <-> director: initiate TCP connection
  console <-> director: initiate a secure TLS connection (cert/psk)
  console <-> director: secondary CRAM/MD5 authentication

  ... do something with console ...

  user -> console: quit session ('q'; Ctrl + D)
  console <-> director: Shutdown TLS
  console <-> director: Finish TCP connection


TLS Handshake before Bareos 18.2
--------------------------------

.. uml::
  :caption: Initiation of a TLS connection prior to Bareos 18.2

  actor "Console\nWebUI" as W
  participant "director\ndaemon" as D

  W <-> D: Open TCP connection

  W -> D: "Hello [*UserAgent*|name] calling"
  note right of D: *UserAgent*: root console\nname: named console
  autonumber 1 "[cram 0:]"
  W <- D: "auth cram-md5[c] <password-md5> ssl=<0,1,2>"
  note right of D: 0:=cleartext\n1:=TLS-Cert possible\n2:=TLS-Cert required
  W -> D: "<password-md5>"
  W <- D: "1000 OK auth"

  W -> D: "auth cram-md5[c] <password-md5> ssl=<0,1,2>"
  W <- D: "<password-md5>"
  W -> D: "1000 OK auth"

  autonumber stop

  W <-> D: [ssl=1,2: TLS Cert Handshake]
  W <- D: 1000 OK: <director-name> Version: <version> (<date>)

  ... run some console commands ...

  W <-> D: [ssl=1,2: Close TLS connection]
  W <-> D: Close TCP connection


Unified Authentication API
---------------------------
Since Bareos 26, the connection and authentication code that used to be
duplicated in every daemon (``dird``, ``filed``, ``stored``,
``bconsole``/``bat``, ``bareos-tray-monitor``) has been unified into a
generic implementation in :file:`lib/bsock.h`, :file:`lib/bsock.cc`,
:file:`lib/hello.h` and :file:`lib/hello.cc`. Every component-pair
specific detail (the exact wording of the *Hello* message, which
resource is used to authenticate against, ...) is expressed via small
template/interface implementations instead of duplicated connection
logic.

Outbound connections: ``BareosConnect()``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
A component that wants to *connect to* another component (e.g. the
Director connecting to the Storage daemon) uses the templated
``BareosConnect<my_type, target_type>()`` overload:

.. code-block:: cpp

  template <global_resource::Type type, global_resource::Type target_type>
  bool BareosConnect(JobControlRecord* jcr,
                     BareosSocket* socket,
                     std::string_view name,
                     const TlsResource* res,
                     bool cleartext_authentication = false);

This overload looks up ``hello_formatter<type, target_type>`` to build
the correct *Hello* message and to determine which
``global_resource::Type`` is used for authentication
(``hello_formatter<...>::auth_type``), then delegates to the
non-templated ``BareosConnect()`` overload, using
``Md5Authenticator`` (CRAM-MD5) for the actual challenge/response.

Inbound connections: ``BareosAccept()``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
A component that *accepts* connections (all daemons) calls
``BareosAccept()`` once per incoming socket:

.. code-block:: cpp

  std::optional<ParsedHello> BareosAccept(BareosSocket* socket,
                                          global_resource::Type type,
                                          const TlsResource* initial_tls,
                                          TlsConfigProvider* provider,
                                          Authenticator* auth);

``BareosAccept()`` performs the initial TLS-PSK/cert handshake (or
accepts a cleartext connection for old peers), receives and parses the
*Hello* message via ``parse_hello()``, uses ``provider`` to map the
identity found in the *Hello* message to a ``TlsResource`` (e.g. the
matching ``DirectorResource`` or a per-job resource), and finally calls
``auth->authenticate_inbound()`` to complete the secondary CRAM-MD5
authentication. ``provider`` may be ``nullptr`` if no TLS-PSK lookup is
required (e.g. cleartext-only setups); ``BareosAccept()`` handles this
case by treating the identity lookup as failed rather than
dereferencing a null pointer.

On success, ``BareosAccept()`` returns a ``ParsedHello`` describing who
connected (``from``), which resource type was used to authenticate
(``type``), the peer's name/version, and any component-pair specific
extra fields (e.g. ``fd_protocol_version`` for File daemon connections,
``old_console`` for pre-18.2 consoles).

Extending the ``Authenticator`` interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Both ``BareosConnect()`` and ``BareosAccept()`` take an
``Authenticator*``:

.. code-block:: cpp

  struct Authenticator {
    struct OutboundArgs {
      JobControlRecord* jcr;
      BareosSocket* socket;
      const TlsResource* target;
    };
    struct InboundArgs {
      BareosSocket* socket;
      const TlsResource* target;
    };

    virtual bool authenticate_outbound(OutboundArgs args) = 0;
    virtual bool authenticate_inbound(InboundArgs args) = 0;
    virtual ~Authenticator() = default;
  };

The only implementation currently shipped is ``Md5Authenticator``,
which performs the classic CRAM-MD5 challenge/response using
``TlsResource::password_``. To support a different secondary
authentication scheme (e.g. for a future protocol version), implement
a new ``Authenticator`` and pass it explicitly to ``BareosConnect()``/
``BareosAccept()`` instead of relying on the ``Md5Authenticator``
convenience overloads.

Adding a new component-pair
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To support a new pair of connecting components (or a new *Hello*
message variant for an existing pair):

#. Add a ``hello_formatter<my_type, target_type>`` specialization in
   :file:`lib/hello.h`/:file:`lib/hello.cc` that defines
   ``auth_type`` (the ``global_resource::Type`` used to look up the
   peer's ``TlsResource``) and a ``format()`` function producing the
   *Hello* message string sent by the connecting side.
#. Add a matching branch to ``parse_hello()`` in :file:`lib/hello.cc`
   for the accepting side's ``my_type``, so the new *Hello* message is
   recognized and turned into a ``ParsedHello``.
#. Provide a ``TlsConfigProvider::get()`` implementation (or extend an
   existing one, see e.g. ``filedaemon::Auth`` /
   ``storagedaemon::Auth``) that maps the parsed identity to the
   correct ``TlsResource`` for the new ``auth_type``.

Each daemon's own ``Auth::get()`` implementation is the place to look
for resource lookup failures; make sure any newly added rejection
branch there also emits a visible ``Emsg()``/``Jmsg()`` (not just a
``Dmsg()``) so that connection failures remain diagnosable without
enabling debug output.


TLS Configuration Implementation
--------------------------------
TLS configuration directives will be transferred from the configuration into dedicated classes as follows.

.. uml::
  :caption: Bareos TLS config internal class relations

  package "Bareos Config as defined in lib/parse_conf.h" #EEEEEE {
  class TLS_COMMON_CONFIG << (B, #FF7700) >> {
    + CFG_TYPE_BOOL TlsAuthenticate <tls_cert.authenticate>
    + CFG_TYPE_BOOL TlsEnable <tls_cert.enable>
    + CFG_TYPE_BOOL TlsRequire <tls_cert.require>
    + CFG_TYPE_STR TlsCipherList <tls_cert.cipherlist>
    + CFG_TYPE_STDSTRDIR TlsDhFile <tls_cert.dhfile>
  }

  class TLS_CERT_CONFIG << (B, #FF7700) >> {
    + CFG_TYPE_BOOL VerifyPeer <tls_cert.VerifyPeer>
    + CFG_TYPE_STDSTRDIR TlsCaCertificateFilec <tls_cert.CaCertfile>
    + CFG_TYPE_STDSTRDIR TlsCaCertificateDir <tls_cert.CaCertfile>
    + CFG_TYPE_STDSTRDIR TlsCertificateRevocationList <tls_cert.crlfile>
    + CFG_TYPE_STDSTRDIR TlsCertificate <tls_cert.certfile>
    + CFG_TYPE_STDSTRDIR TlsKey <tls_cert.keyfile>
    + CFG_TYPE_ALIST_STR TlsAllowedCn <tls_cert.AllowedCns>
  }
  }

  TlsResource *- TlsConfigCert: > initializes

  class TlsResource {
    + s_password password_
    + TlsConfigCert tls_cert_
    + std::string *cipherlist_
    + bool authenticate_
    + bool tls_enable_;
    + bool tls_require_;
  }

  class TlsConfigCert {
     + bool verify_peer_
     + std::string *ca_certfile_
     + std::string *ca_certdir_
     + std::string *crlfile_
     + std::string *certfile_
     + std::string *keyfile_
     + std::string *dhfile_
     + alist *allowed_certificate_common_names_;

     + std::string *pem_message_;
  }

  TLS_COMMON_CONFIG --> TlsResource : initializes\n during config load
  TLS_CERT_CONFIG --> TlsResource : initializes\n during config load


TLS API Implementation
----------------------
The following diagram shows the interface of the *TlsOpenSsl* class and its aggregation in the *BareosSocket* class. During initialization and handshake of a TLS connection *tls_conn_init* will be used and *tls_conn* is invalid. As soon as the TLS connection is established the pointer from *tls_conn_init* will be moved to *tls_conn* and *tls_conn_init* will become invalid.

.. uml::
  :caption: TLS OpenSSL Class overview (simplified)

  class BareosSocket {
    + std::shared_ptr<Tls> tls_conn
    + std::unique_ptr<Tls> tls_conn_init (see text)
  }

  abstract class Tls {
    + new_tls_context()
    + FreeTlsContext()
    + TlsPostconnectVerifyHost()
    + TlsPostconnectVerifyCn()
    + TlsBsockAccept()
    + TlsBsockWriten()
    + TlsBsockReadn()
    + TlsBsockConnect()
    + TlsBsockShutdown()
    + FreeTlsConnection()
  }

  class "TlsOpenSsl" as OpenSsl {
    - const char *default_ciphers
    - SSL_CTX *openssl_
    - SSL *openssl_
    - CRYPTO_PEM_PASSWD_CB *pem_callback
    - const void *pem_userdata
    + new_tls_psk_client_context()
    + new_tls_psk_server_context()
    + TlsCipherGetName()
    + TlsLogConninfo()
    + TlsPolicyHandshake()
  }

  OpenSsl --|> Tls

  BareosSocket o- Tls : initialize >
