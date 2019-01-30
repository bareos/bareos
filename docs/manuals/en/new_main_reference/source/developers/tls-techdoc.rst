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


TLS Configuration Implementation
--------------------------------
TLS configuration directives will be transfered from the configuration into dedicated classes as follows.

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
The following diagramm shows the interface of the *TlsOpenSsl* class and its aggregation in the *BareosSocket* class. During initialization and handshake of a TLS connection *tls_conn_init* will be used and *tls_conn* is invalid. As soon as the TLS connection is established the pointer from *tls_conn_init* will be moved to *tls_conn* and *tls_conn_init* will become invalid.

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

