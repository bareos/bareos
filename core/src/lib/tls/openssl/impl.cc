/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * TLS support functions when using the OpenSSL backend.
 *
 * Author: Landon Fuller <landonf@threerings.net>
 */

#include "include/bareos.h"
#include "lib/bpoll.h"
#include "lib/crypto_openssl.h"

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <algorithm>
#include <array>

#include "lib/bsock.h"
#include "lib/tls/openssl.h"
#include "lib/bstringlist.h"
#include "lib/ascii_control_characters.h"
#include "include/jcr.h"

#include "lib/parse_conf.h"

#include "lib/thread_util.h"

namespace {
std::mutex file_access_mutex_;

class TlsOpenSsl : public Tls {
 public:
  TlsOpenSsl();
  virtual ~TlsOpenSsl();
  TlsOpenSsl(TlsOpenSsl& other) = delete;

  bool init() override;

  bool TlsPostconnectVerifyHost(JobControlRecord* jcr,
                                const char* host) override;
  bool TlsPostconnectVerifyCn(
      JobControlRecord* jcr,
      const std::vector<std::string>& verify_list) override;

  bool TlsBsockAccept(BareosSocket* bsock) override;
  int TlsBsockWriten(BareosSocket* bsock, char* ptr, int32_t nbytes) override;
  int TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes) override;
  bool TlsBsockConnect(BareosSocket* bsock) override;
  void TlsBsockShutdown(BareosSocket* bsock) override;

  std::string TlsCipherGetName() const override;
  void SetCipherList(const std::string& cipherlist) override;
  void SetCipherSuites(const std::string& ciphersuites) override;
  void SetProtocol(const std::string& protocol) override;
  void TlsLogConninfo(JobControlRecord* jcr,
                      const char* host,
                      int port,
                      const char* who) const override;
  void SetTlsPskClientContext(const PskCredentials& credentials) override;
  void SetTlsPskServerContext(ConfigurationParser* config) override;

  void Setca_certfile_(const std::string& ca_certfile) override;
  void SetCaCertdir(const std::string& ca_certdir) override;
  void SetCrlfile(const std::string& crlfile) override;
  void SetCertfile(const std::string& certfile) override;
  void SetKeyfile(const std::string& keyfile) override;
  void SetPemCallback(CRYPTO_PEM_PASSWD_CB pem_callback) override;
  void SetPemUserdata(void* pem_userdata) override;
  void SetDhFile(const std::string& dhfile_) override;
  void SetVerifyPeer(const bool& verify_peer) override;
  void SetEnableKtls(bool ktls) override;
  void SetTcpFileDescriptor(const int& fd) override;

  bool KtlsSendStatus() override;
  bool KtlsRecvStatus() override;

  int TlsPendingBytes() override;

 private:
  friend int tls_pem_callback_dispatch(char* buf,
                                       int size,
                                       int,
                                       void* userdata);

  void ClientContextInsertCredentials(const PskCredentials& credentials);

  bool OpensslBsockSessionStart(BareosSocket* bsock, bool server);

  int OpensslBsockReadwrite(BareosSocket* bsock,
                            char* ptr,
                            int nbytes,
                            bool write);

 private:
  /* each TCP connection has its own SSL_CTX object and SSL object */
  SSL* openssl_{};
  SSL_CTX* openssl_ctx_{};
  SSL_CONF_CTX* openssl_conf_ctx_{};

  /* openssl protocol command */
  std::string protocol_;

  /* cert attributes */
  int tcp_file_descriptor_{kInvalidFiledescriptor};
  std::string ca_certfile_;
  std::string ca_certdir_;
  std::string crlfile_;
  std::string certfile_;
  std::string keyfile_;
  CRYPTO_PEM_PASSWD_CB* pem_callback_{};
  void* pem_userdata_{};
  std::string dhfile_;
  std::string cipherlist_;
  std::string ciphersuites_;
  bool verify_peer_{};
  bool enable_ktls_{false};
  std::shared_ptr<ConfigResourcesContainer>
      config_table_{};  // config table being used

  std::optional<PskCredentials> credentials_;
};

/* No anonymous ciphers, no <128 bit ciphers, no export ciphers, no MD5 ciphers
 */
constexpr std::string_view tls_default_ciphers_{
    "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"};

