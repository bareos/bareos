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
#ifndef BAREOS_LIB_TLS_OPENSSL_H_
#define BAREOS_LIB_TLS_OPENSSL_H_

typedef std::shared_ptr<PskCredentials> sharedPskCredentials;

DLL_IMP_EXP int hex2bin(char *str, unsigned char *out, unsigned int max_out_len);
DLL_IMP_EXP void FreeTlsContext(std::shared_ptr<TLS_CONTEXT> &ctx);

#ifdef HAVE_TLS
DLL_IMP_EXP bool tls_postconnect_verify_host(JobControlRecord *jcr, TLS_CONNECTION *tls_conn,
                                 const char *host);
DLL_IMP_EXP bool tls_postconnect_verify_cn(JobControlRecord *jcr, TLS_CONNECTION *tls_conn,
                               alist *verify_list);
DLL_IMP_EXP TLS_CONNECTION *new_tls_connection(std::shared_ptr<TlsContext> ctx, int fd, bool server);
DLL_IMP_EXP bool tls_bsock_accept(BareosSocket *bsock);
DLL_IMP_EXP int TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes);
DLL_IMP_EXP int TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes);
#endif /* HAVE_TLS */
DLL_IMP_EXP void TlsLogConninfo(JobControlRecord *jcr, TLS_CONNECTION *tls_conn, const char *host, int port, const char *who);
DLL_IMP_EXP bool tls_bsock_connect(BareosSocket *bsock);
DLL_IMP_EXP void TlsBsockShutdown(BareosSocket *bsock);
DLL_IMP_EXP void FreeTlsConnection(TLS_CONNECTION *tls_conn);
DLL_IMP_EXP bool get_tls_require(TLS_CONTEXT *ctx);
DLL_IMP_EXP void set_tls_require(TLS_CONTEXT *ctx, bool value);
DLL_IMP_EXP bool get_tls_enable(TLS_CONTEXT *ctx);
DLL_IMP_EXP void set_tls_enable(TLS_CONTEXT *ctx, bool value);
DLL_IMP_EXP bool get_tls_verify_peer(TLS_CONTEXT *ctx);


#endif // BAREOS_LIB_TLS_OPENSSL_H_
