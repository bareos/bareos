/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "tls_openssl.h"
#include "tls_openssl_private.h"

#include "lib/bpoll.h"
#include "lib/crypto_openssl.h"
#include "lib/tls_openssl_crl.h"

#include "lib/parse_conf.h"
#include "lib/get_tls_psk_by_fqname_callback.h"
#include "lib/bstringlist.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

/* static private */
std::map<const SSL_CTX *, PskCredentials> TlsOpenSslPrivate::psk_client_credentials_;

/* static private */
/* No anonymous ciphers, no <128 bit ciphers, no export ciphers, no MD5 ciphers */
const std::string TlsOpenSslPrivate::tls_default_ciphers_ {"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"};


TlsOpenSslPrivate::TlsOpenSslPrivate()
    : openssl_(nullptr)
    , openssl_ctx_(nullptr)
    , tcp_file_descriptor_(0)
    , pem_callback_(nullptr)
    , pem_userdata_(nullptr)
    , verify_peer_ (false)
{
  Dmsg0(100, "Construct TlsOpenSslPrivate\n");
}

TlsOpenSslPrivate::~TlsOpenSslPrivate()
{
  Dmsg0(100, "Destruct TlsOpenSslPrivate\n");

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
    psk_client_credentials_.erase(openssl_ctx_);
    SSL_CTX_free(openssl_ctx_);
    openssl_ctx_ = nullptr;
  }
};

bool TlsOpenSslPrivate::init()
{
  if (!openssl_ctx_) {
    OpensslPostErrors(M_FATAL, _("Error initializing TlsOpenSsl (no SSL_CTX)\n"));
    return false;
  }

  if (cipherlist_.empty()) {
    cipherlist_ = tls_default_ciphers_;
  }

  if (SSL_CTX_set_cipher_list(openssl_ctx_, cipherlist_.c_str()) != 1) {
    Dmsg0(100, _("Error setting cipher list, no valid ciphers available\n"));
    return false;
  }

  if (pem_callback_ == nullptr) {
    pem_callback_ = CryptoDefaultPemCallback;
    pem_userdata_ = NULL;
  }

  SSL_CTX_set_default_passwd_cb(openssl_ctx_, TlsOpenSslPrivate::tls_pem_callback_dispatch);
  SSL_CTX_set_default_passwd_cb_userdata(openssl_ctx_, reinterpret_cast<void *>(this));

  const char *ca_certfile = ca_certfile_.empty() ? nullptr : ca_certfile_.c_str();
  const char *ca_certdir  = ca_certdir_.empty() ? nullptr : ca_certdir_.c_str();

  if (ca_certfile || ca_certdir) { /* at least one should be set */
    if (!SSL_CTX_load_verify_locations(openssl_ctx_, ca_certfile, ca_certdir)) {
      OpensslPostErrors(M_FATAL, _("Error loading certificate verification stores"));
      return false;
    }
  } else if (verify_peer_) {
    /* At least one CA is required for peer verification */
    Dmsg0(100, _("Either a certificate file or a directory must be"
                 " specified as a verification store\n"));
  }

#if (OPENSSL_VERSION_NUMBER >= 0x00907000L) && (OPENSSL_VERSION_NUMBER < 0x10100000L)
  if (!crlfile_.empty()) {
    if (!SetCertificateRevocationList(crlfile_, openssl_ctx_)) {
      return false;
    }
  }
#endif

  if (!certfile_.empty()) {
    if (!SSL_CTX_use_certificate_chain_file(openssl_ctx_, certfile_.c_str())) {
      OpensslPostErrors(M_FATAL, _("Error loading certificate file"));
      return false;
    }
  }

  if (!keyfile_.empty()) {
    if (!SSL_CTX_use_PrivateKey_file(openssl_ctx_, keyfile_.c_str(), SSL_FILETYPE_PEM)) {
      OpensslPostErrors(M_FATAL, _("Error loading private key"));
      return false;
    }
  }

  if (!dhfile_.empty()) { /* Diffie-Hellman parameters */
    BIO *bio;
    if (!(bio = BIO_new_file(dhfile_.c_str(), "r"))) {
      OpensslPostErrors(M_FATAL, _("Unable to open DH parameters file"));
      return false;
    }
    DH *dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);  // Ueb: Init BIO correctly?
    BIO_free(bio);
    if (!dh) {
      OpensslPostErrors(M_FATAL, _("Unable to load DH parameters from specified file"));
      return false;
    }
    if (!SSL_CTX_set_tmp_dh(openssl_ctx_, dh)) {
      OpensslPostErrors(M_FATAL, _("Failed to set TLS Diffie-Hellman parameters"));
      DH_free(dh);
      return false;
    }

    SSL_CTX_set_options(openssl_ctx_, SSL_OP_SINGLE_DH_USE);
  }

  if (verify_peer_) {
    /*
     * SSL_VERIFY_FAIL_IF_NO_PEER_CERT has no effect in client mode
     */
    SSL_CTX_set_verify(openssl_ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       TlsOpenSslPrivate::OpensslVerifyPeer);
  } else {
    SSL_CTX_set_verify(openssl_ctx_, SSL_VERIFY_NONE, NULL);
  }

  openssl_ = SSL_new(openssl_ctx_);
  if (!openssl_) {
    OpensslPostErrors(M_FATAL, _("Error creating new SSL object"));
    return false;
  }

  /* Non-blocking partial writes */
  SSL_set_mode(openssl_, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  BIO *bio = BIO_new(BIO_s_socket());
  if (!bio) {
    OpensslPostErrors(M_FATAL, _("Error creating file descriptor-based BIO"));
    return false;
  }

  ASSERT(tcp_file_descriptor_);
  BIO_set_fd(bio, tcp_file_descriptor_, BIO_NOCLOSE);

  SSL_set_bio(openssl_, bio, bio);

  return true;
}

