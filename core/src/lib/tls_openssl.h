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
DLL_IMP_EXP bool TlsPostconnectVerifyHost(JobControlRecord *jcr, TLS_CONNECTION *tls_conn,
                                 const char *host);
DLL_IMP_EXP bool TlsPostconnectVerifyCn(JobControlRecord *jcr, TLS_CONNECTION *tls_conn,
                               alist *verify_list);
DLL_IMP_EXP TLS_CONNECTION *new_tls_connection(std::shared_ptr<TlsContext> ctx, int fd, bool server);
DLL_IMP_EXP bool TlsBsockAccept(BareosSocket *bsock);
DLL_IMP_EXP int TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes);
DLL_IMP_EXP int TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes);
#endif /* HAVE_TLS */
DLL_IMP_EXP void TlsLogConninfo(JobControlRecord *jcr, TLS_CONNECTION *tls_conn, const char *host, int port, const char *who);
DLL_IMP_EXP bool TlsBsockConnect(BareosSocket *bsock);
DLL_IMP_EXP void TlsBsockShutdown(BareosSocket *bsock);
DLL_IMP_EXP void FreeTlsConnection(TLS_CONNECTION *tls_conn);
DLL_IMP_EXP bool GetTlsRequire(TLS_CONTEXT *ctx);
DLL_IMP_EXP void SetTlsRequire(TLS_CONTEXT *ctx, bool value);
DLL_IMP_EXP bool GetTlsEnable(TLS_CONTEXT *ctx);
DLL_IMP_EXP void SetTlsEnable(TLS_CONTEXT *ctx, bool value);
DLL_IMP_EXP bool GetTlsVerifyPeer(TLS_CONTEXT *ctx);
DLL_IMP_EXP bool TlsPolicyHandshake(BareosSocket *bs, bool initiated_by_remote,
                                    uint32_t local,   uint32_t *remote);
DLL_IMP_EXP std::shared_ptr<TLS_CONTEXT> new_tls_context(const char *CaCertfile, const char *CaCertdir,
                             const char *crlfile, const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata, const char *dhfile,
                             const char *cipherlist, bool VerifyPeer);
DLL_IMP_EXP std::shared_ptr<TLS_CONTEXT> new_tls_psk_client_context(
    const char *cipherlist, std::shared_ptr<PskCredentials> credentials);
DLL_IMP_EXP std::shared_ptr<TLS_CONTEXT> new_tls_psk_server_context(
    const char *cipherlist, std::shared_ptr<PskCredentials> credentials);

#endif // BAREOS_LIB_TLS_OPENSSL_H_
