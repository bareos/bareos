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
#include "lib/global_resource.h"
#include "lib/tls/openssl.h"
#include "lib/bstringlist.h"
#include "lib/ascii_control_characters.h"
#include "include/jcr.h"
#include "lib/tls_psk_credentials.h"

#include "lib/parse_conf.h"

#include "lib/thread_util.h"

namespace {
std::mutex file_access_mutex_;

#define UNIQUE_PTR(Type)                           \
  std::unique_ptr<Type, decltype([](Type* val) {   \
                    if (val) { Type##_free(val); } \
                  })>

using ssl_ptr = UNIQUE_PTR(SSL);
using ssl_ctx_ptr = UNIQUE_PTR(SSL_CTX);
using ssl_conf_ptr = UNIQUE_PTR(SSL_CONF_CTX);
using cert_ptr = UNIQUE_PTR(X509);
using general_names_ptr = UNIQUE_PTR(GENERAL_NAMES);

#undef UNIQUE_PTR

class TlsOpenSsl : public Tls {
 public:
  TlsOpenSsl(const TlsResource* res, ssl_ptr ptr);
  virtual ~TlsOpenSsl();

  static std::unique_ptr<TlsOpenSsl> make_server(const TlsResource* res,
                                                 TlsConfigProvider* config);

  static std::unique_ptr<TlsOpenSsl> make_client(const TlsResource* res,
                                                 const PskCredentials* creds);

  bool TlsBsockAccept(BareosSocket* bsock) override;
  int TlsBsockWriten(BareosSocket* bsock, char* ptr, int32_t nbytes) override;
  int TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes) override;
  bool TlsBsockConnect(JobControlRecord* jcr, BareosSocket* bsock) override;
  int TlsBsockPeekn(const BareosSocket* bsock,
                    char* ptr,
                    int32_t nbytes) override;
  void TlsBsockShutdown(BareosSocket* bsock) override;

  PskIdentityStatus IsPskIdentityInUse(
      std::string_view identity) const override;

  std::string TlsCipherGetName() const override;
  void TlsLogConninfo(JobControlRecord* jcr,
                      const char* host,
                      int port,
                      const char* who) const override;


  bool KtlsSendStatus() override;
  bool KtlsRecvStatus() override;
  int TlsPendingBytes() override;

 private:
  bool TlsPostconnectVerifyHost(X509* cert, const char* host);
  bool TlsPostconnectVerifyCn(X509* cert,
                              const std::vector<std::string>& verify_list);

  void ClientContextInsertCredentials(const PskCredentials& credentials);

  bool OpensslBsockSessionStart(BareosSocket* bsock, bool server);

  int OpensslBsockReadwrite(BareosSocket* bsock,
                            char* ptr,
                            int nbytes,
                            bool write);

  SSL* ssl() { return openssl_.get(); }
  const SSL* ssl() const { return openssl_.get(); }

 private:
  /* each TCP connection has its own SSL object.  We do not reuse SSL_CTX
   * objects, so there is no need for us to keep a separate pointer to it.
   * If necessary it is still accessible via the SSL object. */
  ssl_ptr openssl_{};

  VerifyPeerSetting verify_peer_{};
  std::vector<std::string> allowed_common_names{};

  std::optional<PskCredentials> credentials_;
};

/* No anonymous ciphers, no <128 bit ciphers, no export ciphers, no MD5 ciphers
 */
constexpr const char* tls_default_ciphers_{"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"};

struct CommonName {
  int index;
  std::string value;
};

std::optional<CommonName> GetCommonName(const X509_NAME* subject,
                                        int previous_index)
{
  if (!subject) { return std::nullopt; }

  /* Pre OpenSSL 4.0, this function still required a non-const name.
   * For compatibilities sake, we remove the const here. */
  const int index = X509_NAME_get_index_by_NID(const_cast<X509_NAME*>(subject),
                                               NID_commonName, previous_index);
  if (index == -1) { return std::nullopt; }

  const X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject, index);
  if (!entry) { return std::nullopt; }

  const ASN1_STRING* name = X509_NAME_ENTRY_get_data(entry);
  if (!name) { return std::nullopt; }

