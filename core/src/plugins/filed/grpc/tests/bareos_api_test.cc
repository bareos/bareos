/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#ifdef TEST_WINDOWS_DR_API
#  include "../../windows_dr/bareos_api.cc"
#else
#  include "../bareos_api.cc"
#endif

#include <gtest/gtest.h>

namespace {
class BareosApiTest : public ::testing::Test {
 protected:
  void TearDown() override { fd.core = nullptr; }
};

TEST_F(BareosApiTest, AcceptFilePreservesCallbackResults)
{
  PluginContext ctx{};
  struct stat stats{};
  stats.st_size = 1234;
  filedaemon::CoreFunctions callbacks{};
  callbacks.size = sizeof(callbacks);
  callbacks.version = FD_PLUGIN_INTERFACE_VERSION;
  callbacks.AcceptFile = [](PluginContext* context, filedaemon::save_pkt* pkt) {
    EXPECT_NE(context, nullptr);
    EXPECT_STREQ(pkt->fname, "/accepted");
    EXPECT_EQ(pkt->statp.st_size, 1234);
    return *static_cast<bRC*>(context->plugin_private_context);
  };
  SetupBareosApi(&callbacks);
  for (bRC result :
       {bRC_OK, bRC_Skip, bRC_Error, bRC_Seen, bRC_Stop, bRC_More, bRC_Term,
        bRC_Cancel, bRC_Core, PYTHON_UNDEFINED_RETURN_VALUE, bRC_Max}) {
    ctx.plugin_private_context = &result;
    EXPECT_EQ(static_cast<int>(AcceptFile(&ctx, "/accepted", stats)),
              static_cast<int>(result));
  }
}

TEST_F(BareosApiTest, AcceptFileWithoutCoreFails)
{
  fd.core = nullptr;
  EXPECT_EQ(static_cast<int>(AcceptFile(nullptr, "/missing", {})),
            static_cast<int>(bRC_Error));
}

TEST_F(BareosApiTest, ClearSeenPreservesCallbackResultsAndArguments)
{
  struct State {
    bRC result{};
    bool all{};
    int calls{};
  } state;
  PluginContext ctx{};
  ctx.plugin_private_context = &state;
  filedaemon::CoreFunctions callbacks{};
  callbacks.size = sizeof(callbacks);
  callbacks.version = FD_PLUGIN_INTERFACE_VERSION;
  callbacks.ClearSeenBitmap
      = [](PluginContext* context, bool all, char* filename) {
          auto& actual = *static_cast<State*>(context->plugin_private_context);
          EXPECT_EQ(all, actual.all);
          if (all) {
            EXPECT_EQ(filename, nullptr);
          } else {
            EXPECT_STREQ(filename, "/candidate");
          }
          ++actual.calls;
          return actual.result;
        };
  SetupBareosApi(&callbacks);
  for (bool all : {false, true}) {
    state.all = all;
    for (bRC result :
         {bRC_OK, bRC_Skip, bRC_Error, bRC_Seen, bRC_Stop, bRC_More, bRC_Term,
          bRC_Cancel, bRC_Core, PYTHON_UNDEFINED_RETURN_VALUE, bRC_Max}) {
      state.result = result;
      state.calls = 0;
      EXPECT_EQ(ClearSeenBitmap(&ctx, all, all ? nullptr : "/candidate"),
                result);
      EXPECT_EQ(state.calls, 1);
    }
  }
}

TEST_F(BareosApiTest, ClearSeenWithoutCoreFails)
{
  fd.core = nullptr;
  EXPECT_EQ(ClearSeenBitmap(nullptr, true, nullptr), bRC_Error);
  EXPECT_EQ(ClearSeenBitmap(nullptr, false, "/candidate"), bRC_Error);
}
}  // namespace
