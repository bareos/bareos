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

#include "tls.h"

DLL_IMP_EXP int32_t BnetRecv(BareosSocket *bsock);
DLL_IMP_EXP bool BnetSend(BareosSocket *bsock);
DLL_IMP_EXP bool BnetFsend(BareosSocket *bs, const char *fmt, ...);
DLL_IMP_EXP bool BnetSetBufferSize(BareosSocket *bs, uint32_t size, int rw);
DLL_IMP_EXP bool BnetSig(BareosSocket *bs, int sig);
DLL_IMP_EXP bool BnetTlsServer(BareosSocket *bsock,
                     alist *verify_list);
DLL_IMP_EXP bool BnetTlsClient(BareosSocket *bsock,
                     bool VerifyPeer, alist *verify_list);
DLL_IMP_EXP int BnetGetPeer(BareosSocket *bs, char *buf, socklen_t buflen);
DLL_IMP_EXP BareosSocket *dup_bsock(BareosSocket *bsock);
DLL_IMP_EXP const char *BnetStrerror(BareosSocket *bsock);
DLL_IMP_EXP const char *BnetSigToAscii(BareosSocket *bsock);
DLL_IMP_EXP int BnetWaitData(BareosSocket *bsock, int sec);
DLL_IMP_EXP int BnetWaitDataIntr(BareosSocket *bsock, int sec);
DLL_IMP_EXP bool IsBnetStop(BareosSocket *bsock);
DLL_IMP_EXP int IsBnetError(BareosSocket *bsock);
DLL_IMP_EXP void BnetSuppressErrorMessages(BareosSocket *bsock, bool flag);
DLL_IMP_EXP dlist *BnetHost2IpAddrs(const char *host, int family, const char **errstr);
DLL_IMP_EXP int BnetSetBlocking(BareosSocket *sock);
DLL_IMP_EXP int BnetSetNonblocking(BareosSocket *sock);
DLL_IMP_EXP void BnetRestoreBlocking(BareosSocket *sock, int flags);
DLL_IMP_EXP int NetConnect(int port);
DLL_IMP_EXP BareosSocket *BnetBind(int port);
DLL_IMP_EXP BareosSocket *BnetAccept(BareosSocket *bsock, char *who);

#endif // BAREOS_LIB_BNET_H_
