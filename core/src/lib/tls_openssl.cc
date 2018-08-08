/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2018 Bareos GmbH & Co. KG

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
 * tls_openssl.c TLS support functions when using OPENSSL backend.
 *
 * Author: Landon Fuller <landonf@threerings.net>
 */

#include "include/bareos.h"
#include "lib/crypto_openssl.h"
#include "lib/bpoll.h"
#include <assert.h>

#if defined(HAVE_TLS) && defined(HAVE_OPENSSL)

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>

#include "lib/tls_openssl.h"

#include "parse_conf.h"

/*
 * No anonymous ciphers, no <128 bit ciphers, no export ciphers, no MD5 ciphers
 */
#define TLS_DEFAULT_CIPHERS "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

static unsigned int psk_client_cb(SSL *ssl,
                                  const char * /*hint*/,
                                  char *identity,
                                  unsigned int max_identity_len,
                                  unsigned char *psk,
                                  unsigned int max_psk_len);

static unsigned int psk_server_cb(SSL *ssl,
                                  const char *identity,
                                  unsigned char *psk,
                                  unsigned int max_psk_len);

static std::map<SSL_CTX *, PskCredentials> psk_server_credentials;
static std::map<SSL_CTX *, PskCredentials> psk_client_credentials;

class TlsOpenSslPrivate
{
public:
   TlsOpenSslPrivate()
   : openssl_(nullptr)
   , openssl_ctx_(nullptr)
   , pem_callback_(nullptr)
   , pem_userdata_(nullptr)
   {
      Dmsg0(100, "Construct TlsImplementationOpenSsl\n");
   }

   ~TlsOpenSslPrivate() //FreeTlsContext
   {
      Dmsg0(100, "Destruct TlsImplementationOpenSsl\n");
      if (openssl_) {
         psk_server_credentials.erase(openssl_ctx_);
         psk_client_credentials.erase(openssl_ctx_);
         SSL_CTX_free(openssl_ctx_);
      }
      if (openssl_) {
         SSL_free(openssl_);
      }
   };

   int OpensslBsockReadwrite(BareosSocket *bsock, char *ptr, int nbytes, bool write);
   bool OpensslBsockSessionStart(BareosSocket *bsock, bool server);
   int OpensslVerifyPeer(int ok, X509_STORE_CTX *store);
   int tls_pem_callback_dispatch(char *buf, int size, int rwflag, void *userdata);

   SSL                  *openssl_;
   SSL_CTX              *openssl_ctx_;
   CRYPTO_PEM_PASSWD_CB *pem_callback_;
   const void           *pem_userdata_;
};

