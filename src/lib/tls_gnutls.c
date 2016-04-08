/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * tls_gnutls.c TLS support functions when using GNUTLS backend.
 *
 * Author: Marco van Wieringen <marco.van.wieringen@bareos.com>
 */

#include "bareos.h"
#include <assert.h>

#if defined(HAVE_TLS) && defined(HAVE_GNUTLS)

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#define TLS_DEFAULT_CIPHERS "NONE:+VERS-TLS1.0:+CIPHER-ALL:+COMP-ALL:+RSA:+DHE-RSA:+DHE-DSS:+MAC-ALL"

#define DH_BITS 1024

/* TLS Context Structure */
struct TLS_Context {
   gnutls_dh_params dh_params;
   gnutls_certificate_client_credentials gnutls_cred;

   CRYPTO_PEM_PASSWD_CB *pem_callback;
   const char *cipher_list;
   const void *pem_userdata;
   unsigned char *dhdata;
   bool verify_peer;
   bool tls_enable;
   bool tls_require;
};

struct TLS_Connection {
   TLS_Context *ctx;
   gnutls_session_t gnutls_state;
};

static inline bool load_dhfile_data(TLS_CONTEXT *ctx, const char *dhfile)
{
   FILE *fp;
   int error;
   size_t size;
   struct stat st;
   gnutls_datum_t dhparms;

   /*
    * Load the content of the file into memory.
    */
   if (stat(dhfile, &st) < 0) {
      return false;
   }

   if ((fp = fopen(dhfile, "r")) == (FILE *)NULL) {
      return false;
   }

   /*
    * Allocate a memory buffer to hold the DH data.
    */
   ctx->dhdata = (unsigned char *)malloc(st.st_size + 1);

   size = fread(ctx->dhdata, sizeof(ctx->dhdata), 1, fp);

   fclose(fp);

   dhparms.data = ctx->dhdata;
   dhparms.size = size;

   error = gnutls_dh_params_import_pkcs3(ctx->dh_params, &dhparms, GNUTLS_X509_FMT_PEM);
   if (error != GNUTLS_E_SUCCESS) {
      return false;
   }

   return true;
}

/*
 * Create a new TLS_CONTEXT instance.
 *  Returns: Pointer to TLS_CONTEXT instance on success
 *           NULL on failure;
 */