// report any errors that occurred
int OpensslVerifyPeer(int preverify_ok, X509_STORE_CTX* store)
{
  if (!preverify_ok) {
    X509* cert = X509_STORE_CTX_get_current_cert(store);
    int depth = X509_STORE_CTX_get_error_depth(store);
    int err = X509_STORE_CTX_get_error(store);
    char issuer[256];
    char subject[256];

    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, 256);
    X509_NAME_oneline(X509_get_subject_name(cert), subject, 256);

    Jmsg5(NULL, M_ERROR, 0,
          T_("Error with certificate at depth: %d, issuer = %s,"
             " subject = %s, ERR=%d:%s\n"),
          depth, issuer, subject, err, X509_verify_cert_error_string(err));
  }

  return preverify_ok;
}

int TlsOpenSsl::OpensslBsockReadwrite(BareosSocket* bsock,
                                      char* ptr,
                                      int nbytes,
                                      bool write)
{
  if (!openssl_) {
    Dmsg0(100, "Attempt to write on a non initialized tls connection\n");
    return 0;
  }

  int flags = bsock->SetNonblocking();

  bsock->timer_start = watchdog_time;
  bsock->ClearTimedOut();
  bsock->SetKillable(false);

  int nleft = nbytes;

  while (nleft > 0) {
    int nwritten = 0;
    if (write) {
      nwritten = SSL_write(openssl_, ptr, nleft);
    } else {
      nwritten = SSL_read(openssl_, ptr, nleft);
    }

    int ssl_error = SSL_get_error(openssl_, nwritten);
    LogSSLError(ssl_error);
    switch (ssl_error) {
      case SSL_ERROR_NONE:
        nleft -= nwritten;
        if (nleft) { ptr += nwritten; }
        break;
      case SSL_ERROR_SYSCALL:
        if (nwritten == -1) {
          if (errno == EINTR) { continue; }
          if (errno == EAGAIN) {
            Bmicrosleep(0, 20000); /* try again in 20 ms */
            continue;
          }
        }
        OpensslPostErrors(bsock->get_jcr(), M_FATAL,
                          T_("TLS read/write failure."));
        goto cleanup;
      case SSL_ERROR_WANT_READ:
        WaitForReadableFd(bsock->fd_, 10000, false);
        break;
      case SSL_ERROR_WANT_WRITE:
        WaitForWritableFd(bsock->fd_, 10000, false);
        break;
      case SSL_ERROR_ZERO_RETURN:
        /* TLS connection was cleanly shut down */
        /* Fall through wanted */
      default:
        /* Socket Error Occurred */
        OpensslPostErrors(bsock->get_jcr(), M_FATAL,
                          T_("TLS read/write failure."));
        goto cleanup;
    }

    if (bsock->UseBwlimit()) {
      if (nwritten > 0) { bsock->ControlBwlimit(nwritten); }
    }

    /* Everything done? */
    if (nleft == 0) { goto cleanup; }

    /* Timeout/Termination, let's take what we can get */
    if (bsock->IsTimedOut() || bsock->IsTerminated()) { goto cleanup; }
  }

cleanup:
  /* Restore saved flags */
  bsock->RestoreBlocking(flags);

  /* Clear timer */
  bsock->timer_start = 0;
  bsock->SetKillable(true);

  return nbytes - nleft;
}

bool TlsOpenSsl::OpensslBsockSessionStart(BareosSocket* bsock, bool server)
{
  bool status = true;

  int flags = bsock->SetNonblocking();

  bsock->timer_start = watchdog_time;
  bsock->ClearTimedOut();
  bsock->SetKillable(false);

  for (;;) {
    int err_accept;
    if (server) {
      err_accept = SSL_accept(openssl_);
    } else {
      err_accept = SSL_connect(openssl_);
    }

    int ssl_error = SSL_get_error(openssl_, err_accept);
    LogSSLError(ssl_error);
    switch (ssl_error) {
      case SSL_ERROR_NONE:
        bsock->SetTlsEstablished();
        status = true;
        goto cleanup;
      case SSL_ERROR_ZERO_RETURN:
        /* TLS connection was cleanly shut down */
        OpensslPostErrors(bsock->get_jcr(), M_FATAL, T_("Connect failure"));
        status = false;
        goto cleanup;
      case SSL_ERROR_WANT_READ:
        WaitForReadableFd(bsock->fd_, 10000, false);
        break;
      case SSL_ERROR_WANT_WRITE:
        WaitForWritableFd(bsock->fd_, 10000, false);
        break;
      default:
        /* Socket Error Occurred */
        OpensslPostErrors(bsock->get_jcr(), M_FATAL, T_("Connect failure"));
        status = false;
        goto cleanup;
    }

    if (bsock->IsTimedOut()) { goto cleanup; }
  }

cleanup:
  /* Restore saved flags */
  bsock->RestoreBlocking(flags);
  /* Clear timer */
  bsock->timer_start = 0;
  bsock->SetKillable(true);

  if (enable_ktls_) {
    // old openssl versions might return -1 as well; so check for > 0 instead
    bool ktls_send = KtlsSendStatus();
    bool ktls_recv = KtlsRecvStatus();
    Dmsg1(150, "kTLS used for Recv: %s\n", ktls_recv ? "yes" : "no");
    Dmsg1(150, "kTLS used for Send: %s\n", ktls_send ? "yes" : "no");
  }

  return status;
}

