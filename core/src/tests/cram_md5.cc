/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2020-2022 Bareos GmbH & Co. KG

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

#include "lib/bsock.h"
#include "lib/bsock_tcp.h"
#include "lib/cram_md5.h"
#include "tests/bareos_test_sockets.h"

#include <cassert>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <memory>

/* clang-format off */
static std::map<CramMd5Handshake::HandshakeResult, std::string>
    cram_result_to_string {
        {CramMd5Handshake::HandshakeResult::NOT_INITIALIZED, "NOT_INITIALIZED"},
        {CramMd5Handshake::HandshakeResult::SUCCESS, "SUCCESS"},
        {CramMd5Handshake::HandshakeResult::FORMAT_MISMATCH, "FORMAT_MISMATCH"},
        {CramMd5Handshake::HandshakeResult::NETWORK_ERROR, "NETWORK_ERROR"},
        {CramMd5Handshake::HandshakeResult::WRONG_HASH, "NETWORK_ERROR"},
        {CramMd5Handshake::HandshakeResult::REPLAY_ATTACK,"REPLAY_ATTACK"}};
/* clang-format on */

/********************** Tests *****************************/

static std::string CreateQualifiedResourceName(const char* r_type_str,
                                               const char* name)
{
  return std::string(r_type_str) + static_cast<char>(0x1e) + std::string(name);
}

static constexpr bool InitiatedByRemote = true;

class CramSockets {
 public:
  CramSockets(const char* r_code_str_1,
              const char* name_1,
              [[maybe_unused]] const char* r_code_str_2,
              const char* name_2)
      : sockets(create_connected_server_and_client_bareos_socket())
      , client_cram(
            CramMd5Handshake(sockets->client.get(),
                             "Secret-Password",
                             TlsPolicy::kBnetTlsNone,
                             CreateQualifiedResourceName(r_code_str_1, name_1)))
      , server_cram(
            CramMd5Handshake(sockets->server.get(),
                             "Secret-Password",
                             TlsPolicy::kBnetTlsNone,
                             CreateQualifiedResourceName(r_code_str_1, name_2)))
  {
    auto client_future
        = std::async(std::launch::async, &CramMd5Handshake::DoHandshake,
                     &client_cram, InitiatedByRemote);
    auto server_future
        = std::async(std::launch::async, &CramMd5Handshake::DoHandshake,
                     &server_cram, !InitiatedByRemote);

    server_future.wait();
    sockets->server->close();
    client_future.wait();
    sockets->client->close();

    handshake_ok_client = client_future.get();
    handshake_ok_server = server_future.get();
  }
  std::unique_ptr<TestSockets> sockets;
  CramMd5Handshake client_cram;
  CramMd5Handshake server_cram;
  bool handshake_ok_client, handshake_ok_server;
};

TEST(cram_md5, same_director_qualified_name)
{
  // should NOT pass cram
  CramSockets cs("R_DIRECTOR", "Test1", "R_DIRECTOR", "Test1");

  EXPECT_FALSE(cs.handshake_ok_client);
  EXPECT_EQ(cs.client_cram.result,
            CramMd5Handshake::HandshakeResult::NETWORK_ERROR)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.client_cram.result) << std::endl
      << "  Which is probably a side effect of a relay attack." << std::endl;

  EXPECT_FALSE(cs.handshake_ok_server);
  EXPECT_EQ(cs.server_cram.result,
            CramMd5Handshake::HandshakeResult::REPLAY_ATTACK)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.server_cram.result) << std::endl;
}

TEST(cram_md5, different_director_qualified_name)
{
  // should pass cram
  CramSockets cs("R_DIRECTOR", "Test1", "R_DIRECTOR", "Test2");

  EXPECT_TRUE(cs.handshake_ok_client);
  EXPECT_EQ(cs.client_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.client_cram.result) << std::endl
      << "  Which is probably a side effect of a relay attack." << std::endl;

  EXPECT_TRUE(cs.handshake_ok_server);
  EXPECT_EQ(cs.server_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.server_cram.result) << std::endl;
}

TEST(cram_md5, same_stored_qualified_name)
{
  // should pass cram
  CramSockets cs("R_STORAGE", "Test1", "R_STORAGE", "Test1");

  EXPECT_TRUE(cs.handshake_ok_client);
  EXPECT_EQ(cs.client_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.client_cram.result) << std::endl
      << "  Which is probably a side effect of a relay attack." << std::endl;

  EXPECT_TRUE(cs.handshake_ok_server);
  EXPECT_EQ(cs.server_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.server_cram.result) << std::endl;
}

TEST(cram_md5, different_stored_qualified_name)
{
  // should pass cram
  CramSockets cs("R_STORAGE", "Test1", "R_STORAGE", "Test2");

  EXPECT_TRUE(cs.handshake_ok_client);
  EXPECT_EQ(cs.client_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.client_cram.result) << std::endl
      << "  Which is probably a side effect of a relay attack." << std::endl;

  EXPECT_TRUE(cs.handshake_ok_server);
  EXPECT_EQ(cs.server_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.server_cram.result) << std::endl;
}