TlsOpenSsl::TlsOpenSsl(int fd)
   : d_(new TlsOpenSslPrivate)
{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
   d_->openssl_ctx_ = SSL_CTX_new(TLS_method());
#else
   d_->openssl_ctx_ = SSL_CTX_new(SSLv23_method());
#endif
   if (!d_->openssl_ctx_) {
      OpensslPostErrors(M_FATAL, _("Error initializing SSL context"));
      throw std::runtime_error(_("Error initializing SSL context"));
   }

   /*
    * Enable all Bug Workarounds
    */
   SSL_CTX_set_options(d_->openssl_ctx_, SSL_OP_ALL);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
   /*
    * Disallow broken sslv2 and sslv3.
    */
   SSL_CTX_set_options(d_->openssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#endif

   if (cipherlist_.empty()) {
      cipherlist_ = TLS_DEFAULT_CIPHERS;
   }

   if (SSL_CTX_set_cipher_list(d_->openssl_ctx_, cipherlist_.c_str()) != 1) {
      Jmsg0(NULL, M_ERROR, 0, _("Error setting cipher list, no valid ciphers available\n"));
      throw std::runtime_error(_("Error setting cipher list, no valid ciphers available"));
   }

   //ueb: hier das erste man den psk callback registrieren und die psk eintragen


   BIO *bio = BIO_new(BIO_s_socket()); /* free the fd manually */

   if (!bio) {
      OpensslPostErrors(M_FATAL, _("Error creating file descriptor-based BIO"));
      throw;
   }
   BIO_set_fd(bio, fd, BIO_NOCLOSE);

   d_->openssl_ = SSL_new(d_->openssl_ctx_);
   if (!d_->openssl_) {
      OpensslPostErrors(M_FATAL, _("Error creating new SSL object"));
      SSL_free(d_->openssl_);
      throw;
   }

   SSL_CTX_set_psk_client_callback(d_->openssl_ctx_, psk_client_cb);
   SSL_CTX_set_psk_server_callback(d_->openssl_ctx_, psk_server_cb);

   SSL_set_bio(d_->openssl_, bio, bio);

   /* Non-blocking partial writes */
   SSL_set_mode(d_->openssl_, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

   /* ******************* */
   return; //ueb
   /* ******************* */

   if (pem_callback_) {
      d_->pem_callback_ = pem_callback_; // ueb: use proteced data from base class
      d_->pem_userdata_ = pem_userdata_;
   } else {
      d_->pem_callback_ = CryptoDefaultPemCallback;
      d_->pem_userdata_ = NULL;
   }

//   SSL_CTX_set_default_passwd_cb(d_->openssl_ctx_, d_->tls_pem_callback_dispatch); // Ueb: static callbacks!
//   SSL_CTX_set_default_passwd_cb_userdata(d_->openssl_ctx_, reinterpret_cast<void *>(ctx.get()));

   if (!ca_certfile_.empty() || !ca_certdir_.empty()) { /* at least one should be set */
      if (!SSL_CTX_load_verify_locations(d_->openssl_ctx_, ca_certfile_.c_str(), ca_certdir_.c_str())) {
         OpensslPostErrors(M_FATAL, _("Error loading certificate verification stores"));
         throw std::runtime_error(_("Error loading certificate verification stores"));
      }
   } // else if (verify_peer_) {
      /* At least one CA is required for peer verification */
      Jmsg0(NULL, M_ERROR, 0, _("Either a certificate file or a directory must be"
                         " specified as a verification store\n"));
      throw std::runtime_error(_("Either a certificate file or a directory must be"
                                    " specified as a verification store"));
   //}

#if (OPENSSL_VERSION_NUMBER >= 0x00907000L)  && (OPENSSL_VERSION_NUMBER < 0x10100000L)
   /*
    * Set certificate revocation list.
    */
   if (crlfile) {
      X509_STORE *store;
      X509_LOOKUP *lookup;

      store = SSL_CTX_get_cert_store(d_->openssl_ctx_);
      if (!store) {
         OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
         throw std::runtime_error(_("Error loading revocation list file"));
      }

      lookup = X509_STORE_add_lookup(store, X509_LOOKUP_crl_reloader());
      if (!lookup) {
         OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
         throw std::runtime_error(_("Error loading revocation list file"));
      }

      if (!LoadNewCrlFile(lookup, (char *)crlfile)) {
         OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
         throw std::runtime_error(_("Error loading revocation list file"));
      }

      X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
   }
#endif

   /*
    * Load our certificate file, if available. This file may also contain a
    * private key, though this usage is somewhat unusual.
    */
   if (!certfile_.empty()) {
      if (!SSL_CTX_use_certificate_chain_file(d_->openssl_ctx_, certfile_.c_str())) {
         OpensslPostErrors(M_FATAL, _("Error loading certificate file"));
         throw std::runtime_error(_("Error loading certificate file"));
      }
   }

   if (!keyfile_.empty()) {
      if (!SSL_CTX_use_PrivateKey_file(d_->openssl_ctx_, keyfile_.c_str(), SSL_FILETYPE_PEM)) {
         OpensslPostErrors(M_FATAL, _("Error loading private key"));
         throw std::runtime_error(_("Error loading private key"));
      }
   }

   if (!dhfile_.empty()) { /* Diffie-Hellman parameters */
      if (!(bio = BIO_new_file(dhfile_.c_str(), "r"))) {
         OpensslPostErrors(M_FATAL, _("Unable to open DH parameters file"));
         throw std::runtime_error(_("Unable to open DH parameters file"));
      }
//      dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL); Ueb: bio richtig initialisieren
//      BIO_free(bio);
//      if (!dh) {
//         OpensslPostErrors(M_FATAL, _("Unable to load DH parameters from specified file"));
//         throw std::runtime_error(_("Unable to load DH parameters from specified file"));
//      }
//      if (!SSL_CTX_set_tmp_dh(d_->openssl_ctx_, dh)) {
//         OpensslPostErrors(M_FATAL, _("Failed to set TLS Diffie-Hellman parameters"));
//         DH_free(dh);
//         throw std::runtime_error(_("Failed to set TLS Diffie-Hellman parameters"));
//      }

      SSL_CTX_set_options(d_->openssl_ctx_, SSL_OP_SINGLE_DH_USE);
   }

   if (verify_peer_) {
      /*
       * SSL_VERIFY_FAIL_IF_NO_PEER_CERT has no effect in client mode
       */
//      SSL_CTX_set_verify(d_->openssl_ctx_,
//                         SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
//                         d_->OpensslVerifyPeer);
   } else {
      SSL_CTX_set_verify(d_->openssl_ctx_,
                         SSL_VERIFY_NONE,
                         NULL);
   }
}

TlsOpenSsl::~TlsOpenSsl()
{
   Dmsg0(100, "Destroy TLsOpenSsl Implementation Object\n");
}

/*
 * OpenSSL certificate verification callback.
 * OpenSSL has already performed internal certificate verification.
 * We just report any errors that occured.
 */
int OpensslVerifyPeer(int ok, X509_STORE_CTX *store)
{
   if (!ok) {
      X509 *cert = X509_STORE_CTX_get_current_cert(store);
      int depth = X509_STORE_CTX_get_error_depth(store);
      int err = X509_STORE_CTX_get_error(store);
      char issuer[256];
      char subject[256];

      X509_NAME_oneline(X509_get_issuer_name(cert), issuer, 256);
      X509_NAME_oneline(X509_get_subject_name(cert), subject, 256);

      Jmsg5(NULL, M_ERROR, 0, _("Error with certificate at depth: %d, issuer = %s,"
            " subject = %s, ERR=%d:%s\n"), depth, issuer,
              subject, err, X509_verify_cert_error_string(err));

   }

   return ok;
}

int TlsOpenSslPrivate::tls_pem_callback_dispatch(char *buf, int size, int rwflag,
                                     void *userdata)
{
   return (pem_callback_(buf, size, pem_userdata_));
}

static unsigned int psk_server_cb(SSL *ssl,
                                  const char *identity,
                                  unsigned char *psk,
                                  unsigned int max_psk_len)
{
   unsigned int result = 0;

   SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
   Dmsg1(100, "psk_server_cb. identitiy: %s.\n", identity);

   if (ctx) {
      try {
         const PskCredentials &credentials = psk_server_credentials.at(ctx);
         /* okay. we found the appropriate psk identity pair.
          * Now let's check if the given identity is the same and
          * provide the psk.
          */
         if (credentials.get_identity() == std::string(identity)) {
            int psklen = Bsnprintf((char *)psk, max_psk_len, "%s", credentials.get_psk().c_str());
            result = (psklen < 0) ? 0 : psklen;
            Dmsg1(100, "psk_server_cb. psk: %s.\n", psk);
        }
         return result;
      } catch (const std::out_of_range & /* exception */) {
         // ssl context unknown
         Dmsg0(100, "Error, TLS-PSK credentials not found.\n");
         return 0;
      }
   }
   Dmsg0(100, "Error, SSL_CTX not set.\n");
   return result;
}

static unsigned int psk_client_cb(SSL *ssl,
                                  const char * /*hint*/,
                                  char *identity,
                                  unsigned int max_identity_len,
                                  unsigned char *psk,
                                  unsigned int max_psk_len)
{

   SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);

   if (ctx) {
      try {
         const PskCredentials &credentials = psk_client_credentials.at(ctx);
         /* okay. we found the appropriate psk identity pair.
          * Now let's check if the given identity is the same and
          * provide the psk.
          */
         int ret =
             Bsnprintf(identity, max_identity_len, "%s", credentials.get_identity().c_str());

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
         Dmsg1(100, "psk_client_cb. psk: %s.\n", psk);

         return ret;
      } catch (const std::out_of_range &exception) {
         // ssl context unknown
         Dmsg0(100, "Error, TLS-PSK CALLBACK not set.\n");
         return 0;
      }
   }

   Dmsg0(100, "Error, SSL_CTX not set.\n");
   return 0;
}

void TlsOpenSsl::SetTlsPskClientContext(const char *cipherlist, const PskCredentials &credentials)
{
   Dmsg1(50, "Preparing TLS_PSK client context for identity %s\n", credentials.get_identity().c_str());

   /* credentials auf Gültigkeit prüfen!! */
   if (d_->openssl_ctx_) {
      psk_client_credentials[d_->openssl_ctx_] = credentials;
      SSL_CTX_set_psk_client_callback(d_->openssl_ctx_, psk_client_cb);
   }
}

void TlsOpenSsl::SetTlsPskServerContext(const char *cipherlist, const PskCredentials &credentials)
{
   Dmsg1(50, "Preparing TLS_PSK server context for identity %s\n", credentials.get_identity().c_str());

   /* credentials auf Gültigkeit prüfen!! */
   psk_server_credentials[d_->openssl_ctx_] = credentials;
   SSL_CTX_set_psk_server_callback(d_->openssl_ctx_, psk_server_cb);
}

std::string TlsOpenSsl::TlsCipherGetName() const
{
   if (!d_->openssl_) {
      return std::string();
   }

   const SSL_CIPHER *cipher = SSL_get_current_cipher(d_->openssl_);

   if (cipher) {
      return std::string(SSL_CIPHER_get_name(cipher));
   }

   return std::string();
}

void TlsOpenSsl::TlsLogConninfo(JobControlRecord *jcr, const char *host, int port, const char *who) const
{
   if (!d_->openssl_) {
      Qmsg(jcr, M_INFO, 0, _("No openssl to %s at %s:%d established\n"), who, host, port);
   } else {
      std::string cipher_name = TlsCipherGetName();
      if (!cipher_name.empty()) {
         Qmsg(jcr, M_INFO, 0, _("Secure connection to %s at %s:%d with cipher %s established\n"), who, host, port, cipher_name.c_str());
      } else {
         Qmsg(jcr, M_WARNING, 0, _("Secure connection to %s at %s:%d with UNKNOWN cipher established\n"), who, host, port);
      }
   }
}

/*
 * Verifies a list of common names against the certificate commonName attribute.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsOpenSsl::TlsPostconnectVerifyCn(JobControlRecord *jcr, alist *verify_list)
{
   X509 *cert;
   X509_NAME *subject;
   bool auth_success = false;
   char data[256];

   if (!(cert = SSL_get_peer_certificate(d_->openssl_))) {
      Qmsg0(jcr, M_ERROR, 0, _("Peer failed to present a TLS certificate\n"));
      return false;
   }

   if ((subject = X509_get_subject_name(cert)) != NULL) {
      if (X509_NAME_get_text_by_NID(subject, NID_commonName, data, sizeof(data)) > 0) {
         char *cn;
         data[255] = 0; /* NULL Terminate data */

         foreach_alist(cn, verify_list) {
            Dmsg2(120, "comparing CNs: cert-cn=%s, allowed-cn=%s\n", data, cn);
            if (Bstrcasecmp(data, cn)) {
               auth_success = true;
            }
         }
      }
   }

   X509_free(cert);
   return auth_success;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName attributes.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsOpenSsl::TlsPostconnectVerifyHost(JobControlRecord *jcr, const char *host)
{
   int i, j;
   int extensions;
   int cnLastPos = -1;
   X509 *cert;
   X509_NAME *subject;
   X509_NAME_ENTRY *neCN;
   ASN1_STRING *asn1CN;
   bool auth_success = false;

   if (!(cert = SSL_get_peer_certificate(d_->openssl_))) {
      Qmsg1(jcr, M_ERROR, 0,
            _("Peer %s failed to present a TLS certificate\n"), host);
      return false;
   }

   /*
    * Check subjectAltName extensions first
    */
   if ((extensions = X509_get_ext_count(cert)) > 0) {
      for (i = 0; i < extensions; i++) {
         X509_EXTENSION *ext;
         const char *extname;

         ext = X509_get_ext(cert, i);
         extname = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

         if (bstrcmp(extname, "subjectAltName")) {
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
            const X509V3_EXT_METHOD *method;
#else
            X509V3_EXT_METHOD *method;
#endif
            STACK_OF(CONF_VALUE) *val;
            CONF_VALUE *nval;
            void *extstr = NULL;
#if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
            const unsigned char *ext_value_data;
#else
            unsigned char *ext_value_data;
#endif

            if (!(method = X509V3_EXT_get(ext))) {
               break;
            }

            ext_value_data = X509_EXTENSION_get_data(ext)->data;

#if (OPENSSL_VERSION_NUMBER > 0x00907000L)
            if (method->it) {
               extstr = ASN1_item_d2i(NULL, &ext_value_data, X509_EXTENSION_get_data(ext)->length,
                                      ASN1_ITEM_ptr(method->it));
            } else {
               /*
                * Old style ASN1
                * Decode ASN1 item in data
                */
               extstr = method->d2i(NULL, &ext_value_data, X509_EXTENSION_get_data(ext)->length);
            }

#else
            extstr = method->d2i(NULL, &ext_value_data, ext->value->length);
#endif

            /*
             * Iterate through to find the dNSName field(s)
             */
            val = method->i2v(method, extstr, NULL);

            for (j = 0; j < sk_CONF_VALUE_num(val); j++) {
               nval = sk_CONF_VALUE_value(val, j);
               if (bstrcmp(nval->name, "DNS")) {
                  if (Bstrcasecmp(nval->value, host)) {
                     auth_success = true;
                     goto success;
                  }
               }
            }
         }
      }
   }

   /*
    * Try verifying against the subject name
    */
   if (!auth_success) {
      if ((subject = X509_get_subject_name(cert)) != NULL) {
         /*
          * Loop through all CNs
          */
         for (;;) {
            cnLastPos = X509_NAME_get_index_by_NID(subject, NID_commonName, cnLastPos);
            if (cnLastPos == -1) {
               break;
            }
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

bool TlsOpenSslPrivate::OpensslBsockSessionStart(BareosSocket *bsock, bool server)
{
   int err;
   int flags;
   bool status = true;

   /* Ensure that socket is non-blocking */
   flags = bsock->SetNonblocking();

   /* start timer */
   bsock->timer_start = watchdog_time;
   bsock->ClearTimedOut();
   bsock->SetKillable(false);

   for (;;) {
      if (server) {
         err = SSL_accept(openssl_);
      } else {
         err = SSL_connect(openssl_);
      }

      /* Handle errors */
      switch (SSL_get_error(openssl_, err)) {
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

bool TlsOpenSsl::TlsBsockConnect(BareosSocket *bsock)
{
   return d_->OpensslBsockSessionStart(bsock, false);
}

bool TlsOpenSsl::TlsBsockAccept(BareosSocket *bsock)
{
   return d_->OpensslBsockSessionStart(bsock, true);
}

void TlsOpenSsl::TlsBsockShutdown(BareosSocket *bsock)
{
   /*
    * SSL_shutdown must be called twice to fully complete the process -
    * The first time to initiate the shutdown handshake, and the second to
    * receive the peer's reply.
    *
    * In addition, if the underlying socket is blocking, SSL_shutdown()
    * will not return until the current stage of the shutdown process has
    * completed or an error has occurred. By setting the socket blocking
    * we can avoid the ugly for()/switch()/select() loop.
    */
   int err;

   btimer_t *tid;

   /* Set socket blocking for shutdown */
   bsock->SetBlocking();

   tid = StartBsockTimer(bsock, 60 * 2);
   err = SSL_shutdown(d_->openssl_);
   StopBsockTimer(tid);
   if (err == 0) {
      /* Complete shutdown */
      tid = StartBsockTimer(bsock, 60 * 2);
      err = SSL_shutdown(d_->openssl_);
      StopBsockTimer(tid);
   }

   switch (SSL_get_error(d_->openssl_, err)) {
   case SSL_ERROR_NONE:
      break;
   case SSL_ERROR_ZERO_RETURN:
      /* TLS connection was shut down on us via a TLS protocol-level closure */
      OpensslPostErrors(bsock->get_jcr(), M_ERROR, _("TLS shutdown failure."));
      break;
   default:
      /* Socket Error Occurred */
      OpensslPostErrors(bsock->get_jcr(), M_ERROR, _("TLS shutdown failure."));
      break;
   }
}

int TlsOpenSslPrivate::OpensslBsockReadwrite(BareosSocket *bsock, char *ptr, int nbytes, bool write)
{
   int flags;
   int nleft = 0;
   int nwritten = 0;

   /* Ensure that socket is non-blocking */
   flags = bsock->SetNonblocking();

   /* start timer */
   bsock->timer_start = watchdog_time;
   bsock->ClearTimedOut();
   bsock->SetKillable(false);

   nleft = nbytes;

   while (nleft > 0) {
      if (write) {
         nwritten = SSL_write(openssl_, ptr, nleft);
      } else {
         nwritten = SSL_read(openssl_, ptr, nleft);
      }

      /* Handle errors */
      switch (SSL_get_error(openssl_, nwritten)) {
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

int TlsOpenSsl::TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
   return d_->OpensslBsockReadwrite(bsock, ptr, nbytes, true);
}

int TlsOpenSsl::TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
   return d_->OpensslBsockReadwrite(bsock, ptr, nbytes, false);
}

#endif /* HAVE_TLS  && HAVE_OPENSSL */