int tls_pem_callback_dispatch(char* buf, int size, int, void* userdata)
{
  TlsOpenSsl* p = static_cast<TlsOpenSsl*>(userdata);
  return (p->pem_callback_(buf, size, p->pem_userdata_));
}

enum class CtxDataIndex : int
{
  Config = 0,
  Cred = 1,
};

void TlsOpenSsl::ClientContextInsertCredentials(
    const PskCredentials& credentials)
{
  if (!openssl_ctx_) { /* do not register nullptr */
    Dmsg0(100, "Psk Server Callback: No SSL_CTX\n");
    return;
  }

  ASSERT(!credentials_.has_value());
  credentials_ = credentials;
  SSL_CTX_set_ex_data(openssl_ctx_, static_cast<int>(CtxDataIndex::Cred),
                      &*credentials_);
}

const PskCredentials& SSL_CTX_getcred(const SSL_CTX* ctx)
{
  void* ptr = SSL_CTX_get_ex_data(ctx, static_cast<int>(CtxDataIndex::Cred));
  ASSERT(ptr);
  auto* creds = static_cast<const PskCredentials*>(ptr);
  return *creds;
}

void SSL_CTX_setconfig(SSL_CTX* ctx, ConfigurationParser* parser)
{
  SSL_CTX_set_ex_data(ctx, static_cast<int>(CtxDataIndex::Config), parser);
}

ConfigurationParser* SSL_CTX_getconfig(SSL_CTX* ctx)
{
  void* ptr = SSL_CTX_get_ex_data(ctx, static_cast<int>(CtxDataIndex::Config));
  auto* config = static_cast<ConfigurationParser*>(ptr);
  return config;
}

unsigned int psk_server_cb(SSL* ssl,
                           const char* identity,
                           unsigned char* psk_output,
                           unsigned int max_psk_len)
{
  unsigned int result = 0;

  SSL_CTX* openssl_ctx = SSL_get_SSL_CTX(ssl);

  if (!openssl_ctx) {
    Dmsg0(100, "Psk Server Callback: No SSL_CTX\n");
    return result;
  }
  BStringList lst(std::string(identity),
                  AsciiControlCharacters::RecordSeparator());
  Dmsg1(100, "psk_server_cb. identity: %s.\n", lst.JoinReadable().c_str());

  std::string configured_psk;

  auto* config = SSL_CTX_getconfig(openssl_ctx);

  if (!config) {
    Dmsg0(100, "Config not set: kConfigurationParserPtr\n");
    return result;
  }

  if (!config->GetTlsPskByFullyQualifiedResourceName(config, identity,
                                                     configured_psk)) {
    Dmsg0(100, "Error, TLS-PSK credentials not found.\n");
  } else {
    int psklen = Bsnprintf((char*)psk_output, max_psk_len, "%s",
                           configured_psk.c_str());
    result = (psklen < 0) ? 0 : psklen;
    Dmsg1(100, "psk_server_cb. result: %u.\n", result);
  }
  return result;
}

unsigned int psk_client_cb(SSL* ssl,
                           const char* /*hint*/,
                           char* identity,
                           unsigned int max_identity_len,
                           unsigned char* psk,
                           unsigned int max_psk_len)
{
  const SSL_CTX* openssl_ctx = SSL_get_SSL_CTX(ssl);

  if (!openssl_ctx) {
    Dmsg0(100, "Psk Client Callback: No SSL_CTX\n");
    return 0;
  }

  const PskCredentials& credentials = SSL_CTX_getcred(openssl_ctx);

  int ret = Bsnprintf(identity, max_identity_len, "%s",
                      credentials.get_identity().c_str());

  if (ret < 0 || (unsigned int)ret > max_identity_len) {
    Dmsg0(100, "Error, identify too long\n");
    return 0;
  }
  std::string identity_log = identity;
  std::replace(identity_log.begin(), identity_log.end(),
               AsciiControlCharacters::RecordSeparator(), ' ');
  Dmsg1(100, "psk_client_cb. identity: %s.\n", identity_log.c_str());

  ret = Bsnprintf((char*)psk, max_psk_len, "%s", credentials.get_psk().c_str());
  if (ret < 0 || (unsigned int)ret > max_psk_len) {
    Dmsg0(100, "Error, psk too long\n");
    return 0;
  }
  return ret;
}