/*
 * report any errors that occured
 */
int TlsOpenSslPrivate::OpensslVerifyPeer(int preverify_ok, X509_STORE_CTX *store)
{ /* static */
  if (!preverify_ok) {
    X509 *cert = X509_STORE_CTX_get_current_cert(store);
    int depth  = X509_STORE_CTX_get_error_depth(store);
    int err    = X509_STORE_CTX_get_error(store);
    char issuer[256];
    char subject[256];

    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, 256);
    X509_NAME_oneline(X509_get_subject_name(cert), subject, 256);

    Jmsg5(NULL, M_ERROR, 0,
          _("Error with certificate at depth: %d, issuer = %s,"
            " subject = %s, ERR=%d:%s\n"),
          depth, issuer, subject, err, X509_verify_cert_error_string(err));
  }

  return preverify_ok;
}

int TlsOpenSslPrivate::OpensslBsockReadwrite(BareosSocket *bsock, char *ptr, int nbytes, bool write)
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
    switch (ssl_error) {
      case SSL_ERROR_NONE:
        nleft -= nwritten;
        if (nleft) {
          ptr += nwritten;
        }
        break;
      case SSL_ERROR_SYSCALL:
        if (nwritten == -1) {
          if (errno == EINTR) {
            continue;
          }
          if (errno == EAGAIN) {
            Bmicrosleep(0, 20000); /* try again in 20 ms */
            continue;
          }
        }
        OpensslPostErrors(bsock->get_jcr(), M_FATAL, _("TLS read/write failure."));
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
        /* Socket Error Occured */
        OpensslPostErrors(bsock->get_jcr(), M_FATAL, _("TLS read/write failure."));
        goto cleanup;
    }

    /* Everything done? */
    if (nleft == 0) {
      goto cleanup;
    }

    /* Timeout/Termination, let's take what we can get */
    if (bsock->IsTimedOut() || bsock->IsTerminated()) {
      goto cleanup;
    }
  }

cleanup:
  /* Restore saved flags */
  bsock->RestoreBlocking(flags);

  /* Clear timer */
  bsock->timer_start = 0;
  bsock->SetKillable(true);

  return nbytes - nleft;
}