TLS_CONTEXT *new_tls_context(const char *ca_certfile,
                             const char *ca_certdir,
                             const char *crlfile,
                             const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata,
                             const char *dhfile,
                             const char *cipherlist,
                             bool verify_peer)
{
   int error;
   TLS_CONTEXT *ctx;

   ctx = (TLS_CONTEXT *)malloc(sizeof(TLS_CONTEXT));
   memset(ctx, 0, sizeof(TLS_CONTEXT));

   ctx->pem_callback = pem_callback;
   ctx->pem_userdata = pem_userdata;
   ctx->cipher_list = cipherlist;
   ctx->verify_peer = verify_peer;

   error = gnutls_certificate_allocate_credentials(&ctx->gnutls_cred);
   if (error != GNUTLS_E_SUCCESS) {
      Jmsg1(NULL, M_ERROR, 0,
            _("Failed to create a new GNUTLS certificate credential: ERR=%s\n"),
            gnutls_strerror(error));
      free(ctx);
      return NULL;
   }

   /*
    * GNUTLS supports only a certfile not a certdir.
    */
   if (ca_certdir && !ca_certfile) {
      Jmsg0(NULL, M_ERROR, 0,
            _("GNUTLS doesn't support certdir use certfile instead\n"));
      goto bail_out;
   }

   /*
    * Try to load the trust file, first in PEM format and if that fails in DER format.
    */
   if (ca_certfile) {
      error = gnutls_certificate_set_x509_trust_file(ctx->gnutls_cred,
                                                     ca_certfile,
                                                     GNUTLS_X509_FMT_PEM);
      if (error < GNUTLS_E_SUCCESS) {
         error = gnutls_certificate_set_x509_trust_file(ctx->gnutls_cred,
                                                        ca_certfile,
                                                        GNUTLS_X509_FMT_DER);
         if (error < GNUTLS_E_SUCCESS) {
            Jmsg1(NULL, M_ERROR, 0,
                  _("Error loading CA certificates from %s\n"),
                  ca_certfile);
            goto bail_out;
         }
      }
   } else if (verify_peer) {
      /*
       * At least one CA is required for peer verification
       */
      Jmsg0(NULL, M_ERROR, 0,
            _("Certificate file must be specified as a verification store\n"));
      goto bail_out;
   }

   /*
    * Try to load the revocation list file, first in PEM format and if that fails in DER format.
    */
   if (crlfile) {
      error = gnutls_certificate_set_x509_crl_file(ctx->gnutls_cred,
                                                   crlfile,
                                                   GNUTLS_X509_FMT_PEM);
      if (error < GNUTLS_E_SUCCESS) {
         error = gnutls_certificate_set_x509_crl_file(ctx->gnutls_cred,
                                                      crlfile,
                                                      GNUTLS_X509_FMT_DER);
         if (error < GNUTLS_E_SUCCESS) {
            Jmsg1(NULL, M_ERROR, 0,
                  _("Error loading certificate revocation list from %s\n"),
                  crlfile);
            goto bail_out;
         }
      }
   }

   /*
    * Try to load the certificate and the keyfile, first in PEM format and if that fails in DER format.
    */
   if (certfile && keyfile) {
      error = gnutls_certificate_set_x509_key_file(ctx->gnutls_cred,
                                                    certfile,
                                                    keyfile,
                                                    GNUTLS_X509_FMT_PEM);
      if (error != GNUTLS_E_SUCCESS) {
         error = gnutls_certificate_set_x509_key_file(ctx->gnutls_cred,
                                                       certfile,
                                                       keyfile,
                                                       GNUTLS_X509_FMT_DER);
         if (error != GNUTLS_E_SUCCESS) {
            Jmsg2(NULL, M_ERROR, 0,
                  _("Error loading key from %s or certificate from %s\n"),
                  keyfile, certfile);
            goto bail_out;
         }
      }
   }

   error = gnutls_dh_params_init(&ctx->dh_params);
   if (error != GNUTLS_E_SUCCESS) {
      goto bail_out;
   }

   if (dhfile) {
      if (!load_dhfile_data(ctx, dhfile)) {
         Jmsg1(NULL, M_ERROR, 0,
               _("Failed to load DH file %s\n"),
               dhfile);
         goto bail_out;
      }
   } else {
      error = gnutls_dh_params_generate2(ctx->dh_params, DH_BITS);
      if (error != GNUTLS_E_SUCCESS) {
         Jmsg0(NULL, M_ERROR, 0,
               _("Failed to generate new DH parameters\n"));
         goto bail_out;
      }
   }

   /*
    * Link the dh params and the credentials.
    */
   gnutls_certificate_set_dh_params(ctx->gnutls_cred, ctx->dh_params);

   return ctx;

bail_out:
   free_tls_context(ctx);
   return NULL;
}

void free_tls_context(TLS_CONTEXT *ctx)
{
   gnutls_certificate_free_credentials(ctx->gnutls_cred);

   if (ctx->dhdata) {
      free(ctx->dhdata);
   }

   free(ctx);
}

bool get_tls_require(TLS_CONTEXT *ctx)
{
   return (ctx) ? ctx->tls_require : false;
}

void set_tls_require(TLS_CONTEXT *ctx, bool value)
{
   if (ctx) {
      ctx->tls_require = value;
   }
}

bool get_tls_enable(TLS_CONTEXT *ctx)
{
   return (ctx) ? ctx->tls_enable : false;
}

void set_tls_enable(TLS_CONTEXT *ctx, bool value)
{
   if (ctx) {
      ctx->tls_enable = value;
   }
}

bool get_tls_verify_peer(TLS_CONTEXT *ctx)
{
   return (ctx) ? ctx->verify_peer : false;
}

/*
 * Certs are not automatically verified during the handshake.
 */