// public interfaces from TlsOpenSsl that set private data
void TlsOpenSsl::Setca_certfile_(const std::string& ca_certfile)
{
  Dmsg1(100, "Set ca_certfile:\t<%s>\n", ca_certfile.c_str());
  ca_certfile_ = ca_certfile;
}

void TlsOpenSsl::SetCaCertdir(const std::string& ca_certdir)
{
  Dmsg1(100, "Set ca_certdir:\t<%s>\n", ca_certdir.c_str());
  ca_certdir_ = ca_certdir;
}

void TlsOpenSsl::SetCrlfile(const std::string& crlfile)
{
  Dmsg1(100, "Set crlfile_:\t<%s>\n", crlfile.c_str());
  crlfile_ = crlfile;
}

void TlsOpenSsl::SetCertfile(const std::string& certfile)
{
  Dmsg1(100, "Set certfile_:\t<%s>\n", certfile.c_str());
  certfile_ = certfile;
}

void TlsOpenSsl::SetKeyfile(const std::string& keyfile)
{
  Dmsg1(100, "Set keyfile_:\t<%s>\n", keyfile.c_str());
  keyfile_ = keyfile;
}

void TlsOpenSsl::SetPemCallback(CRYPTO_PEM_PASSWD_CB pem_callback)
{
  Dmsg1(100, "Set pem_callback to address: <%p>\n", pem_callback);
  pem_callback_ = pem_callback;
}

void TlsOpenSsl::SetPemUserdata(void* pem_userdata)
{
  Dmsg1(100, "Set pem_userdata to address: <%p>\n", pem_userdata);
  pem_userdata_ = pem_userdata;
}

void TlsOpenSsl::SetDhFile(const std::string& dhfile)
{
  Dmsg1(100, "Set dhfile_:\t<%s>\n", dhfile.c_str());
  dhfile_ = dhfile;
}

void TlsOpenSsl::SetVerifyPeer(const bool& verify_peer)
{
  Dmsg1(100, "Set Verify Peer:\t<%s>\n", verify_peer ? "true" : "false");
  verify_peer_ = verify_peer;
}

void TlsOpenSsl::SetEnableKtls(bool ktls)
{
  Dmsg1(100, "Set ktls:\t<%s>\n", ktls ? "true" : "false");
  enable_ktls_ = ktls;
}

void TlsOpenSsl::SetTcpFileDescriptor(const int& fd)
{
  Dmsg1(100, "Set tcp filedescriptor: <%d>\n", fd);
  tcp_file_descriptor_ = fd;
}

void TlsOpenSsl::SetCipherList(const std::string& cipherlist)
{
  Dmsg1(100, "Set cipherlist:\t<%s>\n", cipherlist.c_str());
  cipherlist_ = cipherlist;
}

void TlsOpenSsl::SetCipherSuites(const std::string& ciphersuites)
{
  Dmsg1(100, "Set ciphersuites:\t<%s>\n", ciphersuites.c_str());
  ciphersuites_ = ciphersuites;
}

void TlsOpenSsl::SetProtocol(const std::string& protocol)
{
  Dmsg1(100, "Set protocol:\t<%s>\n", protocol.c_str());
  protocol_ = protocol;
}

TlsOpenSsl::TlsOpenSsl()
{
  Dmsg0(100, "Construct TlsOpenSsl\n");

  /* the SSL_CTX object is the factory that creates
   * openssl objects, so initialize this first */
  openssl_ctx_ = SSL_CTX_new(TLS_method());

  if (!openssl_ctx_) {
    OpensslPostErrors(M_FATAL, T_("Error initializing SSL context"));
    return;
  }

  openssl_conf_ctx_ = SSL_CONF_CTX_new();

  if (!openssl_conf_ctx_) {
    OpensslPostErrors(M_FATAL, T_("Error initializing SSL conf context"));
    SSL_CTX_free(openssl_ctx_);
    openssl_ctx_ = nullptr;
    return;
  }

  SSL_CONF_CTX_set_ssl_ctx(openssl_conf_ctx_, openssl_ctx_);
}