bool TlsOpenSslPrivate::OpensslBsockSessionStart(BareosSocket *bsock, bool server)
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
    switch (ssl_error) {
      case SSL_ERROR_NONE:
        bsock->SetTlsEstablished();
        status = true;
        goto cleanup;
      case SSL_ERROR_ZERO_RETURN:
        /* TLS connection was cleanly shut down */
        OpensslPostErrors(bsock->get_jcr(), M_FATAL, _("Connect failure"));
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
        OpensslPostErrors(bsock->get_jcr(), M_FATAL, _("Connect failure"));
        status = false;
        goto cleanup;
    }

    if (bsock->IsTimedOut()) {
      goto cleanup;
    }
  }

cleanup:
  /* Restore saved flags */
  bsock->RestoreBlocking(flags);
  /* Clear timer */
  bsock->timer_start = 0;
  bsock->SetKillable(true);

  return status;
}

int TlsOpenSslPrivate::tls_pem_callback_dispatch(char *buf, int size, int rwflag, void *userdata)
{
  TlsOpenSslPrivate *p = reinterpret_cast<TlsOpenSslPrivate *>(userdata);
  return (p->pem_callback_(buf, size, p->pem_userdata_));
}

void TlsOpenSslPrivate::ClientContextInsertCredentials(const PskCredentials &credentials)
{
  if (!openssl_ctx_) { /* do not register nullptr */
    Dmsg0(100, "Psk Server Callback: No SSL_CTX\n");
  } else {
  TlsOpenSslPrivate::psk_client_credentials_.insert(
      std::pair<const SSL_CTX *, PskCredentials>(openssl_ctx_, credentials));
  }
}

unsigned int TlsOpenSslPrivate::psk_server_cb(SSL *ssl,
                                              const char *identity,
                                              unsigned char *psk_output,
                                              unsigned int max_psk_len)
{
  unsigned int result = 0;

  SSL_CTX *openssl_ctx = SSL_get_SSL_CTX(ssl);

  if (!openssl_ctx) {
      Dmsg0(100, "Psk Server Callback: No SSL_CTX\n");
      return result;
  }
  BStringList lst(std::string(identity),AsciiControlCharacters::RecordSeparator());
  Dmsg1(100, "psk_server_cb. identitiy: %s.\n", lst.JoinReadable().c_str());

  std::string configured_psk;
  GetTlsPskByFullyQualifiedResourceNameCb_t GetTlsPskByFullyQualifiedResourceNameCb =
      reinterpret_cast<GetTlsPskByFullyQualifiedResourceNameCb_t>(SSL_CTX_get_ex_data(
          openssl_ctx, TlsOpenSslPrivate::SslCtxExDataIndex::kGetTlsPskByFullyQualifiedResourceNameCb));

  if (!GetTlsPskByFullyQualifiedResourceNameCb) {
    Dmsg0(100, "Callback not set: kGetTlsPskByFullyQualifiedResourceNameCb\n");
    return result;
  }

  ConfigurationParser *config  = reinterpret_cast<ConfigurationParser*>(SSL_CTX_get_ex_data(
          openssl_ctx, TlsOpenSslPrivate::SslCtxExDataIndex::kConfigurationParserPtr));

  if (!config) {
    Dmsg0(100, "Config not set: kConfigurationParserPtr\n");
      return result;
    }

  if (!GetTlsPskByFullyQualifiedResourceNameCb(config, identity, configured_psk)) {
    Dmsg0(100, "Error, TLS-PSK credentials not found.\n");
  } else {
      int psklen = Bsnprintf((char *)psk_output, max_psk_len, "%s", configured_psk.c_str());
      result = (psklen < 0) ? 0 : psklen;
      Dmsg1(100, "psk_server_cb. result: %d.\n", result);
  }
  return result;
}