static inline bool tls_cert_verify(TLS_CONNECTION *tls_conn)
{
   unsigned int status = 0;
   int error;
   time_t now = time(NULL);
   time_t peertime;

   error = gnutls_certificate_verify_peers2(tls_conn->gnutls_state, &status);
   if (error != GNUTLS_E_SUCCESS) {
      Jmsg1(NULL, M_ERROR, 0,
            _("gnutls_certificate_verify_peers2 failed: ERR=%s\n"),
            gnutls_strerror(error));
      return false;
   }

   if (status) {
      Jmsg1(NULL, M_ERROR, 0,
            _("peer certificate untrusted or revoked (0x%x)\n"),
            status);
      return false;
   }

   peertime = gnutls_certificate_expiration_time_peers(tls_conn->gnutls_state);
   if (peertime == -1) {
      Jmsg0(NULL, M_ERROR, 0, _("gnutls_certificate_expiration_time_peers failed\n"));
      return false;
   }
   if (peertime < now) {
      Jmsg0(NULL, M_ERROR, 0, _("peer certificate is expired\n"));
      return false;
   }

   peertime = gnutls_certificate_activation_time_peers(tls_conn->gnutls_state);
   if (peertime == -1) {
      Jmsg0(NULL, M_ERROR, 0, _("gnutls_certificate_activation_time_peers failed\n"));
      return false;
   }
   if (peertime > now) {
      Jmsg0(NULL, M_ERROR, 0, _("peer certificate not yet active\n"));
      return false;
   }

   return true;
}

/*
 * Verifies a list of common names against the certificate commonName attribute.
 *
 * Returns: true on success
 *          false on failure
 */
bool tls_postconnect_verify_cn(JCR *jcr, TLS_CONNECTION *tls_conn, alist *verify_list)
{
   char *cn;
   int error, cnt;
   unsigned int list_size;
   size_t cn_length;
   char cannonicalname[256];
   bool auth_success = false;
   gnutls_x509_crt_t cert;
   const gnutls_datum_t *peer_cert_list;

   /*
    * See if we verify the peer certificate.
    */
   if (!tls_conn->ctx->verify_peer) {
      return true;
   }

   peer_cert_list = gnutls_certificate_get_peers(tls_conn->gnutls_state, &list_size);
   if (!peer_cert_list) {
      return false;
   }

   error = gnutls_x509_crt_init(&cert);
   if (error != GNUTLS_E_SUCCESS) {
      return false;
   }

   gnutls_x509_crt_import(cert, peer_cert_list, GNUTLS_X509_FMT_DER);
   if (error != GNUTLS_E_SUCCESS) {
      gnutls_x509_crt_deinit(cert);
      return false;
   }

   /*
    * See what CN's are available.
    */
   error = 0;
   for (cnt = 0; error >= 0; cnt++) {
      cn_length = sizeof(cannonicalname);
      error = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, cnt, 0,
                                            cannonicalname, &cn_length);
      if (error < 0) {
         break;
      }

      /*
       * NULL terminate data.
       */
      cannonicalname[255] = '\0';


      /*
       * Try all the CNs in the list
       */
      foreach_alist(cn, verify_list) {
         Dmsg2(120, "comparing CNs: cert-cn=%s, allowed-cn=%s\n", cannonicalname, cn);
         if (bstrcasecmp(cn, cannonicalname)) {
            auth_success = true;
            break;
         }
      }

      if (auth_success) {
         break;
      }
   }

   gnutls_x509_crt_deinit(cert);

   return auth_success;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName attributes.
 *
 * Returns: true on success
 *          false on failure
 */
bool tls_postconnect_verify_host(JCR *jcr, TLS_CONNECTION *tls_conn, const char *host)
{
   int error;
   unsigned int list_size;
   gnutls_x509_crt_t cert;
   const gnutls_datum_t *peer_cert_list;

   /*
    * See if we verify the peer certificate.
    */
   if (!tls_conn->ctx->verify_peer) {
      return true;
   }

   peer_cert_list = gnutls_certificate_get_peers(tls_conn->gnutls_state, &list_size);
   if (!peer_cert_list) {
      return false;
   }

   error = gnutls_x509_crt_init(&cert);
   if (error != GNUTLS_E_SUCCESS) {
      return false;
   }

   error = gnutls_x509_crt_import(cert, peer_cert_list, GNUTLS_X509_FMT_DER);
   if (error != GNUTLS_E_SUCCESS) {
      gnutls_x509_crt_deinit(cert);
      return false;
   }

   if (!gnutls_x509_crt_check_hostname(cert, host)) {
      gnutls_x509_crt_deinit(cert);
      return false;
   }

   gnutls_x509_crt_deinit(cert);
   return true;
}

