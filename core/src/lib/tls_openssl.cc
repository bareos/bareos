/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2023 Bareos GmbH & Co. KG

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

#include <assert.h>
#include "include/bareos.h"
#include "lib/bpoll.h"
#include "lib/crypto_openssl.h"

#if defined(HAVE_TLS) && defined(HAVE_OPENSSL)

#  include <openssl/asn1.h>
#  include <openssl/asn1t.h>
#  include <openssl/err.h>
#  include <openssl/ssl.h>
#  include <openssl/x509v3.h>

#  include "lib/bsock.h"
#  include "lib/tls_openssl.h"
#  include "lib/tls_openssl_private.h"
#  include "lib/bstringlist.h"
#  include "lib/ascii_control_characters.h"
#  include "include/jcr.h"

#  include "parse_conf.h"

TlsOpenSsl::TlsOpenSsl() : d_(std::make_unique<TlsOpenSslPrivate>()) {}

TlsOpenSsl::~TlsOpenSsl() = default;

bool TlsOpenSsl::init() { return d_->init(); }

void TlsOpenSsl::SetTlsPskClientContext(const PskCredentials& credentials)
{
  if (!d_->openssl_ctx_) {
    Dmsg0(50, "Could not set TLS_PSK CLIENT context (no SSL_CTX)\n");
  } else {
    BStringList ident(credentials.get_identity(),
                      AsciiControlCharacters::RecordSeparator());
    Dmsg1(50, "Preparing TLS_PSK CLIENT context for identity %s\n",
          ident.JoinReadable().c_str());
    d_->ClientContextInsertCredentials(credentials);
    SSL_CTX_set_psk_client_callback(d_->openssl_ctx_,
                                    TlsOpenSslPrivate::psk_client_cb);
  }
}

void TlsOpenSsl::SetTlsPskServerContext(ConfigurationParser* config)
{
  if (!d_->openssl_ctx_) {
    Dmsg0(50, "Could not prepare TLS_PSK SERVER callback (no SSL_CTX)\n");
  } else if (!config) {
    Dmsg0(50, "Could not prepare TLS_PSK SERVER callback (no config)\n");
  } else {
    // keep a shared_ptr to the current config, so a reload won't
    // free the memory we're going to use in the private context
    d_->config_table_ = config->GetResourcesContainer();
    SSL_CTX_set_ex_data(
        d_->openssl_ctx_,
        TlsOpenSslPrivate::SslCtxExDataIndex::kConfigurationParserPtr,
        (void*)config);

    SSL_CTX_set_psk_server_callback(d_->openssl_ctx_,
                                    TlsOpenSslPrivate::psk_server_cb);
  }
}

std::string TlsOpenSsl::TlsCipherGetName() const
{
  if (d_->openssl_) {
    const SSL_CIPHER* cipher = SSL_get_current_cipher(d_->openssl_);
    const char* protocol_name = SSL_get_version(d_->openssl_);
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
  if (!d_->openssl_) {
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

  if (!(cert = SSL_get_peer_certificate(d_->openssl_))) {
    Qmsg0(jcr, M_ERROR, 0, T_("Peer failed to present a TLS certificate\n"));
    return false;
  }

  if ((subject = X509_get_subject_name(cert)) != NULL) {
    char data[256]; /* nullterminated by X509_NAME_get_text_by_NID */
    if (X509_NAME_get_text_by_NID(subject, NID_commonName, data, sizeof(data))
        > 0) {
      std::string cn;
      for (const std::string& cn : verify_list) {
        std::string d(data);
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

  if (!(cert = SSL_get_peer_certificate(d_->openssl_))) {
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
#  if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
        const X509V3_EXT_METHOD* method;
#  else
        X509V3_EXT_METHOD* method;
#  endif
        STACK_OF(CONF_VALUE) * val;
        CONF_VALUE* nval;
        void* extstr = NULL;
#  if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
        const unsigned char* ext_value_data;
#  else
        unsigned char* ext_value_data;
#  endif

        if (!(method = X509V3_EXT_get(ext))) { break; }

        ext_value_data = X509_EXTENSION_get_data(ext)->data;

#  if (OPENSSL_VERSION_NUMBER > 0x00907000L)
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

#  else
        extstr = method->d2i(NULL, &ext_value_data, ext->value->length);
#  endif

        // Iterate through to find the dNSName field(s)
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
  return d_->OpensslBsockSessionStart(bsock, false);
}

bool TlsOpenSsl::TlsBsockAccept(BareosSocket* bsock)
{
  return d_->OpensslBsockSessionStart(bsock, true);
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

  if (!d_->openssl_) { return; }

  /* Set socket blocking for shutdown */
  bsock->SetBlocking();

  btimer_t* tid = StartBsockTimer(bsock, 60 * 2);

  int err_shutdown = SSL_shutdown(d_->openssl_);

  StopBsockTimer(tid);

  if (err_shutdown == 0) {
    /* Complete the shutdown with the second call */
    tid = StartBsockTimer(bsock, 2);
    err_shutdown = SSL_shutdown(d_->openssl_);
    StopBsockTimer(tid);
  }

  int ssl_error = SSL_get_error(d_->openssl_, err_shutdown);

  /* There may be more errors on the thread-local error-queue.
   * As we just shutdown our context and looked at the errors that we were
   * interested in we clear the queue so nobody else gets to read an error
   * that may have occured here. */
  ERR_clear_error();  // empties the current thread's openssl error queue

  SSL_free(d_->openssl_);
  d_->openssl_ = nullptr;


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
  return d_->OpensslBsockReadwrite(bsock, ptr, nbytes, true);
}

int TlsOpenSsl::TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes)
{
  return d_->OpensslBsockReadwrite(bsock, ptr, nbytes, false);
}
bool TlsOpenSsl::KtlsSendStatus() { return d_->KtlsSendStatus(); }

bool TlsOpenSsl::KtlsRecvStatus() { return d_->KtlsRecvStatus(); }

#endif /* HAVE_TLS  && HAVE_OPENSSL */
