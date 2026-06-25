/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2020-2026 Bareos GmbH & Co. KG

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

#include "webui-proxy/director_connection.h"
#include "lib/bsock.h"
#include "lib/bsock_tcp.h"
#include "lib/base64.h"
#include "lib/cram_md5.h"
#include "tests/bareos_test_sockets.h"

#include <cassert>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <signal.h>
#include <string_view>

static bool InitSignalHandler()
{
#if !defined(HAVE_WIN32)
  struct sigaction sig = {};
  sig.sa_handler = SIG_IGN;
  sigaction(SIGUSR2, &sig, nullptr);
  sigaction(SIGPIPE, &sig, nullptr);
#endif

  return true;
}

static bool did_init = InitSignalHandler();

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

static std::string CreateQualifiedResourceName(std::string_view r_type_str,
                                               std::string_view name)
{
  return std::string(r_type_str) + static_cast<char>(0x1e)
         + std::string(name);
}

static std::string ExtractChallenge(std::string_view message)
{
  const auto prefix_end = message.find('<');
  const auto suffix = message.find(" ssl=");

  EXPECT_NE(prefix_end, std::string_view::npos);
  EXPECT_NE(suffix, std::string_view::npos);

  return std::string(message.substr(prefix_end, suffix - prefix_end));
}

static constexpr bool InitiatedByRemote = true;

TEST(cram_md5, derives_cram_md5_key_from_plaintext_password)
{
  EXPECT_EQ(MakeCramMd5Key("secret"), "5ebe2294ecd0e0f08eab7690d2a6ee69");
}

class CramSockets {
 public:
  CramSockets(const char* r_code_str_1,
              const char* name_1,
              const char*,
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

TEST(cram_md5, accepts_compatible_challenge_without_qualified_name)
{
  auto sockets = create_connected_server_and_client_bareos_socket();
  sockets->server->InitBnetDump(
      CreateQualifiedResourceName("R_DIRECTOR", "srv"));

  CramMd5Handshake server_cram(
      sockets->server.get(), "Secret-Password", TlsPolicy::kBnetTlsNone,
      CreateQualifiedResourceName("R_DIRECTOR", "srv"));

  auto server_future = std::async(
      std::launch::async, &CramMd5Handshake::DoHandshake, &server_cram, false);

  const std::string challenge
      = "auth cram-md5c <1234567890.1715520000@client> ssl=0\n";
  ASSERT_TRUE(
      static_cast<BareosSocket*>(sockets->client.get())
          ->send(challenge.c_str(), static_cast<uint32_t>(challenge.size())));

  ASSERT_GT(sockets->client->recv(), 0);

  uint8_t hmac[20];
  hmac_md5((uint8_t*)"<1234567890.1715520000@client>",
           strlen("<1234567890.1715520000@client>"),
           (uint8_t*)"Secret-Password", strlen("Secret-Password"), hmac);

  char expected[50]{};
  BinToBase64(expected, sizeof(expected), (char*)hmac, 16, true);
  EXPECT_STREQ(sockets->client->msg, expected);

  const std::string auth_ok = "1000 OK auth\n";
  ASSERT_TRUE(
      static_cast<BareosSocket*>(sockets->client.get())
          ->send(auth_ok.c_str(), static_cast<uint32_t>(auth_ok.size())));

  ASSERT_GT(sockets->client->recv(), 0);
  const std::string server_challenge = ExtractChallenge(sockets->client->msg);

  hmac_md5((uint8_t*)server_challenge.c_str(), strlen(server_challenge.c_str()),
           (uint8_t*)"Secret-Password", strlen("Secret-Password"), hmac);
  BinToBase64(expected, sizeof(expected), (char*)hmac, 16, true);

  ASSERT_TRUE(static_cast<BareosSocket*>(sockets->client.get())
                  ->send(expected, static_cast<uint32_t>(strlen(expected))));
  ASSERT_GT(sockets->client->recv(), 0);
  EXPECT_STREQ(sockets->client->msg, "1000 OK auth\n");

  EXPECT_TRUE(server_future.get());
  EXPECT_EQ(server_cram.result, CramMd5Handshake::HandshakeResult::SUCCESS);

  sockets->client->close();
  sockets->server->close();
}