TlsOpenSsl::~TlsOpenSsl()
{
  Dmsg0(100, "Destruct TlsOpenSsl\n");

  if (openssl_conf_ctx_) {
    SSL_CONF_CTX_free(openssl_conf_ctx_);
    openssl_conf_ctx_ = nullptr;
  }

  /* Free in this order:
   * 1. openssl object
   * 2. openssl_ctx object */

  if (openssl_) {
    SSL_free(openssl_);
    openssl_ = nullptr;
  }

  /* the openssl_ctx object is the factory that creates
   * openssl objects, so delete this at the end */
  if (openssl_ctx_) {
    SSL_CTX_free(openssl_ctx_);
    openssl_ctx_ = nullptr;
  }
}

bool TlsOpenSsl::init()
{
  if (!openssl_ctx_) {
    OpensslPostErrors(M_FATAL,
                      T_("Error initializing TlsOpenSsl (no SSL_CTX)\n"));
    return false;
  }

  if (!protocol_.empty()) {
    SSL_CONF_CTX_set_flags(openssl_conf_ctx_,
                           SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_SHOW_ERRORS
                               | SSL_CONF_FLAG_CLIENT | SSL_CONF_FLAG_SERVER);

    bool err
        = SSL_CONF_cmd(openssl_conf_ctx_, "Protocol", protocol_.c_str()) != 2;

    if (err) {
      std::string err_str{T_("Error setting OpenSSL Protocol options:\n")};
      std::array<char, 256> buffer;
      ERR_error_string(ERR_get_error(), buffer.data());
      err_str += buffer.data();
      err_str += "\n";
      Dmsg1(100, "%s", err_str.c_str());
      return false;
    }
  }

  SSL_CTX_set_options(openssl_ctx_, SSL_OP_ALL);

  SSL_CTX_set_options(openssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
  SSL_CTX_set_read_ahead(openssl_ctx_, 1);

#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  if (enable_ktls_) { SSL_CTX_set_options(openssl_ctx_, SSL_OP_ENABLE_KTLS); }
#endif

  if (cipherlist_.empty()) { cipherlist_ = tls_default_ciphers_; }

  if (SSL_CTX_set_cipher_list(openssl_ctx_, cipherlist_.c_str()) != 1) {
    OpensslPostErrors(M_ERROR, "Error setting cipher list");
    return false;
  }

  // use the default tls 1.3 cipher suites if nothing is set
  if (!ciphersuites_.empty()
      && SSL_CTX_set_ciphersuites(openssl_ctx_, ciphersuites_.c_str()) != 1) {
    OpensslPostErrors(M_ERROR, "Error setting cipher suite");
    return false;
  }

  if (pem_callback_ == nullptr) {
    pem_callback_ = CryptoDefaultPemCallback;
    pem_userdata_ = NULL;
  }

  SSL_CTX_set_default_passwd_cb(openssl_ctx_, tls_pem_callback_dispatch);
  SSL_CTX_set_default_passwd_cb_userdata(openssl_ctx_,
                                         static_cast<void*>(this));

  const char* ca_certfile
      = ca_certfile_.empty() ? nullptr : ca_certfile_.c_str();
  const char* ca_certdir = ca_certdir_.empty() ? nullptr : ca_certdir_.c_str();

  if (ca_certfile || ca_certdir) { /* at least one should be set */
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!SSL_CTX_load_verify_locations(openssl_ctx_, ca_certfile, ca_certdir)) {
      OpensslPostErrors(M_FATAL,
                        T_("Error loading certificate verification stores"));
      return false;
    }
  } else if (verify_peer_) {
    /* At least one CA is required for peer verification */
    Dmsg0(100, T_("Either a certificate file or a directory must be"
                  " specified as a verification store\n"));
  }

  if (!crlfile_.empty()) {
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    X509_STORE* store = SSL_CTX_get_cert_store(openssl_ctx_);
    if (!store) {
      OpensslPostErrors(M_FATAL,
                        T_("Error getting certificate verification store"));
      return false;
    }

    X509_LOOKUP* lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
    if (!lookup) {
      OpensslPostErrors(M_FATAL, T_("Error creating CRL lookup handler"));
      return false;
    }

    if (X509_load_crl_file(lookup, crlfile_.c_str(), X509_FILETYPE_PEM) <= 0) {
      OpensslPostErrors(M_FATAL,
                        T_("Error loading certificate revocation list"));
      return false;
    }

    if (!X509_STORE_set_flags(
            store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL)) {
      OpensslPostErrors(M_FATAL, T_("Error enabling CRL verification"));
      return false;
    }
  }

  if (!certfile_.empty()) {
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!SSL_CTX_use_certificate_chain_file(openssl_ctx_, certfile_.c_str())) {
      OpensslPostErrors(M_FATAL, T_("Error loading certificate file"));
      return false;
    }
  }

  if (!keyfile_.empty()) {
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!SSL_CTX_use_PrivateKey_file(openssl_ctx_, keyfile_.c_str(),
                                     SSL_FILETYPE_PEM)) {
      OpensslPostErrors(M_FATAL, T_("Error loading private key"));
      return false;
    }
  }

  if (!dhfile_.empty()) { /* Diffie-Hellman parameters */
    BIO* bio;
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!(bio = BIO_new_file(dhfile_.c_str(), "r"))) {
      OpensslPostErrors(M_FATAL, T_("Unable to open DH parameters file"));
      return false;
    }
    IGNORE_DEPRECATED_ON;
    DH* dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    IGNORE_DEPRECATED_OFF;
    BIO_free(bio);
    if (!dh) {
      OpensslPostErrors(M_FATAL,
                        T_("Unable to load DH parameters from specified file"));
      return false;
    }
    if (!SSL_CTX_set_tmp_dh(openssl_ctx_, dh)) {
      OpensslPostErrors(M_FATAL,
                        T_("Failed to set TLS Diffie-Hellman parameters"));
      IGNORE_DEPRECATED_ON;
      DH_free(dh);
      IGNORE_DEPRECATED_OFF;
      return false;
    }

    // SSL_CTX_set_tmp_dh creates a copy, so we need to free the parameters
    IGNORE_DEPRECATED_ON;
    DH_free(dh);
    IGNORE_DEPRECATED_OFF;
    SSL_CTX_set_options(openssl_ctx_, SSL_OP_SINGLE_DH_USE);
  }

  if (verify_peer_) {
    // SSL_VERIFY_FAIL_IF_NO_PEER_CERT has no effect in client mode
    SSL_CTX_set_verify(openssl_ctx_,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       OpensslVerifyPeer);
  } else {
    SSL_CTX_set_verify(openssl_ctx_, SSL_VERIFY_NONE, NULL);
  }

  openssl_ = SSL_new(openssl_ctx_);
  if (!openssl_) {
    OpensslPostErrors(M_FATAL, T_("Error creating new SSL object"));
    return false;
  }

  /* Non-blocking partial writes */
  SSL_set_mode(openssl_, SSL_MODE_ENABLE_PARTIAL_WRITE
                             | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  BIO* bio = BIO_new(BIO_s_socket());
  if (!bio) {
    OpensslPostErrors(M_FATAL, T_("Error creating file descriptor-based BIO"));
    return false;
  }

  ASSERT(tcp_file_descriptor_ >= 0);  // 0 is a good (socket-)fd
  BIO_set_fd(bio, tcp_file_descriptor_, BIO_NOCLOSE);

  SSL_set_bio(openssl_, bio, bio);

  return true;
}