  unsigned char* raw_utf8_data = nullptr;
  const int length = ASN1_STRING_to_UTF8(&raw_utf8_data, name);
  using Utf8DataPtr
      = std::unique_ptr<unsigned char, decltype([](unsigned char* data) {
                          OPENSSL_free(data);
                        })>;
  Utf8DataPtr utf8_data{raw_utf8_data};
  if (!utf8_data || length <= 0) { return std::nullopt; }

  std::string value{reinterpret_cast<const char*>(utf8_data.get()),
                    static_cast<size_t>(length)};
  if (value.find('\0') != std::string::npos) { return std::nullopt; }

  return CommonName{index, std::move(value)};
}

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
      nwritten = SSL_write(ssl(), ptr, nleft);
    } else {
      nwritten = SSL_read(ssl(), ptr, nleft);
    }

    int ssl_error = SSL_get_error(ssl(), nwritten);
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
  if (bsock->fd_ < 0) {
    Dmsg0(50, "Cannot start tls session with invalid fd %d (bsock: %p)\n",
          bsock->fd_, bsock);
    return false;
  }

  BIO* bio = BIO_new(BIO_s_socket());
  if (!bio) {
    OpensslPostErrors(M_FATAL, T_("Error creating file descriptor-based BIO"));
    return false;
  }

  BIO_set_fd(bio, bsock->fd_, BIO_NOCLOSE);
  SSL_set_bio(ssl(), bio, bio);

