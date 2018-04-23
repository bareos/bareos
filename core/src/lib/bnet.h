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
#ifndef BAREOS_LIB_BNET_H_
#define BAREOS_LIB_BNET_H_

DLL_IMP_EXP int32_t bnet_recv(BareosSocket *bsock);
DLL_IMP_EXP bool bnet_send(BareosSocket *bsock);
DLL_IMP_EXP bool bnet_fsend(BareosSocket *bs, const char *fmt, ...);
DLL_IMP_EXP bool bnet_set_buffer_size(BareosSocket *bs, uint32_t size, int rw);
DLL_IMP_EXP bool bnet_sig(BareosSocket *bs, int sig);
DLL_IMP_EXP bool bnet_tls_server(std::shared_ptr<TlsContext> tls_ctx, BareosSocket *bsock,
                     alist *verify_list);
DLL_IMP_EXP bool bnet_tls_client(std::shared_ptr<TLS_CONTEXT> tls_ctx, BareosSocket *bsock,
                     bool verify_peer, alist *verify_list);
DLL_IMP_EXP int bnet_get_peer(BareosSocket *bs, char *buf, socklen_t buflen);
DLL_IMP_EXP BareosSocket *dup_bsock(BareosSocket *bsock);
DLL_IMP_EXP const char *bnet_strerror(BareosSocket *bsock);
DLL_IMP_EXP const char *bnet_sig_to_ascii(BareosSocket *bsock);
DLL_IMP_EXP int bnet_wait_data(BareosSocket *bsock, int sec);
DLL_IMP_EXP int bnet_wait_data_intr(BareosSocket *bsock, int sec);
DLL_IMP_EXP bool is_bnet_stop(BareosSocket *bsock);
DLL_IMP_EXP int is_bnet_error(BareosSocket *bsock);
DLL_IMP_EXP void bnet_suppress_error_messages(BareosSocket *bsock, bool flag);
DLL_IMP_EXP dlist *bnet_host2ipaddrs(const char *host, int family, const char **errstr);
DLL_IMP_EXP int bnet_set_blocking(BareosSocket *sock);
DLL_IMP_EXP int bnet_set_nonblocking(BareosSocket *sock);
DLL_IMP_EXP void bnet_restore_blocking(BareosSocket *sock, int flags);
DLL_IMP_EXP int net_connect(int port);
DLL_IMP_EXP BareosSocket *bnet_bind(int port);
DLL_IMP_EXP BareosSocket *bnet_accept(BareosSocket *bsock, char *who);

#endif // BAREOS_LIB_BNET_H_