unsigned int TlsOpenSslPrivate::psk_client_cb(SSL *ssl,
                                              const char * /*hint*/,
                                              char *identity,
                                              unsigned int max_identity_len,
                                              unsigned char *psk,
                                              unsigned int max_psk_len)
{
  const SSL_CTX *openssl_ctx = SSL_get_SSL_CTX(ssl);

  if (!openssl_ctx) {
      Dmsg0(100, "Psk Client Callback: No SSL_CTX\n");
      return 0;
  }

  if (psk_client_credentials_.find(openssl_ctx) == psk_client_credentials_.end()) {
    Dmsg0(100, "Error, TLS-PSK CALLBACK not set because SSL_CTX is not registered.\n");
  } else {
    const PskCredentials &credentials = TlsOpenSslPrivate::psk_client_credentials_.at(openssl_ctx);
      int ret = Bsnprintf(identity, max_identity_len, "%s", credentials.get_identity().c_str());

      if (ret < 0 || (unsigned int)ret > max_identity_len) {
        Dmsg0(100, "Error, identify too long\n");
        return 0;
      }
      Dmsg1(100, "psk_client_cb. identity: %s.\n", identity);

      ret = Bsnprintf((char *)psk, max_psk_len, "%s", credentials.get_psk().c_str());
      if (ret < 0 || (unsigned int)ret > max_psk_len) {
        Dmsg0(100, "Error, psk too long\n");
        return 0;
      }
      return ret;
    }
  return 0;
}

/*
 * public interfaces from TlsOpenSsl that set private data
 */
void TlsOpenSsl::SetCaCertfile(const std::string &ca_certfile)
{
  Dmsg1(100, "Set ca_certfile:\t<%s>\n", ca_certfile.c_str());
  d_->ca_certfile_ = ca_certfile;
}

void TlsOpenSsl::SetCaCertdir(const std::string &ca_certdir)
{
  Dmsg1(100, "Set ca_certdir:\t<%s>\n", ca_certdir.c_str());
  d_->ca_certdir_ = ca_certdir;
}

void TlsOpenSsl::SetCrlfile(const std::string &crlfile)
{
  Dmsg1(100, "Set crlfile:\t<%s>\n", crlfile.c_str());
  d_->crlfile_ = crlfile;
}

void TlsOpenSsl::SetCertfile(const std::string &certfile)
{
  Dmsg1(100, "Set certfile:\t<%s>\n", certfile.c_str());
  d_->certfile_ = certfile;
}

void TlsOpenSsl::SetKeyfile(const std::string &keyfile)
{
  Dmsg1(100, "Set keyfile:\t<%s>\n", keyfile.c_str());
  d_->keyfile_ = keyfile;
}

void TlsOpenSsl::SetPemCallback(CRYPTO_PEM_PASSWD_CB pem_callback)
{
  Dmsg1(100, "Set pem_callback to address: <%#x>\n", reinterpret_cast<uint64_t>(pem_callback));
  d_->pem_callback_ = pem_callback;
}

void TlsOpenSsl::SetPemUserdata(void *pem_userdata)
{
  Dmsg1(100, "Set pem_userdata to address: <%#x>\n", reinterpret_cast<uint64_t>(pem_userdata));
  d_->pem_userdata_ = pem_userdata;
}

void TlsOpenSsl::SetDhFile(const std::string &dhfile)
{
  Dmsg1(100, "Set dhfile:\t<%s>\n", dhfile.c_str());
  d_->dhfile_ = dhfile;
}

void TlsOpenSsl::SetVerifyPeer(const bool &verify_peer)
{
  Dmsg1(100, "Set Verify Peer:\t<%s>\n", verify_peer ? "true" : "false");
  d_->verify_peer_ = verify_peer;
}

void TlsOpenSsl::SetTcpFileDescriptor(const int &fd)
{
  Dmsg1(100, "Set tcp filedescriptor: <%d>\n", fd);
  d_->tcp_file_descriptor_ = fd;
}

void TlsOpenSsl::SetCipherList(const std::string &cipherlist)
{
  Dmsg1(100, "Set cipherlist:\t<%s>\n", cipherlist.c_str());
  d_->cipherlist_ = cipherlist;
}