void TlsOpenSsl::SetTlsPskClientContext(const PskCredentials& credentials)
{
  if (!openssl_ctx_) {
    Dmsg0(50, "Could not set TLS_PSK CLIENT context (no SSL_CTX)\n");
    return;
  }
  BStringList ident(credentials.get_identity(),
                    AsciiControlCharacters::RecordSeparator());
  Dmsg1(50, "Preparing TLS_PSK CLIENT context for identity %s\n",
        ident.JoinReadable().c_str());
  ClientContextInsertCredentials(credentials);
  SSL_CTX_set_psk_client_callback(openssl_ctx_, psk_client_cb);
}

void TlsOpenSsl::SetTlsPskServerContext(ConfigurationParser* config)
{
  if (!openssl_ctx_) {
    Dmsg0(50, "Could not prepare TLS_PSK SERVER callback (no SSL_CTX)\n");
    return;
  }

  if (!config) {
    Dmsg0(50, "Could not prepare TLS_PSK SERVER callback (no config)\n");
    return;
  }

  // keep a shared_ptr to the current config, so a reload won't
  // free the memory we're going to use in the private context
  config_table_ = config->GetResourcesContainer();
  SSL_CTX_setconfig(openssl_ctx_, config);

  SSL_CTX_set_psk_server_callback(openssl_ctx_, psk_server_cb);
}

std::string TlsOpenSsl::TlsCipherGetName() const
{
  if (openssl_) {
    const SSL_CIPHER* cipher = SSL_get_current_cipher(openssl_);
    const char* protocol_name = SSL_get_version(openssl_);
    if (cipher) {
      return std::string(SSL_CIPHER_get_name(cipher)) + " " + protocol_name;
    }
  }
  return std::string();
}

