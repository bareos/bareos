/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.

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
 * tls_none.c TLS support functions when no backend.
 *
 * Author: Landon Fuller <landonf@threerings.net>
 */

#include "include/bareos.h"
#include <assert.h>

#ifndef HAVE_TLS

/* Dummy routines */
TLS_CONTEXT *new_tls_context(const char *cipherlist) {
   return nullptr;
}

TLS_CONTEXT *new_tls_context(const char *CaCertfile,
                             const char *CaCertdir,
                             const char *crlfile,
                             const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata,
                             const char *dhfile,
                             const char *cipherlist,
                             bool VerifyPeer) {
   return nullptr;
}

/**
 * Get connection info and log it into joblog
 */
void TlsLogConninfo(JobControlRecord *jcr, TLS_CONNECTION *tls_conn, const char *host, int port, const char *who) {
   Qmsg(jcr, M_INFO, 0, _("Cleartext connection to %s at %s:%d established\n"), who, host, port);
}

std::shared_ptr<TLS_CONTEXT> tls_cert_t::CreateClientContext(
   std::shared_ptr<PskCredentials> /* credentials */) const {
   return nullptr;
}

std::shared_ptr<TLS_CONTEXT> tls_cert_t::CreateServerContext(
   std::shared_ptr<PskCredentials> /* credentials */) const {
   return nullptr;
}

bool tls_cert_t::enabled(u_int32_t policy) {
   return false;
}

bool tls_cert_t::required(u_int32_t policy) {
   return false;
}

bool tls_psk_t::enabled(u_int32_t policy) {
   return false;
}

bool tls_psk_t::required(u_int32_t policy) {
   return false;
}

std::shared_ptr<TLS_CONTEXT> tls_psk_t::CreateClientContext(
   std::shared_ptr<PskCredentials> credentials) const {
   return nullptr;
}

std::shared_ptr<TLS_CONTEXT> tls_psk_t::CreateServerContext(
   std::shared_ptr<PskCredentials> credentials) const {
   return nullptr;
}

void FreeTlsContext(TLS_CONTEXT *ctx) {
}

bool GetTlsRequire(TLS_CONTEXT *ctx) {
   return false;
}

void SetTlsRequire(TLS_CONTEXT *ctx, bool value) {
}

bool GetTlsEnable(TLS_CONTEXT *ctx) {
   return false;
}

void SetTlsEnable(TLS_CONTEXT *ctx, bool value) {
}

bool GetTlsVerifyPeer(TLS_CONTEXT *ctx) {
   return false;
}

TLS_CONNECTION *new_tls_connection(TLS_CONTEXT *ctx, int fd, bool server) {
   return NULL;
}

void FreeTlsConnection(TLS_CONNECTION *tls_conn) {
}

void TlsBsockShutdown(BareosSocket *bsock) {
}
#endif /* HAVE_TLS */
