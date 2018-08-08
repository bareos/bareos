/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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

#include "tls_openssl_private.h"
#include "tls_openssl.h"

#include "lib/crypto_openssl.h"
#include "lib/bpoll.h"

#include <bareos.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

std::map<const SSL_CTX *, PskCredentials> TlsOpenSslPrivate::psk_server_credentials;
std::map<const SSL_CTX *, PskCredentials> TlsOpenSslPrivate::psk_client_credentials;

TlsOpenSslPrivate::TlsOpenSslPrivate()
  : openssl_(nullptr)
  , openssl_ctx_(nullptr)
  , pem_callback_(nullptr)
  , pem_userdata_(nullptr)
{
   Dmsg0(100, "Construct TlsImplementationOpenSsl\n");
}

TlsOpenSslPrivate::~TlsOpenSslPrivate() //FreeTlsContext
{
   Dmsg0(100, "Destruct TlsImplementationOpenSsl\n");
   if (openssl_ctx_) {
      psk_server_credentials.erase(openssl_ctx_);
      psk_client_credentials.erase(openssl_ctx_);
      SSL_CTX_free(openssl_ctx_);
   }
   if (openssl_) {
      SSL_free(openssl_);
   }
};

/*
 * report any errors that occured
 */
int TlsOpenSslPrivate::OpensslVerifyPeer(int ok, X509_STORE_CTX *store)
{  /* static */
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

int TlsOpenSslPrivate::tls_pem_callback_dispatch(char *buf, int size, int rwflag,
                                     void *userdata)
{
   TlsOpenSslPrivate *p = reinterpret_cast<TlsOpenSslPrivate*>(userdata);
   return (p->pem_callback_(buf, size, p->pem_userdata_));
}

void TlsOpenSslPrivate::ClientContextInsertCredentials(const PskCredentials &credentials)
{
   TlsOpenSslPrivate::psk_client_credentials.insert(
               std::pair<const SSL_CTX*, PskCredentials>(openssl_ctx_,credentials));
}

void TlsOpenSslPrivate::ServerContextInsertCredentials(const PskCredentials &credentials)
{
   TlsOpenSslPrivate::psk_server_credentials.insert(
               std::pair<const SSL_CTX *, PskCredentials>(openssl_ctx_,credentials));
}

unsigned int TlsOpenSslPrivate::psk_server_cb(SSL *ssl,
                                  const char *identity,
                                  unsigned char *psk,
                                  unsigned int max_psk_len)
{
   unsigned int result = 0;

   SSL_CTX *openssl_ctx = SSL_get_SSL_CTX(ssl);
   Dmsg1(100, "psk_server_cb. identitiy: %s.\n", identity);

   if (openssl_ctx) {
      if (psk_server_credentials.find(openssl_ctx) != psk_server_credentials.end()) {
         const PskCredentials &credentials = psk_server_credentials.at(openssl_ctx);

         if (credentials.get_identity() == std::string(identity)) {
            int psklen = Bsnprintf((char *)psk, max_psk_len, "%s", credentials.get_psk().c_str());
            Dmsg1(100, "psk_server_cb. psk: %s.\n", psk);
            result = (psklen < 0) ? 0 : psklen;
         }
      } else {
         Dmsg0(100, "Error, TLS-PSK credentials not found.\n");
      }
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

   if (openssl_ctx) {
      if (psk_client_credentials.find(openssl_ctx) != psk_client_credentials.end()) {
         const PskCredentials &credentials = TlsOpenSslPrivate::psk_client_credentials.at(openssl_ctx);
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
      } else {
         Dmsg0(100, "Error, TLS-PSK CALLBACK not set.\n");
         return 0;
      }
   }

   Dmsg0(100, "Error, SSL_CTX not set.\n");
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

void TlsOpenSsl::SetTcpFileDescriptor(const int& fd)
{
   Dmsg1(100, "Set tcp filedescriptor: <%d>\n", fd);
   d_->tcp_file_descriptor_ = fd ;
}

void TlsOpenSsl::SetCipherList(const std::string &cipherlist)
{
   Dmsg1(100, "Set cipherlist:\t<%s>\n", cipherlist.c_str());
   d_->cipherlist_ = cipherlist;
}