void TlsOpenSsl::TlsLogConninfo(JobControlRecord* jcr,
                                const char* host,
                                int port,
                                const char* who) const
{
  if (!openssl_) {
    Qmsg(jcr, M_INFO, 0, T_("No openssl to %s at %s:%d established\n"), who,
         host, port);
  } else {
    std::string cipher_name = TlsCipherGetName();
    Qmsg(jcr, M_INFO, 0, T_("Connected %s at %s:%d, encryption: %s\n"), who,
         host, port, cipher_name.empty() ? "Unknown" : cipher_name.c_str());
  }
}

/*
 * Verifies a list of common names against the certificate commonName
 * attribute.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsOpenSsl::TlsPostconnectVerifyCn(
    JobControlRecord* jcr,
    const std::vector<std::string>& verify_list)
{
  X509* cert;
  X509_NAME* subject;
  bool auth_success = false;

  if (!(cert = SSL_get_peer_certificate(openssl_))) {
    Qmsg0(jcr, M_ERROR, 0, T_("Peer failed to present a TLS certificate\n"));
    return false;
  }

  if ((subject = X509_get_subject_name(cert)) != NULL) {
    char data[256]; /* nullterminated by X509_NAME_get_text_by_NID */
    if (X509_NAME_get_text_by_NID(subject, NID_commonName, data, sizeof(data))
        > 0) {
      const std::string_view d(data);
      for (const std::string& cn : verify_list) {
        Dmsg2(120, "comparing CNs: cert-cn=%s, allowed-cn=%s\n", data,
              cn.c_str());
        if (d.compare(cn) == 0) { auth_success = true; }
      }
    }
  }

  X509_free(cert);
  return auth_success;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName
 * attributes.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsOpenSsl::TlsPostconnectVerifyHost(JobControlRecord* jcr,
                                          const char* host)
{
  int i, j;
  int extensions;
  int cnLastPos = -1;
  X509* cert;
  X509_NAME* subject;
  X509_NAME_ENTRY* neCN;
  ASN1_STRING* asn1CN;
  bool auth_success = false;
  auto free_subject_alt_name_data
      = [](const X509V3_EXT_METHOD* method, void* extstr,
           STACK_OF(CONF_VALUE) * val) {
          if (val) { sk_CONF_VALUE_pop_free(val, X509V3_conf_free); }

          if (!extstr) { return; }

          if (method->it) {
            ASN1_item_free(reinterpret_cast<ASN1_VALUE*>(extstr),
                           ASN1_ITEM_ptr(method->it));
          } else if (method->ext_free) {
            method->ext_free(extstr);
          }
        };

  if (!(cert = SSL_get_peer_certificate(openssl_))) {
    Qmsg1(jcr, M_ERROR, 0, T_("Peer %s failed to present a TLS certificate\n"),
          host);
    return false;
  }

  // Check subjectAltName extensions first
  if ((extensions = X509_get_ext_count(cert)) > 0) {
    for (i = 0; i < extensions; i++) {
      X509_EXTENSION* ext;
      const char* extname;

      ext = X509_get_ext(cert, i);
      extname = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

      if (bstrcmp(extname, "subjectAltName")) {
        const X509V3_EXT_METHOD* method;
        STACK_OF(CONF_VALUE)* val = nullptr;
        CONF_VALUE* nval;
        void* extstr = nullptr;
        const unsigned char* ext_value_data;

        if (!(method = X509V3_EXT_get(ext))) { break; }

        ext_value_data = X509_EXTENSION_get_data(ext)->data;

        if (method->it) {
          extstr = ASN1_item_d2i(NULL, &ext_value_data,
                                 X509_EXTENSION_get_data(ext)->length,
                                 ASN1_ITEM_ptr(method->it));
        } else {
          /* Old style ASN1
           * Decode ASN1 item in data */
          extstr = method->d2i(NULL, &ext_value_data,
                               X509_EXTENSION_get_data(ext)->length);
        }

        // Iterate through to find the dNSName field(s)
        val = method->i2v(method, extstr, NULL);
        if (!val) {
          free_subject_alt_name_data(method, extstr, val);
          continue;
        }

        for (j = 0; j < sk_CONF_VALUE_num(val); j++) {
          nval = sk_CONF_VALUE_value(val, j);
          if (bstrcmp(nval->name, "DNS")) {
            if (Bstrcasecmp(nval->value, host)) {
              auth_success = true;
              break;
            }
          }
        }

        free_subject_alt_name_data(method, extstr, val);
        if (auth_success) { goto success; }
      }
    }
  }

  // Try verifying against the subject name
  if (!auth_success) {
    if ((subject = X509_get_subject_name(cert)) != NULL) {
      // Loop through all CNs
      for (;;) {
        cnLastPos
            = X509_NAME_get_index_by_NID(subject, NID_commonName, cnLastPos);
        if (cnLastPos == -1) { break; }
        neCN = X509_NAME_get_entry(subject, cnLastPos);
        asn1CN = X509_NAME_ENTRY_get_data(neCN);
        if (Bstrcasecmp((const char*)asn1CN->data, host)) {
          auth_success = true;
          break;
        }
      }
    }
  }

success:
  X509_free(cert);

  return auth_success;
}

