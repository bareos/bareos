/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
/**
 * Mockup for Network Utility Routines
 *
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/bstringlist.h"
#include "lib/cram_md5.h"
#include "lib/tls.h"
#include "lib/util.h"

#include <algorithm>

#ifdef __GNUC__
#ifndef __clang__
/* ignore the suggest-override warnings caused by MOCK_METHODx */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#endif

class BareosSocketMock : public BareosSocket {
 public:
  BareosSocketMock() : BareosSocket() {}
  ~BareosSocketMock();
  MOCK_METHOD0(clone, BareosSocket*());
  MOCK_METHOD9(connect,
               bool(JobControlRecord*,
                    int,
                    utime_t,
                    utime_t,
                    const char*,
                    const char*,
                    char*,
                    int,
                    bool));
  MOCK_METHOD0(recv, int32_t());
  MOCK_METHOD0(send, bool());
  MOCK_METHOD2(read_nbytes, int32_t(char*, int32_t));
  MOCK_METHOD2(write_nbytes, int32_t(char*, int32_t));
  MOCK_METHOD0(close, void());
  MOCK_METHOD0(destroy, void());
  MOCK_METHOD2(GetPeer, int(char*, socklen_t));
  MOCK_METHOD2(SetBufferSize, bool(uint32_t, int));
  MOCK_METHOD0(SetNonblocking, int());
  MOCK_METHOD0(SetBlocking, int());
  MOCK_METHOD1(RestoreBlocking, void(int));
  MOCK_METHOD0(ConnectionReceivedTerminateSignal, bool());
  MOCK_METHOD2(WaitData, int(int, int));
  MOCK_METHOD2(WaitDataIntr, int(int, int));
  MOCK_METHOD6(FinInit,
               void(JobControlRecord*,
                    int,
                    const char*,
                    const char*,
                    int,
                    struct sockaddr*));
  MOCK_METHOD7(open,
               bool(JobControlRecord*,
                    const char*,
                    const char*,
                    char*,
                    int,
                    utime_t,
                    int*));
};
#ifdef __GNUC__
#ifndef __clang__
#pragma GCC diagnostic pop
#endif
#endif
/* define a gmock action that fills bsock->msg so we can recv() a message */
ACTION_P2(BareosSocket_Recv, bsock, msg)
{
  Bsnprintf(bsock->msg, int32_t(strlen(msg) + 1), msg);
}
#define BSOCK_RECV(bsock, msg) \
  DoAll(BareosSocket_Recv(bsock, msg), Return(strlen(msg)))

BareosSocketMock::~BareosSocketMock()
{
  if (msg) { /* duplicated */
    FreePoolMemory(msg);
    msg = nullptr;
  }
  if (errmsg) { /* duplicated */
    FreePoolMemory(errmsg);
    errmsg = nullptr;
  }
  if (who_) { /* duplicated */
    free(who_);
    who_ = nullptr;
  }
  if (host_) { /* duplicated */
    free(host_);
    host_ = nullptr;
  }
  if (src_addr) { /* duplicated */
    free(src_addr);
    src_addr = nullptr;
  }
}
