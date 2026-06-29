/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2026 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "ndmprotocol.h"

// Verify that CONNECT_CLOSE across all protocol versions has
// xdr_reply == NULL in the XMT table. This ensures ndmconn_call()
// treats CONNECT_CLOSE as a send-only message (no reply expected),
// avoiding the spurious "ERR=EOF" and "ERR=exchange-failed" errors
// that occurred during normal connection teardown.
TEST(ndmp_xmt, connect_close_has_no_reply)
{
  // NDMP0
  auto* ent = ndmp_xmt_lookup(0, NDMP0_CONNECT_CLOSE);
  ASSERT_NE(ent, nullptr);
  EXPECT_EQ(ent->xdr_reply, nullptr);

  // NDMP2
  ent = ndmp_xmt_lookup(NDMP2VER, NDMP2_CONNECT_CLOSE);
  ASSERT_NE(ent, nullptr);
  EXPECT_EQ(ent->xdr_reply, nullptr);

  // NDMP3
  ent = ndmp_xmt_lookup(NDMP3VER, NDMP3_CONNECT_CLOSE);
  ASSERT_NE(ent, nullptr);
  EXPECT_EQ(ent->xdr_reply, nullptr);

  // NDMP4
  ent = ndmp_xmt_lookup(NDMP4VER, NDMP4_CONNECT_CLOSE);
  ASSERT_NE(ent, nullptr);
  EXPECT_EQ(ent->xdr_reply, nullptr);
}

// Sanity check: TAPE_OPEN should still have a reply XDR (it's a normal
// request-reply message). This confirms we didn't accidentally change
// unrelated entries.
TEST(ndmp_xmt, tape_open_has_reply)
{
  auto* ent = ndmp_xmt_lookup(NDMP4VER, NDMP4_TAPE_OPEN);
  ASSERT_NE(ent, nullptr);
  EXPECT_NE(ent->xdr_reply, nullptr);
}