bool TlsOpenSsl::TlsBsockConnect(BareosSocket* bsock)
{
  return OpensslBsockSessionStart(bsock, false);
}

bool TlsOpenSsl::TlsBsockAccept(BareosSocket* bsock)
{
  return OpensslBsockSessionStart(bsock, true);
}

void TlsOpenSsl::TlsBsockShutdown(BareosSocket* bsock)
{
  /* SSL_shutdown must be called twice to fully complete the process -
   * The first time to initiate the shutdown handshake, and the second to
   * receive the peer's reply.
   *
   * In addition, if the underlying socket is blocking, SSL_shutdown()
   * will not return until the current stage of the shutdown process has
   * completed or an error has occurred. By setting the socket blocking
   * we can avoid the ugly for()/switch()/select() loop. */

  if (!openssl_) { return; }

  /* Set socket blocking for shutdown */
  bsock->SetBlocking();

  btimer_t* tid = StartBsockTimer(bsock, 60 * 2);

  int err_shutdown = SSL_shutdown(openssl_);

  StopBsockTimer(tid);

  if (err_shutdown == 0) {
    /* Complete the shutdown with the second call */
    tid = StartBsockTimer(bsock, 2);
    err_shutdown = SSL_shutdown(openssl_);
    StopBsockTimer(tid);
  }

  int ssl_error = SSL_get_error(openssl_, err_shutdown);
  LogSSLError(ssl_error);

  /* There may be more errors on the thread-local error-queue.
   * As we just shutdown our context and looked at the errors that we were
   * interested in we clear the queue so nobody else gets to read an error
   * that may have occurred here. */
  ERR_clear_error();  // empties the current thread's openssl error queue

  SSL_free(openssl_);
  openssl_ = nullptr;


  JobControlRecord* jcr = bsock->get_jcr();

  if (jcr && jcr->is_passive_client_connection_probing) { return; }

  std::string message{T_("TLS shutdown failure.")};

  switch (ssl_error) {
    case SSL_ERROR_NONE:
      break;
    case SSL_ERROR_ZERO_RETURN:
      /* TLS connection was shut down on us via a TLS protocol-level closure
       */
      OpensslPostErrors(jcr, M_ERROR, message.c_str());
      break;
    default:
      /* Socket Error Occurred */
      OpensslPostErrors(jcr, M_ERROR, message.c_str());
      break;
  }
}

int TlsOpenSsl::TlsBsockWriten(BareosSocket* bsock, char* ptr, int32_t nbytes)
{
  return OpensslBsockReadwrite(bsock, ptr, nbytes, true);
}

int TlsOpenSsl::TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes)
{
  return OpensslBsockReadwrite(bsock, ptr, nbytes, false);
}

bool TlsOpenSsl::KtlsSendStatus()
{
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  // old openssl versions might return -1 as well; so check for > 0 instead
  return BIO_get_ktls_send(SSL_get_wbio(openssl_)) > 0;
#else
  return false;
#endif
}

bool TlsOpenSsl::KtlsRecvStatus()
{
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  // old openssl versions might return -1 as well; so check for > 0 instead
  return BIO_get_ktls_recv(SSL_get_rbio(openssl_)) > 0;
#else
  return false;
#endif
}

int TlsOpenSsl::TlsPendingBytes()
{
  /* SSL_pending() returns the amount of already decrypted bytes
   * since we are using readahead, openssl will read as many bytes as possible
   * without decrypting them, so SSL_pending() may return
   * false even though some bytes are ready to be read.
   * As such, we use SSL_has_pending() as that returns a truthy value if
   * any number of bytes are inside openssls buffer.
   * See https://docs.openssl.org/3.6/man3/SSL_pending for more information */
  if (SSL_has_pending(openssl_)) { return 1; }

  return 0;
}
};  // namespace

std::unique_ptr<Tls> make_openssl_tls()
{
  return std::make_unique<TlsOpenSsl>();
}
