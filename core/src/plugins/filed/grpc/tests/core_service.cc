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

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsubobject-linkage"
#endif
#include "../grpc_impl.cc"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#include <gtest/gtest.h>

namespace {
class GrpcServiceTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    callbacks.AcceptFile
        = [](PluginContext* context, filedaemon::save_pkt* pkt) {
            EXPECT_STREQ(pkt->fname, "/candidate");
            EXPECT_EQ(pkt->statp.st_size, 4321);
            return *static_cast<bRC*>(context->plugin_private_context);
          };
    SetupBareosApi(&callbacks);
    ctx.plugin_private_context = &result;
    grpc::ServerBuilder builder;
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    client = bc::Core::NewStub(server->InProcessChannel({}));
  }

  void TearDown() override
  {
    server->Shutdown();
    SetupBareosApi(nullptr);
  }

  bRC result{bRC_OK};
  PluginContext ctx{};
  filedaemon::CoreFunctions callbacks{};
  BareosCore service{&ctx};
  std::unique_ptr<grpc::Server> server;
  std::unique_ptr<bc::Core::Stub> client;
};

TEST_F(GrpcServiceTest, AcceptFilePreservesAcceptanceAndReportsErrors)
{
  bc::AcceptFileRequest request;
  request.set_file("/candidate");
  struct stat stats{};
  stats.st_size = 4321;
  request.set_stats(&stats, sizeof(stats));
  for (bRC value :
       {bRC_OK, bRC_Skip, bRC_Error, bRC_Seen, bRC_Stop, bRC_More, bRC_Term,
        bRC_Cancel, bRC_Core, PYTHON_UNDEFINED_RETURN_VALUE, bRC_Max}) {
    result = value;
    grpc::ClientContext context;
    bc::AcceptFileResponse response;
    auto status = client->Bareos_AcceptFile(&context, request, &response);
    if (value == bRC_OK || value == bRC_Skip) {
      EXPECT_TRUE(status.ok());
      EXPECT_EQ(response.skip(), value == bRC_Skip);
    } else {
      EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL)
          << "callback result " << value;
    }
  }
  SetupBareosApi(nullptr);
  grpc::ClientContext context;
  bc::AcceptFileResponse response;
  EXPECT_EQ(
      client->Bareos_AcceptFile(&context, request, &response).error_code(),
      grpc::StatusCode::INTERNAL);
}

TEST_F(GrpcServiceTest, AcceptFileRejectsMalformedStats)
{
  bc::AcceptFileRequest request;
  bc::AcceptFileResponse response;
  grpc::ClientContext context;
  EXPECT_EQ(
      client->Bareos_AcceptFile(&context, request, &response).error_code(),
      grpc::StatusCode::INVALID_ARGUMENT);
}
}  // namespace