/*
 * Create a new TLS_CONNECTION instance.
 *
 * Returns: Pointer to TLS_CONNECTION instance on success
 *          NULL on failure;
 */
TLS_CONNECTION *new_tls_connection(TLS_CONTEXT *ctx, int fd, bool server)
{
   TLS_CONNECTION *tls_conn;
   int error;

   /*
    * Allocate our new tls connection
    */
   tls_conn = (TLS_CONNECTION *)malloc(sizeof(TLS_CONNECTION));
   memset(tls_conn, 0, sizeof(TLS_CONNECTION));

   /*
    * Link the TLS context and the TLS session.
    */
   tls_conn->ctx = ctx;

   if (server) {
      error = gnutls_init(&tls_conn->gnutls_state, GNUTLS_SERVER);
   } else {
      error = gnutls_init(&tls_conn->gnutls_state, GNUTLS_CLIENT);
   }

   if (error != GNUTLS_E_SUCCESS) {
      Jmsg1(NULL, M_ERROR, 0,
            _("Failed to create a new GNUTLS session: ERR=%s\n"),
            gnutls_strerror(error));
      goto bail_out;
   }

   /*
    * Set the default ciphers to use for the TLS connection.
    */
   if (ctx->cipher_list) {
      gnutls_priority_set_direct(tls_conn->gnutls_state, ctx->cipher_list, NULL);
   } else {
      gnutls_priority_set_direct(tls_conn->gnutls_state, TLS_DEFAULT_CIPHERS, NULL);
   }

   /*
    * Link the credentials and the session.
    */
   gnutls_credentials_set(tls_conn->gnutls_state, GNUTLS_CRD_CERTIFICATE, ctx->gnutls_cred);

   /*
    * Link the TLS session and the filedescriptor of the socket used.
    * gnutls_transport_set_ptr may cause problems on some platforms,
    * therefore the replacement gnutls_transport_set_int is used,
    * when available (since GnuTLS >= 3.1.9)
    */
#ifdef HAVE_GNUTLS_TRANSPORT_SET_INT
   gnutls_transport_set_int(tls_conn->gnutls_state, fd);
#else
   gnutls_transport_set_ptr(tls_conn->gnutls_state, (gnutls_transport_ptr_t)fd);
#endif

   /*
    * Server specific settings.
    */
   if (server) {
      /*
       * See if we require the other party to have a certificate too.
       */
      if (ctx->tls_require) {
         gnutls_certificate_server_set_request(tls_conn->gnutls_state, GNUTLS_CERT_REQUIRE);
      } else {
         gnutls_certificate_server_set_request(tls_conn->gnutls_state, GNUTLS_CERT_REQUEST);
      }

      gnutls_dh_set_prime_bits(tls_conn->gnutls_state, DH_BITS);
   }

   return tls_conn;

bail_out:
   free(tls_conn);
   return NULL;
}

void free_tls_connection(TLS_CONNECTION *tls_conn)
{
   gnutls_deinit(tls_conn->gnutls_state);
   free(tls_conn);
}

static inline bool gnutls_bsock_session_start(BSOCK *bsock, bool server)
{
   int flags, error;
   bool status = true;
   bool done = false;
   unsigned int list_size;
   TLS_CONNECTION *tls_conn = bsock->tls_conn;
   const gnutls_datum_t *peer_cert_list;

   /* Ensure that socket is non-blocking */
   flags = bsock->set_nonblocking();

   /* start timer */
   bsock->timer_start = watchdog_time;
   bsock->clear_timed_out();
   bsock->set_killable(false);

   while (!done) {
      error = gnutls_handshake(tls_conn->gnutls_state);

      switch (error) {
      case GNUTLS_E_SUCCESS:
         status = true;
         done = true;
         break;
      case GNUTLS_E_AGAIN:
      case GNUTLS_E_INTERRUPTED:
         if (gnutls_record_get_direction(tls_conn->gnutls_state) == 1) {
            wait_for_writable_fd(bsock->m_fd, 10000, false);
         } else {
            wait_for_readable_fd(bsock->m_fd, 10000, false);
         }
         status = true;
         continue;
      default:
         status = false;
         goto cleanup;
      }

      if (bsock->is_timed_out()) {
         goto cleanup;
      }

      /*
       * See if we need to verify the peer.
       */
      peer_cert_list = gnutls_certificate_get_peers(tls_conn->gnutls_state, &list_size);
      if (!peer_cert_list && !tls_conn->ctx->tls_require) {
         goto cleanup;
      }

      if (tls_conn->ctx->verify_peer) {
         if (!tls_cert_verify(tls_conn)) {
            status = false;
            goto cleanup;
         }
      }
   }

cleanup:
   /* Restore saved flags */
   bsock->restore_blocking(flags);

   /* Clear timer */
   bsock->timer_start = 0;
   bsock->set_killable(true);

   return status;
}

