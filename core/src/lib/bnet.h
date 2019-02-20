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

int32_t BnetRecv(BareosSocket* bsock);
bool BnetSend(BareosSocket* bsock);
bool BnetFsend(BareosSocket* bs, const char* fmt, ...);
bool BnetSetBufferSize(BareosSocket* bs, uint32_t size, int rw);
bool BnetSig(BareosSocket* bs, int sig);
bool BnetTlsServer(BareosSocket* bsock,
                   const std::vector<std::string>& verify_list);
bool BnetTlsClient(BareosSocket* bsock,
                   bool VerifyPeer,
                   const std::vector<std::string>& verify_list);
int BnetGetPeer(BareosSocket* bs, char* buf, socklen_t buflen);
BareosSocket* dup_bsock(BareosSocket* bsock);
const char* BnetStrerror(BareosSocket* bsock);
const char* BnetSigToAscii(BareosSocket* bsock);
int BnetWaitData(BareosSocket* bsock, int sec);
int BnetWaitDataIntr(BareosSocket* bsock, int sec);
bool IsBnetStop(BareosSocket* bsock);
int IsBnetError(BareosSocket* bsock);
void BnetSuppressErrorMessages(BareosSocket* bsock, bool flag);
dlist* BnetHost2IpAddrs(const char* host, int family, const char** errstr);
int BnetSetBlocking(BareosSocket* sock);
int BnetSetNonblocking(BareosSocket* sock);
void BnetRestoreBlocking(BareosSocket* sock, int flags);
int NetConnect(int port);
BareosSocket* BnetBind(int port);
BareosSocket* BnetAccept(BareosSocket* bsock, char* who);

enum : uint32_t
{
  kMessageIdUnknown = 0,
  kMessageIdProtokollError = 1,
  kMessageIdReceiveError = 2,
  kMessageIdOk = 1000,
  kMessageIdPamRequired = 1001,
  kMessageIdInfoMessage = 1002,
  kMessageIdPamInteractive = 4001,
  kMessageIdPamUserCredentials = 4002
};

class BStringList;

#ifdef BAREOS_TEST_LIB
bool ReadoutCommandIdFromMessage(const BStringList& list_of_arguments,
                                 uint32_t& id_out);
bool EvaluateResponseMessageId(const std::string& message,
                               uint32_t& id_out,
                               BStringList& args_out);
#endif

bool ReceiveAndEvaluateResponseMessage(BareosSocket* bsock,
                                       uint32_t& id_out,
                                       BStringList& args_out);
bool FormatAndSendResponseMessage(BareosSocket* bsock,
                                  uint32_t id,
                                  const std::string& str);
bool FormatAndSendResponseMessage(BareosSocket* bsock,
                                  uint32_t id,
                                  const BStringList& list_of_agruments);

#endif  // BAREOS_LIB_BNET_H_
