/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * tls.c TLS support functions
 *
 * Author: Landon Fuller <landonf@threerings.net>
 *
 * This file was contributed to the Bacula project by Landon Fuller
 * and Three Rings Design, Inc.
 *
 * Three Rings Design, Inc. has been granted a perpetual, worldwide,
 * non-exclusive, no-charge, royalty-free, irrevocable copyright
 * license to reproduce, prepare derivative works of, publicly
 * display, publicly perform, sublicense, and distribute the original
 * work contributed by Three Rings Design, Inc. and its employees to
 * the Bacula project in source or object form.
 *
 * If you wish to license contributions from Three Rings Design, Inc,
 * under an alternate open source license please contact
 * Landon Fuller <landonf@threerings.net>.
 */


#include "bacula.h"
#include <assert.h>


#ifdef HAVE_TLS /* Is TLS enabled? */

#ifdef HAVE_OPENSSL /* How about OpenSSL? */

/* No anonymous ciphers, no <128 bit ciphers, no export ciphers, no MD5 ciphers */
#define TLS_DEFAULT_CIPHERS "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

/* TLS Context Structure */
struct TLS_Context {
   SSL_CTX *openssl;
   CRYPTO_PEM_PASSWD_CB *pem_callback;
   const void *pem_userdata;
   bool tls_enable;
   bool tls_require;
};

struct TLS_Connection {
   SSL *openssl;
};

/*
 * OpenSSL certificate verification callback.
 * OpenSSL has already performed internal certificate verification.
 * We just report any errors that occured.
 */
static int openssl_verify_peer(int ok, X509_STORE_CTX *store)
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

/* Dispatch user PEM encryption callbacks */
static int tls_pem_callback_dispatch (char *buf, int size, int rwflag, void *userdata)
{
   TLS_CONTEXT *ctx = (TLS_CONTEXT *)userdata;
   return (ctx->pem_callback(buf, size, ctx->pem_userdata));
}

/*
 * Create a new TLS_CONTEXT instance.
 *  Returns: Pointer to TLS_CONTEXT instance on success
 *           NULL on failure;
 */
TLS_CONTEXT *new_tls_context(const char *ca_certfile, const char *ca_certdir,
                             const char *certfile, const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata, const char *dhfile,
                             bool verify_peer)
{
   TLS_CONTEXT *ctx;
   BIO *bio;
   DH *dh;

   ctx = (TLS_CONTEXT *)malloc(sizeof(TLS_CONTEXT));

   /* Allocate our OpenSSL TLSv1 Context */
   ctx->openssl = SSL_CTX_new(TLSv1_method());

   if (!ctx->openssl) {
      openssl_post_errors(M_FATAL, _("Error initializing SSL context"));
      goto err;
   }

   /* Set up pem encryption callback */
   if (pem_callback) {
      ctx->pem_callback = pem_callback;
      ctx->pem_userdata = pem_userdata;
   } else {
      ctx->pem_callback = crypto_default_pem_callback;
      ctx->pem_userdata = NULL;
   }
   SSL_CTX_set_default_passwd_cb(ctx->openssl, tls_pem_callback_dispatch);
   SSL_CTX_set_default_passwd_cb_userdata(ctx->openssl, (void *) ctx);

   /*
    * Set certificate verification paths. This requires that at least one
    * value be non-NULL
    */
   if (ca_certfile || ca_certdir) {
      if (!SSL_CTX_load_verify_locations(ctx->openssl, ca_certfile, ca_certdir)) {
         openssl_post_errors(M_FATAL, _("Error loading certificate verification stores"));
         goto err;
      }
   } else if (verify_peer) {
      /* At least one CA is required for peer verification */
      Jmsg0(NULL, M_ERROR, 0, _("Either a certificate file or a directory must be"
                         " specified as a verification store\n"));
      goto err;
   }

   /*
    * Load our certificate file, if available. This file may also contain a
    * private key, though this usage is somewhat unusual.
    */
   if (certfile) {
      if (!SSL_CTX_use_certificate_chain_file(ctx->openssl, certfile)) {
         openssl_post_errors(M_FATAL, _("Error loading certificate file"));
         goto err;
      }
   }

   /* Load our private key. */
   if (keyfile) {
      if (!SSL_CTX_use_PrivateKey_file(ctx->openssl, keyfile, SSL_FILETYPE_PEM)) {
         openssl_post_errors(M_FATAL, _("Error loading private key"));
         goto err;
      }
   }

   /* Load Diffie-Hellman Parameters. */
   if (dhfile) {
      if (!(bio = BIO_new_file(dhfile, "r"))) {
         openssl_post_errors(M_FATAL, _("Unable to open DH parameters file"));
         goto err;
      }
      dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
      BIO_free(bio);
      if (!dh) {
         openssl_post_errors(M_FATAL, _("Unable to load DH parameters from specified file"));
         goto err;
      }
      if (!SSL_CTX_set_tmp_dh(ctx->openssl, dh)) {
         openssl_post_errors(M_FATAL, _("Failed to set TLS Diffie-Hellman parameters"));
         DH_free(dh);
         goto err;
      }
      /* Enable Single-Use DH for Ephemeral Keying */
      SSL_CTX_set_options(ctx->openssl, SSL_OP_SINGLE_DH_USE);
   }

   if (SSL_CTX_set_cipher_list(ctx->openssl, TLS_DEFAULT_CIPHERS) != 1) {
      Jmsg0(NULL, M_ERROR, 0,
             _("Error setting cipher list, no valid ciphers available\n"));
      goto err;
   }

   /* Verify Peer Certificate */
   if (verify_peer) {
      /* SSL_VERIFY_FAIL_IF_NO_PEER_CERT has no effect in client mode */
      SSL_CTX_set_verify(ctx->openssl,
                         SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                         openssl_verify_peer);
   }

   return ctx;

err:
   /* Clean up after ourselves */
   if(ctx->openssl) {
      SSL_CTX_free(ctx->openssl);
   }
   free(ctx);
   return NULL;
}