/*
 * Initiates a TLS connection with the server.
 *  Returns: true on success
 *           false on failure
 */
bool tls_bsock_connect(BSOCK *bsock)
{
   return gnutls_bsock_session_start(bsock, false);
}

/*
 * Listens for a TLS connection from a client.
 *  Returns: true on success
 *           false on failure
 */
bool tls_bsock_accept(BSOCK *bsock)
{
   return gnutls_bsock_session_start(bsock, true);
}

void tls_bsock_shutdown(BSOCK *bsock)
{
   TLS_CONNECTION *tls_conn = bsock->tls_conn;

   gnutls_bye(tls_conn->gnutls_state, GNUTLS_SHUT_WR);
}

/* Does all the manual labor for tls_bsock_readn() and tls_bsock_writen() */
static inline int gnutls_bsock_readwrite(BSOCK *bsock, char *ptr, int nbytes, bool write)
{
   TLS_CONNECTION *tls_conn = bsock->tls_conn;
   int error;
   int flags;
   int nleft = 0;
   int nwritten = 0;

   /* Ensure that socket is non-blocking */
   flags = bsock->set_nonblocking();

   /* start timer */
   bsock->timer_start = watchdog_time;
   bsock->clear_timed_out();
   bsock->set_killable(false);

   nleft = nbytes;

   while (nleft > 0) {
      if (write) {
         nwritten = gnutls_record_send(tls_conn->gnutls_state, ptr, nleft);
      } else {
         nwritten = gnutls_record_recv(tls_conn->gnutls_state, ptr, nleft);
      }

      /* Handle errors */
      if (nwritten > 0) {
         nleft -= nwritten;
         if (nleft) {
            ptr += nwritten;
         }
      } else {
         switch (nwritten) {
         case GNUTLS_E_REHANDSHAKE:
            /*
             * TLS renegotiation requested.
             */
            error = gnutls_handshake(tls_conn->gnutls_state);
            if (error != GNUTLS_E_SUCCESS) {
               goto cleanup;
            }
            break;
         case GNUTLS_E_AGAIN:
         case GNUTLS_E_INTERRUPTED:
            if (gnutls_record_get_direction(tls_conn->gnutls_state) == 1) {
               wait_for_writable_fd(bsock->m_fd, 10000, false);
            } else {
               wait_for_readable_fd(bsock->m_fd, 10000, false);
            }
            break;
         default:
            goto cleanup;
         }
      }

      /* Everything done? */
      if (nleft == 0) {
         goto cleanup;
      }

      /* Timeout/Termination, let's take what we can get */
      if (bsock->is_timed_out() || bsock->is_terminated()) {
         goto cleanup;
      }
   }

cleanup:
   /* Restore saved flags */
   bsock->restore_blocking(flags);

   /* Clear timer */
   bsock->timer_start = 0;
   bsock->set_killable(true);

   return nbytes - nleft;
}

int tls_bsock_writen(BSOCK *bsock, char *ptr, int32_t nbytes)
{
   return gnutls_bsock_readwrite(bsock, ptr, nbytes, true);
}

int tls_bsock_readn(BSOCK *bsock, char *ptr, int32_t nbytes)
{
   return gnutls_bsock_readwrite(bsock, ptr, nbytes, false);
}
#endif /* HAVE_TLS && HAVE_GNUTLS */