#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  if (bsock->enable_ktls_) { SSL_set_options(ssl(), SSL_OP_ENABLE_KTLS); }
#endif

  bool status = true;

  int flags = bsock->SetNonblocking();

  bsock->timer_start = watchdog_time;
  bsock->ClearTimedOut();
  bsock->SetKillable(false);

  for (;;) {
    int err_accept;
    if (server) {
      err_accept = SSL_accept(ssl());
    } else {
      err_accept = SSL_connect(ssl());
    }

    int ssl_error = SSL_get_error(ssl(), err_accept);
    LogSSLError(ssl_error);
    switch (ssl_error) {
      case SSL_ERROR_NONE:
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

  if (bsock->enable_ktls_) {
    // old openssl versions might return -1 as well; so check for > 0 instead
    bool ktls_send = KtlsSendStatus();
    bool ktls_recv = KtlsRecvStatus();
    Dmsg1(150, "kTLS used for Recv: %s\n", ktls_recv ? "yes" : "no");
    Dmsg1(150, "kTLS used for Send: %s\n", ktls_send ? "yes" : "no");
  }

  return status;
}

int tls_pem_callback_dispatch(char* buf, int size, int, void*)
{
  return CryptoDefaultPemCallback(buf, size, nullptr);
}

enum class CtxDataIndex : int
{
  SecretProvider = 0,
  Cred = 1,
};

void TlsOpenSsl::ClientContextInsertCredentials(
    const PskCredentials& credentials)
{
  ASSERT(!credentials_.has_value());
  credentials_ = credentials;
  SSL_set_ex_data(ssl(), static_cast<int>(CtxDataIndex::Cred), &*credentials_);
}

const PskCredentials& SSL_getcred(SSL* ctx)
{
  void* ptr = SSL_get_ex_data(ctx, static_cast<int>(CtxDataIndex::Cred));
  ASSERT(ptr);
  auto* creds = static_cast<const PskCredentials*>(ptr);
  return *creds;
}

void SSL_set_secretprovider(SSL* ctx, TlsConfigProvider* parser)
{
  SSL_set_ex_data(ctx, static_cast<int>(CtxDataIndex::SecretProvider), parser);
}

TlsConfigProvider* SSL_get_secretprovider(SSL* ctx)
{
  void* ptr
      = SSL_get_ex_data(ctx, static_cast<int>(CtxDataIndex::SecretProvider));
  auto* provider = static_cast<TlsConfigProvider*>(ptr);
  return provider;
}

unsigned int psk_server_cb(SSL* ssl,
                           const char* identity,
                           unsigned char* psk_output,
                           unsigned int max_psk_len)
{
  static constexpr unsigned int ERROR_RETURN = 0;

  BStringList lst(std::string(identity),
                  AsciiControlCharacters::RecordSeparator());
  Dmsg1(100, "psk_server_cb. identity: %s.\n", lst.JoinReadable().c_str());

  std::string configured_psk;

  auto* data = SSL_get_secretprovider(ssl);

  if (!data) {
    Dmsg0(100, "secret provider not set!\n");
    return ERROR_RETURN;
  }

  auto [type, name] = global_resource::ParseQualifiedName(identity);

  if (type == global_resource::Type::Unknown) {
    Dmsg0(100, "Could not parse identity: %s!\n", identity);
    return ERROR_RETURN;
  }

  // , std::span<unsigned char>(psk_output, max_psk_len)
  auto* tls_res = data->get(type, name);

  if (!tls_res) { return ERROR_RETURN; }

  auto* pw = tls_res->password_.value;
  auto pw_len = strlen(pw);
  if (pw_len > max_psk_len) {
    Dmsg0(100, "psk_server_cb: password too long for %s\n", identity);
    return ERROR_RETURN;
  }

  memcpy(psk_output, pw, pw_len);
  return pw_len;
}

unsigned int psk_client_cb(SSL* ssl,
                           const char* /*hint*/,
                           char* identity,
                           unsigned int max_identity_len,
                           unsigned char* psk,
                           unsigned int max_psk_len)
{
  const PskCredentials& credentials = SSL_getcred(ssl);

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

TlsOpenSsl::TlsOpenSsl(const TlsResource* config, ssl_ptr ptr)
    : openssl_(std::move(ptr))
{
  Dmsg0(100, "Create TlsOpenSsl at %p\n", this);

  auto& tls_cert = config->tls_cert_;
  verify_peer_ = tls_cert.verify_peer_;

  allowed_common_names = tls_cert.allowed_certificate_common_names_;
}

TlsOpenSsl::~TlsOpenSsl() { Dmsg0(100, "Destruct TlsOpenSsl at %p\n", this); }

std::string TlsOpenSsl::TlsCipherGetName() const
{
  if (auto* openssl = ssl()) {
    const SSL_CIPHER* cipher = SSL_get_current_cipher(openssl);
    const char* protocol_name = SSL_get_version(openssl);
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
    X509* cert,
    const std::vector<std::string>& verify_list)
{
  ASSERT(cert);

  auto* subject = X509_get_subject_name(cert);
  if (subject != NULL) {
    const std::optional<CommonName> common_name = GetCommonName(subject, -1);
    if (common_name) {
      for (const std::string& cn : verify_list) {
        Dmsg2(120, "comparing CNs: cert-cn=%s, allowed-cn=%s\n",
              common_name->value.c_str(), cn.c_str());
        if (common_name->value == cn) { return true; }
      }
    }
  }

  return false;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName
 * attributes.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsOpenSsl::TlsPostconnectVerifyHost(X509* cert, const char* host)
{
  ASSERT(cert);

  int cnLastPos = -1;

  // Check subjectAltName extensions first
  if (general_names_ptr sans{static_cast<GENERAL_NAMES*>(
          X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr))}) {
    const int num = sk_GENERAL_NAME_num(sans.get());
    for (int i = 0; i < num; ++i) {
      const GENERAL_NAME* name = sk_GENERAL_NAME_value(sans.get(), i);
      if (name->type == GEN_DNS) {
        const ASN1_IA5STRING* dns = name->d.dNSName;
        const char* dns_data
            = reinterpret_cast<const char*>(ASN1_STRING_get0_data(dns));
        const int dns_len = ASN1_STRING_length(dns);
        if (dns_data && dns_len > 0) {
          std::string_view dns_view{dns_data, static_cast<size_t>(dns_len)};
          if (dns_view.find('\0') == std::string_view::npos
              && Bstrcasecmp(std::string(dns_view).c_str(), host)) {
            return true;
          }
        }
      }
    }
  }

  // Try verifying against the subject name
  auto* subject = X509_get_subject_name(cert);
  if (subject != NULL) {
    // Loop through all CNs
    for (;;) {
      const std::optional<CommonName> common_name
          = GetCommonName(subject, cnLastPos);
      if (!common_name) { break; }

      cnLastPos = common_name->index;
      if (Bstrcasecmp(common_name->value.c_str(), host)) { return true; }
    }
  }
  return false;
}

bool TlsOpenSsl::TlsBsockConnect(JobControlRecord* jcr, BareosSocket* bsock)
{
  if (!OpensslBsockSessionStart(bsock, false)) {
    Dmsg0(100, "Could not establish a tls session with %s\n", bsock->host());
    return false;
  }

  cert_ptr cert{SSL_get_peer_certificate(ssl())};
  switch (verify_peer_) {
    case VerifyPeerSetting::Disabled: {
      Dmsg0(200, "We do not check the peer\n");
      return true;
    } break;
    case VerifyPeerSetting::IfCertificatePresented: {
      if (!cert) {
        Dmsg0(200,
              "Peer did not present a TLS certificate -> skipping check\n");
        return true;
      }
    } break;
    case VerifyPeerSetting::Required: {
      if (!cert) {
        Qmsg0(jcr, M_ERROR, 0, "Peer failed to present a TLS certificate\n");
        return false;
      }
    } break;
    default: {
      Qmsg0(jcr, M_ERROR, 0, "Unknown verify peer setting: %zu\n",
            static_cast<size_t>(verify_peer_));
      return false;
    } break;
  }

  ASSERT(cert);

  /* If there's an Allowed CN verify list, use that to validate the remote
   * certificate's CN. Otherwise, we use standard host/CN matching. */
  if (!allowed_common_names.empty()) {
    if (!TlsPostconnectVerifyCn(cert.get(), allowed_common_names)) {
      Qmsg1(jcr, M_FATAL, 0,
            "TLS certificate verification failed."
            " Peer certificate did not match a required commonName\n");
      return false;
    }
  } else {
    if (!TlsPostconnectVerifyHost(cert.get(), bsock->host())) {
      Qmsg1(jcr, M_FATAL, 0,
            "TLS host certificate verification failed. Host name \"%s\" "
            "did not match presented certificate\n",
            bsock->host());
      return false;
    }
  }

  return true;
}

bool TlsOpenSsl::TlsBsockAccept(BareosSocket* bsock)
{
  if (!OpensslBsockSessionStart(bsock, true)) {
    Dmsg0(100, "Could not accept a tls session from %s\n", bsock->host());
    return false;
  }

  auto* jcr = bsock->jcr();

  cert_ptr cert{SSL_get_peer_certificate(ssl())};
  switch (verify_peer_) {
    case VerifyPeerSetting::Disabled: {
      Dmsg0(200, "We do not check the peer\n");
      return true;
    } break;
    case VerifyPeerSetting::IfCertificatePresented: {
      if (!cert) {
        Dmsg0(200,
              "Peer did not present a TLS certificate -> skipping check\n");
        return true;
      }
    } break;
    case VerifyPeerSetting::Required: {
      if (!cert) {
        Qmsg0(jcr, M_ERROR, 0, "Peer failed to present a TLS certificate\n");
        return false;
      }
    } break;
    default: {
      Qmsg0(jcr, M_ERROR, 0, "Unknown verify peer setting: %zu\n",
            static_cast<size_t>(verify_peer_));
      return false;
    } break;
  }

  ASSERT(cert);

  if (!allowed_common_names.empty()) {
    if (!TlsPostconnectVerifyCn(cert.get(), allowed_common_names)) {
      Qmsg1(bsock->jcr(), M_FATAL, 0,
            T_("TLS certificate verification failed."
               " Peer certificate did not match a required commonName\n"));
      return false;
    }
  }

  return true;
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

  int err_shutdown = SSL_shutdown(ssl());

  StopBsockTimer(tid);

  if (err_shutdown == 0) {
    /* Complete the shutdown with the second call */
    tid = StartBsockTimer(bsock, 2);
    err_shutdown = SSL_shutdown(ssl());
    StopBsockTimer(tid);
  }

  int ssl_error = SSL_get_error(ssl(), err_shutdown);
  LogSSLError(ssl_error);

  /* There may be more errors on the thread-local error-queue.
   * As we just shutdown our context and looked at the errors that we were
   * interested in we clear the queue so nobody else gets to read an error
   * that may have occurred here. */
  ERR_clear_error();  // empties the current thread's openssl error queue

  openssl_.reset();

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

int TlsOpenSsl::TlsBsockPeekn(const BareosSocket*, char* ptr, int32_t nbytes)
{
  if (!openssl_) {
    Dmsg0(100, "Attempt to write on a non initialized tls connection\n");
    return 0;
  }

  return SSL_peek(openssl_, ptr, nbytes);
}

bool TlsOpenSsl::KtlsSendStatus()
{
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  // old openssl versions might return -1 as well; so check for > 0 instead
  return BIO_get_ktls_send(SSL_get_wbio(ssl())) > 0;
#else
  return false;
#endif
}

bool TlsOpenSsl::KtlsRecvStatus()
{
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  // old openssl versions might return -1 as well; so check for > 0 instead
  return BIO_get_ktls_recv(SSL_get_rbio(ssl())) > 0;
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
  if (SSL_has_pending(ssl())) { return 1; }

  return 0;
}

ssl_ptr make_ssl_from_res(const TlsResource* res)
{
  /* the SSL_CTX object is the factory that creates
   * openssl objects, so initialize this first */
  ssl_ctx_ptr openssl_ctx_{SSL_CTX_new(TLS_method())};

  if (!openssl_ctx_) {
    OpensslPostErrors(M_FATAL, T_("Error initializing SSL context"));
    return {};
  }

  ssl_conf_ptr openssl_conf_ctx_{SSL_CONF_CTX_new()};

  if (!openssl_conf_ctx_) {
    OpensslPostErrors(M_FATAL, T_("Error initializing SSL conf context"));
    return {};
  }

  SSL_CONF_CTX_set_ssl_ctx(openssl_conf_ctx_.get(), openssl_ctx_.get());

  auto& tls_cert = res->tls_cert_;

  auto& protocol_ = res->protocol_;
  auto& cipherlist_ = res->cipherlist_;
  auto& ciphersuites_ = res->ciphersuites_;
  auto& ca_certfile_ = tls_cert.ca_certfile_;
  auto& ca_certdir_ = tls_cert.ca_certdir_;
  auto& crlfile_ = tls_cert.crlfile_;
  auto& certfile_ = tls_cert.certfile_;

  auto& keyfile_ = tls_cert.keyfile_;
  auto& dhfile_ = tls_cert.dhfile_;

  auto verify_peer = tls_cert.verify_peer_;

  if (!protocol_.empty()) {
    SSL_CONF_CTX_set_flags(openssl_conf_ctx_.get(),
                           SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_SHOW_ERRORS
                               | SSL_CONF_FLAG_CLIENT | SSL_CONF_FLAG_SERVER);

    bool err
        = SSL_CONF_cmd(openssl_conf_ctx_.get(), "Protocol", protocol_.c_str())
          != 2;

    if (err) {
      std::string err_str{T_("Error setting OpenSSL Protocol options:\n")};
      std::array<char, 256> buffer;
      ERR_error_string(ERR_get_error(), buffer.data());
      err_str += buffer.data();
      err_str += "\n";
      Dmsg1(100, "%s", err_str.c_str());
      return {};
    }
  }

  SSL_CTX_set_options(openssl_ctx_.get(), SSL_OP_ALL);

  SSL_CTX_set_options(openssl_ctx_.get(), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
  SSL_CTX_set_read_ahead(openssl_ctx_.get(), 1);

  SSL_CTX_set_default_passwd_cb(openssl_ctx_.get(), tls_pem_callback_dispatch);
  SSL_CTX_set_default_passwd_cb_userdata(openssl_ctx_.get(), nullptr);

  auto* used_cipher_list = tls_default_ciphers_;
  if (!cipherlist_.empty()) { used_cipher_list = cipherlist_.c_str(); }

  if (SSL_CTX_set_cipher_list(openssl_ctx_.get(), used_cipher_list) != 1) {
    OpensslPostErrors(M_ERROR, "Error setting cipher list");
    return {};
  }

  // use the default tls 1.3 cipher suites if nothing is set
  if (!ciphersuites_.empty()
      && SSL_CTX_set_ciphersuites(openssl_ctx_.get(), ciphersuites_.c_str())
             != 1) {
    OpensslPostErrors(M_ERROR, "Error setting cipher suite");
    return {};
  }

  const char* ca_certfile
      = ca_certfile_.empty() ? nullptr : ca_certfile_.c_str();
  const char* ca_certdir = ca_certdir_.empty() ? nullptr : ca_certdir_.c_str();

  if (ca_certfile || ca_certdir) { /* at least one should be set */
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!SSL_CTX_load_verify_locations(openssl_ctx_.get(), ca_certfile,
                                       ca_certdir)) {
      OpensslPostErrors(M_FATAL,
                        T_("Error loading certificate verification stores"));
      return {};
    }
  } else if (verify_peer != VerifyPeerSetting::Disabled) {
    /* At least one CA is required for peer verification */
    Dmsg0(100, T_("Either a certificate file or a directory must be"
                  " specified as a verification store\n"));
  }

  if (!crlfile_.empty()) {
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    X509_STORE* store = SSL_CTX_get_cert_store(openssl_ctx_.get());
    if (!store) {
      OpensslPostErrors(M_FATAL,
                        T_("Error getting certificate verification store"));
      return {};
    }

    X509_LOOKUP* lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
    if (!lookup) {
      OpensslPostErrors(M_FATAL, T_("Error creating CRL lookup handler"));
      return {};
    }

    if (X509_load_crl_file(lookup, crlfile_.c_str(), X509_FILETYPE_PEM) <= 0) {
      OpensslPostErrors(M_FATAL,
                        T_("Error loading certificate revocation list"));
      return {};
    }

    if (!X509_STORE_set_flags(
            store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL)) {
      OpensslPostErrors(M_FATAL, T_("Error enabling CRL verification"));
      return {};
    }
  }

  if (!certfile_.empty()) {
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!SSL_CTX_use_certificate_chain_file(openssl_ctx_.get(),
                                            certfile_.c_str())) {
      OpensslPostErrors(M_FATAL, T_("Error loading certificate file"));
      return {};
    }
  }

  if (!keyfile_.empty()) {
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    if (!SSL_CTX_use_PrivateKey_file(openssl_ctx_.get(), keyfile_.c_str(),
                                     SSL_FILETYPE_PEM)) {
      OpensslPostErrors(M_FATAL, T_("Error loading private key"));
      return {};
    }
  }

  if (!dhfile_.empty()) { /* Diffie-Hellman parameters */
    std::lock_guard<std::mutex> lg(file_access_mutex_);
    BIO* bio = BIO_new_file(dhfile_.c_str(), "r");
    if (!bio) {
      OpensslPostErrors(M_FATAL, T_("Unable to open DH parameters file"));
      return {};
    }
    IGNORE_DEPRECATED_ON;
    DH* dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    IGNORE_DEPRECATED_OFF;
    BIO_free(bio);
    if (!dh) {
      OpensslPostErrors(M_FATAL,
                        T_("Unable to load DH parameters from specified file"));
      return {};
    }
    if (!SSL_CTX_set_tmp_dh(openssl_ctx_.get(), dh)) {
      OpensslPostErrors(M_FATAL,
                        T_("Failed to set TLS Diffie-Hellman parameters"));
      IGNORE_DEPRECATED_ON;
      DH_free(dh);
      IGNORE_DEPRECATED_OFF;
      return {};
    }

    // SSL_CTX_set_tmp_dh creates a copy, so we need to free the parameters
    IGNORE_DEPRECATED_ON;
    DH_free(dh);
    IGNORE_DEPRECATED_OFF;
    SSL_CTX_set_options(openssl_ctx_.get(), SSL_OP_SINGLE_DH_USE);
  }

  switch (verify_peer) {
    case VerifyPeerSetting::Disabled: {
      SSL_CTX_set_verify(openssl_ctx_.get(), SSL_VERIFY_NONE, NULL);
    } break;
    case VerifyPeerSetting::IfCertificatePresented: {
      SSL_CTX_set_verify(openssl_ctx_.get(), SSL_VERIFY_PEER,
                         OpensslVerifyPeer);
    } break;
    case VerifyPeerSetting::Required: {
      // NOTE: SSL_VERIFY_FAIL_IF_NO_PEER_CERT has no effect in client mode
      //  But the verification will still fail later when we do our own check!
      SSL_CTX_set_verify(openssl_ctx_.get(),
                         SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                         OpensslVerifyPeer);
    } break;
    default: {
      Dmsg0(50, "Uknown verify peer setting %zu\n",
            static_cast<size_t>(verify_peer));
      return {};
    } break;
  }

  ssl_ptr openssl_{SSL_new(openssl_ctx_.get())};

  if (!openssl_) {
    OpensslPostErrors(M_FATAL, T_("Error creating new SSL object"));
    return {};
  }

  /* Non-blocking partial writes */
  SSL_set_mode(openssl_.get(), SSL_MODE_ENABLE_PARTIAL_WRITE
                                   | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  return openssl_;
}

std::unique_ptr<TlsOpenSsl> TlsOpenSsl::make_server(const TlsResource* res,
                                                    TlsConfigProvider* config)
{
  Dmsg0(100, "Construct TlsOpenSsl\n");

  auto openssl_ = make_ssl_from_res(res);
  if (!openssl_) { return {}; }

  // this is necessary (for some reason) to support SSL_VERIFY_PEER
  // when no client certificate is given.  Without this call on the server side,
  // the server will complain that it was not setup.
  // NOTE: do not set this on the client!  For some reason connections will fail
  //  due to a session resumption error, when this is set on the client...
  SSL_set_session_id_context(openssl_.get(), (unsigned const char*)"bareos", 6);

  if (config) {
    SSL_set_secretprovider(openssl_.get(), config);
    SSL_set_psk_server_callback(openssl_.get(), psk_server_cb);
  }

  return std::make_unique<TlsOpenSsl>(res, std::move(openssl_));
}

std::unique_ptr<TlsOpenSsl> TlsOpenSsl::make_client(const TlsResource* res,
                                                    const PskCredentials* creds)
{
  auto openssl_ = make_ssl_from_res(res);
  if (!openssl_) { return {}; }

  auto ptr = std::make_unique<TlsOpenSsl>(res, std::move(openssl_));

  if (creds) {
    BStringList ident(creds->get_identity(),
                      AsciiControlCharacters::RecordSeparator());
    Dmsg1(50, "Preparing TLS_PSK CLIENT context for identity %s\n",
          ident.JoinReadable().c_str());
    ptr->ClientContextInsertCredentials(*creds);
    SSL_set_psk_client_callback(ptr->ssl(), psk_client_cb);
  }

  return ptr;
}

void print_options(const TlsResource* res)
{
  auto& cert = res->tls_cert_;
  Dmsg1(100, "Set protocol:\t<%s>\n", res->protocol_.c_str());
  Dmsg1(100, "Set cipherlist:\t<%s>\n", res->cipherlist_.c_str());
  Dmsg1(100, "Set ciphersuites:\t<%s>\n", res->ciphersuites_.c_str());
  Dmsg1(100, "Set ca_certfile:\t<%s>\n", cert.ca_certfile_.c_str());
  Dmsg1(100, "Set ca_certdir:\t<%s>\n", cert.ca_certdir_.c_str());
  Dmsg1(100, "Set crlfile_:\t<%s>\n", cert.crlfile_.c_str());
  Dmsg1(100, "Set certfile_:\t<%s>\n", cert.certfile_.c_str());
  Dmsg1(100, "Set keyfile_:\t<%s>\n", cert.keyfile_.c_str());
  Dmsg1(100, "Set dhfile_:\t<%s>\n", cert.dhfile_.c_str());
  Dmsg1(100, "Set Verify Peer:\t<%s>\n", as_str(cert.verify_peer_).c_str());
}

auto TlsOpenSsl::IsPskIdentityInUse(std::string_view identity) const
    -> PskIdentityStatus
{
  auto* psk_used = SSL_get_psk_identity(openssl_);
  if (!psk_used) { return PskIdentityStatus::NoIdentityInUse; }
  if (identity != psk_used) {
    return PskIdentityStatus::DifferentIdentityInUse;
  }

  return PskIdentityStatus::IsInUse;
}
};  // namespace

std::unique_ptr<Tls> make_openssl_server_tls(const TlsResource* res,
                                             TlsConfigProvider* config)
{
  print_options(res);
  return TlsOpenSsl::make_server(res, config);
}
std::unique_ptr<Tls> make_openssl_client_tls(const TlsResource* res,
                                             const PskCredentials* creds)
{
  print_options(res);
  return TlsOpenSsl::make_client(res, creds);
}
