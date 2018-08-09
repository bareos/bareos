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
#include "lib/tls_openssl_private.h"

#include "parse_conf.h"

/*
 * No anonymous ciphers, no <128 bit ciphers, no export ciphers, no MD5 ciphers
 */
#define TLS_DEFAULT_CIPHERS "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

TlsOpenSsl::TlsOpenSsl()
   : d_(new TlsOpenSslPrivate)
{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
   d_->openssl_ctx_ = SSL_CTX_new(TLS_method());
#else
   d_->openssl_ctx_ = SSL_CTX_new(SSLv23_method());
#endif

   if (!d_->openssl_ctx_) {
      OpensslPostErrors(M_FATAL, _("Error initializing SSL context"));
      return;
   }

   SSL_CTX_set_options(d_->openssl_ctx_, SSL_OP_ALL);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
   SSL_CTX_set_options(d_->openssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#endif
}

TlsOpenSsl::~TlsOpenSsl()
{
   Dmsg0(100, "Destroy TLsOpenSsl Implementation Object\n");
}

bool TlsOpenSsl::init()
{
   if (d_->cipherlist_.empty()) {
      d_->cipherlist_ = TLS_DEFAULT_CIPHERS;
   }

   if (SSL_CTX_set_cipher_list(d_->openssl_ctx_, d_->cipherlist_.c_str()) != 1) {
      Jmsg0(NULL, M_ERROR, 0, _("Error setting cipher list, no valid ciphers available\n"));
      return false;
   }

   if (d_->pem_callback_) {
      d_->pem_userdata_ = d_->pem_userdata_;
   } else {
      d_->pem_callback_ = CryptoDefaultPemCallback;
      d_->pem_userdata_ = NULL;
   }

   SSL_CTX_set_default_passwd_cb(d_->openssl_ctx_, TlsOpenSslPrivate::tls_pem_callback_dispatch);
   SSL_CTX_set_default_passwd_cb_userdata(d_->openssl_ctx_, reinterpret_cast<void *>(d_.get()));

   const char *ca_certfile = d_->ca_certfile_.empty() ? nullptr : d_->ca_certfile_.c_str();
   const char *ca_certdir = d_->ca_certdir_.empty() ? nullptr : d_->ca_certfile_.c_str();

   if (ca_certfile || ca_certdir) { /* at least one should be set */
      if (!SSL_CTX_load_verify_locations(d_->openssl_ctx_, ca_certfile, ca_certdir)) {
         OpensslPostErrors(M_FATAL, _("Error loading certificate verification stores"));
         return false;
      }
   } else if (d_->verify_peer_) {
      /* At least one CA is required for peer verification */
      Jmsg0(NULL, M_ERROR, 0, _("Either a certificate file or a directory must be"
                         " specified as a verification store\n"));
      return false;
   }

#if (OPENSSL_VERSION_NUMBER >= 0x00907000L)  && (OPENSSL_VERSION_NUMBER < 0x10100000L)
   /*
    * Set certificate revocation list.
    */
   if (!d_->crlfile_.empty()) {
      X509_STORE *store;
      X509_LOOKUP *lookup;

      store = SSL_CTX_get_cert_store(d_->openssl_ctx_);
      if (!store) {
         OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
         return false;
      }

      lookup = X509_STORE_add_lookup(store, X509_LOOKUP_crl_reloader());
      if (!lookup) {
         OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
         return false;
      }

      if (!LoadNewCrlFile(lookup, (char *)d_->crlfile_.c_str())) {
         OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
         return false;
      }

      X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
   }
#endif

   if (!d_->certfile_.empty()) {
      if (!SSL_CTX_use_certificate_chain_file(d_->openssl_ctx_, d_->certfile_.c_str())) {
         OpensslPostErrors(M_FATAL, _("Error loading certificate file"));
         return false;
      }
   }

   if (!d_->keyfile_.empty()) {
      if (!SSL_CTX_use_PrivateKey_file(d_->openssl_ctx_, d_->keyfile_.c_str(), SSL_FILETYPE_PEM)) {
         OpensslPostErrors(M_FATAL, _("Error loading private key"));
         return false;
      }
   }

   if (!d_->dhfile_.empty()) { /* Diffie-Hellman parameters */
      BIO *bio;
      if (!(bio = BIO_new_file(d_->dhfile_.c_str(), "r"))) {
         OpensslPostErrors(M_FATAL, _("Unable to open DH parameters file"));
         return false;
      }
      DH *dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL); // Ueb: bio richtig initialisieren
      BIO_free(bio);
      if (!dh) {
         OpensslPostErrors(M_FATAL, _("Unable to load DH parameters from specified file"));
         return false;
      }
      if (!SSL_CTX_set_tmp_dh(d_->openssl_ctx_, dh)) {
         OpensslPostErrors(M_FATAL, _("Failed to set TLS Diffie-Hellman parameters"));
         DH_free(dh);
         return false;
      }

      SSL_CTX_set_options(d_->openssl_ctx_, SSL_OP_SINGLE_DH_USE);
   }

   if (d_->verify_peer_) {
      /*
       * SSL_VERIFY_FAIL_IF_NO_PEER_CERT has no effect in client mode
       */
      SSL_CTX_set_verify(d_->openssl_ctx_,
                         SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                         TlsOpenSslPrivate::OpensslVerifyPeer);
   } else {
      SSL_CTX_set_verify(d_->openssl_ctx_,
                         SSL_VERIFY_NONE,
                         NULL);
   }

   d_->openssl_ = SSL_new(d_->openssl_ctx_);
   if (!d_->openssl_) {
      OpensslPostErrors(M_FATAL, _("Error creating new SSL object"));
      return false;
   }

   /* Non-blocking partial writes */
   SSL_set_mode(d_->openssl_, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

   BIO *bio = BIO_new(BIO_s_socket());
   if (!bio) {
      OpensslPostErrors(M_FATAL, _("Error creating file descriptor-based BIO"));
      return false;
   }

   ASSERT(d_->tcp_file_descriptor_);
   BIO_set_fd(bio, d_->tcp_file_descriptor_, BIO_NOCLOSE);

   SSL_set_bio(d_->openssl_, bio, bio);

   return true;
}

void TlsOpenSsl::SetTlsPskClientContext(const PskCredentials &credentials)
{
   Dmsg1(50, "Preparing TLS_PSK CLIENT context for identity %s\n", credentials.get_identity().c_str());

   if (d_->openssl_ctx_) {
      d_->ClientContextInsertCredentials(credentials);
      SSL_CTX_set_psk_client_callback(d_->openssl_ctx_, TlsOpenSslPrivate::psk_client_cb);
   }
}

void TlsOpenSsl::SetTlsPskServerContext(const PskCredentials &credentials)
{
   Dmsg1(50, "Preparing TLS_PSK SERVER context for identity %s\n", credentials.get_identity().c_str());

   if (d_->openssl_ctx_) {
      d_->ServerContextInsertCredentials(credentials);
      SSL_CTX_set_psk_server_callback(d_->openssl_ctx_, TlsOpenSslPrivate::psk_server_cb);
   }
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

int TlsOpenSsl::TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
   return d_->OpensslBsockReadwrite(bsock, ptr, nbytes, true);
}

int TlsOpenSsl::TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
   return d_->OpensslBsockReadwrite(bsock, ptr, nbytes, false);
}

#endif /* HAVE_TLS  && HAVE_OPENSSL */
