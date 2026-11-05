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

#define main bridge_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "../bridge_module/bridge_module.cc"
#pragma GCC diagnostic pop
#undef main
#include <gtest/gtest.h>

namespace {
class TestCore : public bc::Core::Service {
 public:
  grpc::Status Bareos_AcceptFile(grpc::ServerContext*,
                                 const bc::AcceptFileRequest* request,
                                 bc::AcceptFileResponse* response) override
  {
    EXPECT_EQ(request->file(), "/candidate");
    EXPECT_EQ(request->stats().size(), sizeof(struct stat));
    response->set_skip(skip);
    return fail ? grpc::Status(grpc::StatusCode::INTERNAL, "callback failed")
                : grpc::Status::OK;
  }
  bool skip{};
  bool fail{};
};

class GrpcCoreTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    grpc::ServerBuilder builder;
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    client = bc::Core::NewStub(server->InProcessChannel({}));
    global_stub = client.get();
  }

  void TearDown() override
  {
    global_stub = nullptr;
    server->Shutdown();
  }

  TestCore service;
  std::unique_ptr<grpc::Server> server;
  std::unique_ptr<bc::Core::Stub> client;
};

TEST_F(GrpcCoreTest, AcceptFileRoundTrip)
{
  filedaemon::save_pkt pkt{};
  pkt.fname = const_cast<char*>("/candidate");
  pkt.statp.st_size = 4321;
  for (bool skip : {false, true}) {
    service.skip = skip;
    EXPECT_EQ(filedaemon::Wrapper_AcceptFile(nullptr, &pkt),
              skip ? bRC_Skip : bRC_OK);
  }
  service.fail = true;
  EXPECT_EQ(filedaemon::Wrapper_AcceptFile(nullptr, &pkt), bRC_Error);
}
}  // namespace
