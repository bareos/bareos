/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * tls.c TLS support functions when using Mozilla NSS backend.
 *
 * Author: Marco van Wieringen <marco.van.wieringen@bareos.com>
 */

#include "bareos.h"
#include <assert.h>

#if defined(HAVE_TLS) && defined(HAVE_NSS)

/* TLS Context Structure */
struct TlsContext {
   bool tls_enable;
   bool tls_require;
};

struct TlsConnection {
};

TLS_CONTEXT *new_tls_context(const char *cipherlist, CRYPTO_TLS_PSK_CB) {}

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
   return NULL;
}

void free_tls_context(TLS_CONTEXT *ctx)
{
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
 * Verifies a list of common names against the certificate commonName attribute.
 *
 * Returns: true on success
 *          false on failure
 */
bool tls_postconnect_verify_cn(JobControlRecord *jcr, TLS_CONNECTION *tls_conn, alist *verify_list)
{
   return true;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName attributes.
 *
 * Returns: true on success
 *          false on failure
 */
bool tls_postconnect_verify_host(JobControlRecord *jcr, TLS_CONNECTION *tls_conn, const char *host)
{
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
      return NULL;
}

void free_tls_connection(TLS_CONNECTION *tls_conn)
{
}

/*
 * Initiates a TLS connection with the server.
 *  Returns: true on success
 *           false on failure
 */
bool tls_bsock_connect(BareosSocket *bsock)
{
   return false;
}

/*
 * Listens for a TLS connection from a client.
 *  Returns: true on success
 *           false on failure
 */
bool tls_bsock_accept(BareosSocket *bsock)
{
   return false;
}

void tls_bsock_shutdown(BareosSocket *bsock)
{
}

int tls_bsock_writen(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
   return -1;
}

int tls_bsock_readn(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
   return -1;
}
#endif /* HAVE_TLS && HAVE_NSS */
