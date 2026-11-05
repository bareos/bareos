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
#include "../grpc_impl.h"
#include <gtest/gtest.h>

bool send_fd(int unix_socket, int fd);
std::optional<int> receive_fd(int unix_socket, int expected_name);

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
  bool clear_all{};

  grpc::Status Bareos_ClearSeen(grpc::ServerContext*,
                                const bc::ClearSeenRequest* request,
                                bc::ClearSeenResponse*) override
  {
    EXPECT_EQ(request->has_file(), !clear_all);
    if (!clear_all) { EXPECT_EQ(request->file(), "/candidate"); }
    return fail ? grpc::Status(grpc::StatusCode::INTERNAL, "callback failed")
                : grpc::Status::OK;
  }
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

TEST_F(GrpcCoreTest, ClearSeenRoundTrip)
{
  for (bool all : {false, true}) {
    service.clear_all = all;
    for (bool fail : {false, true}) {
      service.fail = fail;
      EXPECT_EQ(
          filedaemon::Wrapper_ClearSeenBitmap(
              nullptr, all, all ? nullptr : const_cast<char*>("/candidate")),
          fail ? bRC_Error : bRC_OK);
    }
  }
}

TEST_F(GrpcCoreTest, DescriptorSenderTransfersUsableDescriptor)
{
  int sockets[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  Socket sender{sockets[0]};
  Socket receiver{sockets[1]};
  int descriptors[2];
  ASSERT_EQ(pipe(descriptors), 0);
  Socket source{descriptors[0]};
  Socket sink{descriptors[1]};
  const int name = source.get();
  ASSERT_TRUE(send_fd(sender.get(), name));
  source = Socket{};

  auto received = receive_fd(receiver.get(), name);
  ASSERT_TRUE(received);
  Socket transferred{*received};
  constexpr std::string_view payload{"hello"};
  ASSERT_EQ(write(sink.get(), payload.data(), payload.size()), payload.size());
  char data[payload.size()]{};
  ASSERT_EQ(read(transferred.get(), data, sizeof(data)), sizeof(data));
  EXPECT_EQ(std::string_view(data, sizeof(data)), payload);
}

TEST_F(GrpcCoreTest, DescriptorSenderReportsClosedPeer)
{
  int sockets[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  Socket sender{sockets[0]};
  close(sockets[1]);
  Socket source{open("/dev/null", O_RDONLY)};
  ASSERT_GE(source.get(), 0);
  EXPECT_FALSE(send_fd(sender.get(), source.get()));
}
}  // namespace