/*
 * Free TLS_CONTEXT instance
 */
void free_tls_context(TLS_CONTEXT *ctx)
{
   SSL_CTX_free(ctx->openssl);
   free(ctx);
}

bool get_tls_require(TLS_CONTEXT *ctx) 
{
   return ctx->tls_require;
}

bool get_tls_enable(TLS_CONTEXT *ctx) 
{
   return ctx->tls_enable;
}


/*
 * Verifies a list of common names against the certificate
 * commonName attribute.
 *  Returns: true on success
 *           false on failure
 */
bool tls_postconnect_verify_cn(JCR *jcr, TLS_CONNECTION *tls, alist *verify_list)
{
   SSL *ssl = tls->openssl;
   X509 *cert;
   X509_NAME *subject;
   bool auth_success = false;
   char data[256];

   /* Check if peer provided a certificate */
   if (!(cert = SSL_get_peer_certificate(ssl))) {
      Qmsg0(jcr, M_ERROR, 0, _("Peer failed to present a TLS certificate\n"));
      return false;
   }

   if ((subject = X509_get_subject_name(cert)) != NULL) {
      if (X509_NAME_get_text_by_NID(subject, NID_commonName, data, sizeof(data)) > 0) {
         char *cn;
         /* NULL terminate data */
         data[255] = 0;

         /* Try all the CNs in the list */
         foreach_alist(cn, verify_list) {
            if (strcasecmp(data, cn) == 0) {
               auth_success = true;
            }
         }
      }
   }

   X509_free(cert);
   return auth_success;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName
 * attributes.
 *  Returns: true on success
 *           false on failure
 */
bool tls_postconnect_verify_host(JCR *jcr, TLS_CONNECTION *tls, const char *host)
{
   SSL *ssl = tls->openssl;
   X509 *cert;
   X509_NAME *subject;
   bool auth_success = false;
   int extensions;
   int i, j;

   int cnLastPos = -1;
   X509_NAME_ENTRY *neCN;
   ASN1_STRING *asn1CN;

   /* Check if peer provided a certificate */
   if (!(cert = SSL_get_peer_certificate(ssl))) {
      Qmsg1(jcr, M_ERROR, 0, 
            _("Peer %s failed to present a TLS certificate\n"), host);
      return false;
   }

   /* Check subjectAltName extensions first */
   if ((extensions = X509_get_ext_count(cert)) > 0) {
      for (i = 0; i < extensions; i++) {
         X509_EXTENSION *ext;
         const char *extname;

         ext = X509_get_ext(cert, i);
         extname = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

         if (strcmp(extname, "subjectAltName") == 0) {
#ifdef HAVE_OPENSSLv1
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

            /* Get x509 extension method structure */
            if (!(method = X509V3_EXT_get(ext))) {
               break;
            }

            ext_value_data = ext->value->data;

#if (OPENSSL_VERSION_NUMBER > 0x00907000L)
            if (method->it) {
               /* New style ASN1 */

               /* Decode ASN1 item in data */
               extstr = ASN1_item_d2i(NULL, &ext_value_data, ext->value->length,
                                      ASN1_ITEM_ptr(method->it));
            } else {
               /* Old style ASN1 */

               /* Decode ASN1 item in data */
               extstr = method->d2i(NULL, &ext_value_data, ext->value->length);
            }

#else
            extstr = method->d2i(NULL, &ext_value_data, ext->value->length);
#endif

            /* Iterate through to find the dNSName field(s) */
            val = method->i2v(method, extstr, NULL);

            /* dNSName shortname is "DNS" */
            for (j = 0; j < sk_CONF_VALUE_num(val); j++) {
               nval = sk_CONF_VALUE_value(val, j);
               if (strcmp(nval->name, "DNS") == 0) {
                  if (strcasecmp(nval->value, host) == 0) {
                     auth_success = true;
                     goto success;
                  }
               }
            }
         }
      }
   }

   /* Try verifying against the subject name */
   if (!auth_success) {
      if ((subject = X509_get_subject_name(cert)) != NULL) {
         /* Loop through all CNs */
         for (;;) {
            cnLastPos = X509_NAME_get_index_by_NID(subject, NID_commonName, cnLastPos);
            if (cnLastPos == -1) {
               break;
            }
            neCN = X509_NAME_get_entry(subject, cnLastPos);
            asn1CN = X509_NAME_ENTRY_get_data(neCN);
            if (strcasecmp((const char*)asn1CN->data, host) == 0) {
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

/*
 * Create a new TLS_CONNECTION instance.
 *
 * Returns: Pointer to TLS_CONNECTION instance on success
 *          NULL on failure;
 */
TLS_CONNECTION *new_tls_connection(TLS_CONTEXT *ctx, int fd)
{
   BIO *bio;

   /*
    * Create a new BIO and assign the fd.
    * The caller will remain responsible for closing the associated fd
    */
   bio = BIO_new(BIO_s_socket());
   if (!bio) {
      /* Not likely, but never say never */
      openssl_post_errors(M_FATAL, _("Error creating file descriptor-based BIO"));
      return NULL; /* Nothing allocated, nothing to clean up */
   }
   BIO_set_fd(bio, fd, BIO_NOCLOSE);

   /* Allocate our new tls connection */
   TLS_CONNECTION *tls = (TLS_CONNECTION *)malloc(sizeof(TLS_CONNECTION));

   /* Create the SSL object and attach the socket BIO */
   if ((tls->openssl = SSL_new(ctx->openssl)) == NULL) {
      /* Not likely, but never say never */
      openssl_post_errors(M_FATAL, _("Error creating new SSL object"));
      goto err;
   }

   SSL_set_bio(tls->openssl, bio, bio);

   /* Non-blocking partial writes */
   SSL_set_mode(tls->openssl, SSL_MODE_ENABLE_PARTIAL_WRITE|SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

   return tls;

err:
   /* Clean up */
   BIO_free(bio);
   SSL_free(tls->openssl);
   free(tls);
   return NULL;
}

/*
 * Free TLS_CONNECTION instance
 */
void free_tls_connection(TLS_CONNECTION *tls)
{
   SSL_free(tls->openssl);
   free(tls);
}

/* Does all the manual labor for tls_bsock_accept() and tls_bsock_connect() */
static inline bool openssl_bsock_session_start(BSOCK *bsock, bool server)
{
   TLS_CONNECTION *tls = bsock->tls;
   int err;
   int fdmax, flags;
   int stat = true;
   fd_set fdset;
   struct timeval tv;

   /* Zero the fdset, we'll set our fd prior to each invocation of select() */
   FD_ZERO(&fdset);
   fdmax = bsock->m_fd + 1;

   /* Ensure that socket is non-blocking */
   flags = bsock->set_nonblocking();

   /* start timer */
   bsock->timer_start = watchdog_time;
   bsock->clear_timed_out();
   bsock->set_killable(false);

   for (;;) { 
      if (server) {
         err = SSL_accept(tls->openssl);
      } else {
         err = SSL_connect(tls->openssl);
      }

      /* Handle errors */
      switch (SSL_get_error(tls->openssl, err)) {
      case SSL_ERROR_NONE:
         stat = true;
         goto cleanup;
      case SSL_ERROR_ZERO_RETURN:
         /* TLS connection was cleanly shut down */
         openssl_post_errors(bsock->get_jcr(), M_FATAL, _("Connect failure"));
         stat = false;
         goto cleanup;
      case SSL_ERROR_WANT_READ:
         /* If we timeout of a select, this will be unset */
         FD_SET((unsigned) bsock->m_fd, &fdset);
         /* Set our timeout */
         tv.tv_sec = 10;
         tv.tv_usec = 0;
         /* Block until we can read */
         select(fdmax, &fdset, NULL, NULL, &tv);
         break;
      case SSL_ERROR_WANT_WRITE:
         /* If we timeout of a select, this will be unset */
         FD_SET((unsigned) bsock->m_fd, &fdset);
         /* Set our timeout */
         tv.tv_sec = 10;
         tv.tv_usec = 0;
         /* Block until we can write */
         select(fdmax, NULL, &fdset, NULL, &tv);
         break;
      default:
         /* Socket Error Occurred */
         openssl_post_errors(bsock->get_jcr(), M_FATAL, _("Connect failure"));
         stat = false;
         goto cleanup;
      }

      if (bsock->is_timed_out()) {
         goto cleanup;
      }
   }

cleanup:
   /* Restore saved flags */
   bsock->restore_blocking(flags);
   /* Clear timer */
   bsock->timer_start = 0;
   bsock->set_killable(true);

   return stat;
}

/*
 * Initiates a TLS connection with the server.
 *  Returns: true on success
 *           false on failure
 */
bool tls_bsock_connect(BSOCK *bsock)
{
   /* SSL_connect(bsock->tls) */
   return openssl_bsock_session_start(bsock, false);
}

/*
 * Listens for a TLS connection from a client.
 *  Returns: true on success
 *           false on failure
 */
bool tls_bsock_accept(BSOCK *bsock)
{
   /* SSL_accept(bsock->tls) */
   return openssl_bsock_session_start(bsock, true);
}

/*
 * Shutdown TLS_CONNECTION instance
 */
void tls_bsock_shutdown(BSOCK *bsock)
{
   /*
    * SSL_shutdown must be called twice to fully complete the process -
    * The first time to initiate the shutdown handshake, and the second to
    * receive the peer's reply.
    *
    * In addition, if the underlying socket is blocking, SSL_shutdown()
    * will not return until the current stage of the shutdown process has
    * completed or an error has occured. By setting the socket blocking
    * we can avoid the ugly for()/switch()/select() loop.
    */
   int err;

   btimer_t *tid;

   /* Set socket blocking for shutdown */
   bsock->set_blocking();

   tid = start_bsock_timer(bsock, 60 * 2);
   err = SSL_shutdown(bsock->tls->openssl);
   stop_bsock_timer(tid);
   if (err == 0) {
      /* Complete shutdown */
      tid = start_bsock_timer(bsock, 60 * 2);
      err = SSL_shutdown(bsock->tls->openssl);
      stop_bsock_timer(tid);
   }


   switch (SSL_get_error(bsock->tls->openssl, err)) {
   case SSL_ERROR_NONE:
      break;
   case SSL_ERROR_ZERO_RETURN:
      /* TLS connection was shut down on us via a TLS protocol-level closure */
      openssl_post_errors(bsock->get_jcr(), M_ERROR, _("TLS shutdown failure."));
      break;
   default:
      /* Socket Error Occurred */
      openssl_post_errors(bsock->get_jcr(), M_ERROR, _("TLS shutdown failure."));
      break;
   }
}

/* Does all the manual labor for tls_bsock_readn() and tls_bsock_writen() */
static inline int openssl_bsock_readwrite(BSOCK *bsock, char *ptr, int nbytes, bool write)
{
   TLS_CONNECTION *tls = bsock->tls;
   int fdmax, flags;
   fd_set fdset;
   struct timeval tv;
   int nleft = 0;
   int nwritten = 0;

   /* Zero the fdset, we'll set our fd prior to each invocation of select() */
   FD_ZERO(&fdset);
   fdmax = bsock->m_fd + 1;

   /* Ensure that socket is non-blocking */
   flags = bsock->set_nonblocking();

   /* start timer */
   bsock->timer_start = watchdog_time;
   bsock->clear_timed_out();
   bsock->set_killable(false);

   nleft = nbytes;

   while (nleft > 0) { 
      if (write) {
         nwritten = SSL_write(tls->openssl, ptr, nleft);
      } else {
         nwritten = SSL_read(tls->openssl, ptr, nleft);
      }

      /* Handle errors */
      switch (SSL_get_error(tls->openssl, nwritten)) {
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
               bmicrosleep(0, 20000); /* try again in 20 ms */
               continue;
            }
         }
         openssl_post_errors(bsock->get_jcr(), M_FATAL, _("TLS read/write failure."));
         goto cleanup;

      case SSL_ERROR_WANT_READ:
         /* If we timeout on a select, this will be unset */
         FD_SET((unsigned)bsock->m_fd, &fdset);
         tv.tv_sec = 10;
         tv.tv_usec = 0;
         /* Block until we can read */
         select(fdmax, &fdset, NULL, NULL, &tv);
         break;

      case SSL_ERROR_WANT_WRITE:
         /* If we timeout on a select, this will be unset */
         FD_SET((unsigned)bsock->m_fd, &fdset);
         tv.tv_sec = 10;
         tv.tv_usec = 0;
         /* Block until we can write */
         select(fdmax, NULL, &fdset, NULL, &tv);
         break;

      case SSL_ERROR_ZERO_RETURN:
         /* TLS connection was cleanly shut down */
         /* Fall through wanted */
      default:
         /* Socket Error Occured */
         openssl_post_errors(bsock->get_jcr(), M_FATAL, _("TLS read/write failure."));
         goto cleanup;
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
   /* SSL_write(bsock->tls->openssl, ptr, nbytes) */
   return openssl_bsock_readwrite(bsock, ptr, nbytes, true);
}

int tls_bsock_readn(BSOCK *bsock, char *ptr, int32_t nbytes) 
{
   /* SSL_read(bsock->tls->openssl, ptr, nbytes) */
   return openssl_bsock_readwrite(bsock, ptr, nbytes, false);
}

#else /* HAVE_OPENSSL */
# error No TLS implementation available.
#endif /* !HAVE_OPENSSL */


#else     /* TLS NOT enabled, dummy routines substituted */


/* Dummy routines */
TLS_CONTEXT *new_tls_context(const char *ca_certfile, const char *ca_certdir,
                             const char *certfile, const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata, const char *dhfile,
                             bool verify_peer)
{
   return NULL;
}
void free_tls_context(TLS_CONTEXT *ctx) { }

void tls_bsock_shutdown(BSOCK *bsock) { }

void free_tls_connection(TLS_CONNECTION *tls) { }

bool get_tls_require(TLS_CONTEXT *ctx) 
{
   return false;
}

bool get_tls_enable(TLS_CONTEXT *ctx) 
{
   return false;
}

#endif /* HAVE_TLS */
