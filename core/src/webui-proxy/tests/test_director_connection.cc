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

#define private public
#include "../director_connection.h"
#undef private

#include "../bareos_base64.h"
#include "lib/cram_md5.h"
#include "lib/ascii_control_characters.h"
#include "lib/bnet_protocol_signals.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <thread>

#include <openssl/evp.h>
#include <openssl/hmac.h>

TEST(DirectorConnection, BashesSpacesInUsernameInHelloMessage)
{
  // Usernames containing spaces must have spaces encoded as \x01 so the
  // director's space-splitting Hello parser reconstructs the full name.
  int sockets[2] = {-1, -1};
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  DirectorConnection connection;
  connection.fd_ = sockets[0];

  DirectorConfig cfg;
  cfg.username = "my console";  // space in name
  cfg.password = "secret";
  cfg.json_mode = false;

  std::thread director([peer = sockets[1]]() {
    int32_t header = 0;
    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto hello_size = static_cast<size_t>(ntohl(header));
    std::string hello(hello_size, '\0');
    ASSERT_EQ(read(peer, hello.data(), hello.size()),
              static_cast<ssize_t>(hello.size()));

    // Space in username must be encoded as \x01, not a literal space.
    const std::string bashed
        = "my\x01"
          "console";
    EXPECT_NE(hello.find("Hello " + bashed + " calling version "),
              std::string::npos)
        << "Hello message was: " << hello;

    // Close without completing auth — we only care about the Hello content.
    close(peer);
  });

  // Authenticate will throw because we closed the socket early; that's fine.
  EXPECT_THROW(connection.Authenticate(cfg), std::runtime_error);

  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}

TEST(DirectorConnection, FormatsTlsPskIdentityAsQualifiedConsoleName)
{
  // The proxy prepends the console resource name and the separator byte.
  const std::string identity = GetTlsPskIdentityForDirector("admin-tls");
  std::string expected = "R_CONSOLE";
  expected.push_back(AsciiControlCharacters::RecordSeparator());
  expected.append("admin-tls");

  EXPECT_EQ(identity, expected);
}

namespace {
std::array<uint8_t, 16> HmacMd5Digest(const std::string& key,
                                      const std::string& data)
{
  std::array<uint8_t, 16> out{};
  unsigned int len = 16;
  HMAC(EVP_md5(), key.data(), static_cast<int>(key.size()),
       reinterpret_cast<const uint8_t*>(data.data()), data.size(), out.data(),
       &len);
  return out;
}

void WriteFrame(int fd, std::string_view data)
{
  const int32_t header = htonl(static_cast<int32_t>(data.size()));
  ASSERT_EQ(write(fd, &header, sizeof(header)), sizeof(header));
  if (!data.empty()) {
    ASSERT_EQ(write(fd, data.data(), data.size()),
              static_cast<ssize_t>(data.size()));
  }
}

void WriteSignal(int fd, int32_t signal)
{
  const int32_t header = htonl(signal);
  ASSERT_EQ(write(fd, &header, sizeof(header)), sizeof(header));
}
}  // namespace

TEST(DirectorConnection, CompletesPamAuthenticationBeforeFirstCommand)
{
  int sockets[2] = {-1, -1};
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  DirectorConnection connection;
  connection.fd_ = sockets[0];
  connection.tls_psk_active_ = true;
  connection.json_mode_ = false;
  DirectorConfig cfg;
  cfg.json_mode = false;
  cfg.pam_credentials = PamCredentials{"pam-user", "pam-password"};

  std::thread director([peer = sockets[1]]() {
    WriteFrame(peer,
               "1001\x1e"
               "PAM required");
    int32_t header = 0;
    if (read(peer, &header, sizeof(header)) != sizeof(header)) {
      close(peer);
      return;
    }
    const auto size = static_cast<size_t>(ntohl(header));
    std::string credentials(size, '\0');
    ASSERT_EQ(read(peer, credentials.data(), size), static_cast<ssize_t>(size));
    EXPECT_EQ(credentials,
              "4002\x1e"
              "pam-user\x1e"
              "pam-password");
    WriteFrame(peer,
               "1000\x1e"
               "OK: bareos-dir");
    WriteFrame(peer,
               "1002\x1e"
               "additional info");
    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto command_size = static_cast<size_t>(ntohl(header));
    std::string command(command_size, '\0');
    ASSERT_EQ(read(peer, command.data(), command_size),
              static_cast<ssize_t>(command_size));
    EXPECT_EQ(command, "whoami\n");
    WriteFrame(peer, "pam-user\n");
    WriteSignal(peer, BNET_MAIN_PROMPT);
    close(peer);
  });

  try {
    connection.FinishAuthentication(cfg);
    EXPECT_EQ(connection.Call("whoami").text, "pam-user\n");
  } catch (const std::exception& ex) {
    ADD_FAILURE() << ex.what();
  }
  shutdown(connection.fd_, SHUT_RDWR);
  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}

TEST(DirectorConnection, RejectsUnsafeOrFailedPamWithoutLeakingCredentials)
{
  struct Scenario {
    bool tls;
    std::optional<PamCredentials> credentials;
    std::string final_message;
    bool sends_credentials;
    int32_t final_signal = 0;
  };
  const Scenario scenarios[]{
      {false, PamCredentials{"pam-user", "pam-secret"}, {}, false},
      {true, std::nullopt, {}, false},
      {true, PamCredentials{"", "pam-secret"}, {}, false},
      {true, PamCredentials{"pam-user", ""}, {}, false},
      {true,
       PamCredentials{"bad\x1e"
                      "user",
                      "pam-secret"},
       {},
       false},
      {true,
       PamCredentials{"pam-user", std::string("bad\0secret", 10)},
       {},
       false},
      {true, PamCredentials{"pam-user", "pam-secret"},
       "1999 Authorization failed", true},
      {true, PamCredentials{"pam-user", "pam-secret"}, {}, true},
      {true,
       PamCredentials{"pam-user", "pam-secret"},
       {},
       true,
       BNET_TERMINATE},
  };
  for (const auto& scenario : scenarios) {
    int sockets[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    DirectorConnection connection;
    connection.fd_ = sockets[0];
    connection.tls_psk_active_ = scenario.tls;
    DirectorConfig cfg;
    cfg.json_mode = false;
    cfg.pam_credentials = scenario.credentials;
    WriteFrame(sockets[1],
               "1001\x1e"
               "PAM required");
    if (!scenario.final_message.empty()) {
      WriteFrame(sockets[1], scenario.final_message);
    }
    if (scenario.final_signal) {
      WriteSignal(sockets[1], scenario.final_signal);
    }
    shutdown(sockets[1], SHUT_WR);

    try {
      connection.FinishAuthentication(cfg);
      ADD_FAILURE() << "PAM should have failed";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(message.find("PAM authentication failed"), std::string::npos);
      EXPECT_EQ(message.find("pam-secret"), std::string::npos);
    }
    char buffer[256]{};
    const auto sent = recv(sockets[1], buffer, sizeof(buffer), MSG_DONTWAIT);
    EXPECT_EQ(sent > 0, scenario.sends_credentials);
    close(sockets[1]);
    close(connection.fd_);
    connection.fd_ = -1;
  }
}

TEST(DirectorConnection, RejectsPamLoginWhenConsoleDoesNotRequirePam)
{
  for (const std::string welcome :
       {"1000\x1e"
        "OK",
        "10001 wrong", "invalid 1000", "1000invalid"}) {
    int sockets[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    DirectorConnection connection;
    connection.fd_ = sockets[0];
    DirectorConfig cfg;
    cfg.json_mode = false;
    cfg.pam_credentials = PamCredentials{"pam-user", "pam-secret"};
    WriteFrame(sockets[1], welcome);
    WriteFrame(sockets[1],
               "1002\x1e"
               "info");
    EXPECT_THROW(connection.FinishAuthentication(cfg), std::runtime_error);
    char buffer[256]{};
    EXPECT_LT(recv(sockets[1], buffer, sizeof(buffer), MSG_DONTWAIT), 0);
    close(sockets[1]);
    close(connection.fd_);
    connection.fd_ = -1;
  }
}

TEST(DirectorConnection, KeepsRestoreTreePromptDataWithSameCommand)
{
  int sockets[2] = {-1, -1};
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  DirectorConnection connection;
  connection.fd_ = sockets[0];
  connection.json_mode_ = false;

  std::thread director([peer = sockets[1]]() {
    int32_t header = 0;
    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));

    const auto payload_size = static_cast<size_t>(ntohl(header));
    std::string payload(payload_size, '\0');
    ASSERT_EQ(read(peer, payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    EXPECT_EQ(payload, "find *\n");

    WriteFrame(peer, "selection ready\n");
    WriteSignal(peer, BNET_START_RTREE);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    WriteFrame(peer, "cwd is: /\n$ ");
    WriteSignal(peer, BNET_SUB_PROMPT);
    close(peer);
  });

  const auto result = connection.Call("find *");

  EXPECT_EQ(result.text, "selection ready\ncwd is: /\n$ ");
  EXPECT_EQ(result.prompt, DirectorPrompt::Sub);

  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}

TEST(DirectorConnection, StreamsChunksWithoutAccumulatingInCallStreamed)
{
  int sockets[2] = {-1, -1};
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  DirectorConnection connection;
  connection.fd_ = sockets[0];
  connection.json_mode_ = false;

  std::thread director([peer = sockets[1]]() {
    int32_t header = 0;
    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));

    const auto payload_size = static_cast<size_t>(ntohl(header));
    std::string payload(payload_size, '\0');
    ASSERT_EQ(read(peer, payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    EXPECT_EQ(payload, "status dir\n");

    WriteFrame(peer, "first chunk\n");
    WriteFrame(peer, "second chunk\n");
    WriteSignal(peer, BNET_MAIN_PROMPT);
    close(peer);
  });

  std::string streamed;
  const auto prompt = connection.CallStreamed(
      "status dir", [&](std::string_view chunk) { streamed.append(chunk); });

  EXPECT_EQ(streamed, "first chunk\nsecond chunk\n");
  EXPECT_EQ(prompt, DirectorPrompt::Main);

  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}

TEST(DirectorConnection, UsesCompatibleDirectorIdentityResponse)
{
  int sockets[2] = {-1, -1};
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  DirectorConnection connection;
  connection.fd_ = sockets[0];

  DirectorConfig cfg;
  cfg.username = "admin";
  cfg.password = "secret";
  cfg.json_mode = false;

  std::thread director([peer = sockets[1], cfg]() {
    int32_t header = 0;

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto hello_size = static_cast<size_t>(ntohl(header));
    std::string hello(hello_size, '\0');
    ASSERT_EQ(read(peer, hello.data(), hello.size()),
              static_cast<ssize_t>(hello.size()));
    EXPECT_NE(hello.find("Hello admin calling version "), std::string::npos);

    WriteFrame(peer, "auth cram-md5 <director-challenge> ssl=0\n");

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto response_size = static_cast<size_t>(ntohl(header));
    std::string response(response_size, '\0');
    ASSERT_EQ(read(peer, response.data(), response.size()),
              static_cast<ssize_t>(response.size()));

    const std::string key = MakeCramMd5Key(cfg.password);
    auto expected_client_hmac = HmacMd5Digest(key, "<director-challenge>");
    ASSERT_EQ(expected_client_hmac.size(), 16U);
    EXPECT_EQ(response, BareosBase64Encode(expected_client_hmac.data(),
                                           expected_client_hmac.size()));

    WriteFrame(peer, "1000 OK auth\n");

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto challenge_size = static_cast<size_t>(ntohl(header));
    std::string challenge_msg(challenge_size, '\0');
    ASSERT_EQ(read(peer, challenge_msg.data(), challenge_msg.size()),
              static_cast<ssize_t>(challenge_msg.size()));
    ASSERT_NE(challenge_msg.find("auth cram-md5c <"), std::string::npos);

    const std::string prefix = "auth cram-md5c ";
    const auto challenge_begin = challenge_msg.find(prefix);
    ASSERT_NE(challenge_begin, std::string::npos);
    const auto challenge_end = challenge_msg.find(" ssl=", challenge_begin);
    ASSERT_NE(challenge_end, std::string::npos);
    const std::string challenge = challenge_msg.substr(
        challenge_begin + prefix.size(),
        challenge_end - (challenge_begin + prefix.size()));

    auto expected_director_hmac = HmacMd5Digest(key, challenge);
    ASSERT_EQ(expected_director_hmac.size(), 16U);
    WriteFrame(peer, BareosBase64Encode(expected_director_hmac.data(),
                                        expected_director_hmac.size()));

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto auth_ok_size = static_cast<size_t>(ntohl(header));
    std::string auth_ok(auth_ok_size, '\0');
    ASSERT_EQ(read(peer, auth_ok.data(), auth_ok.size()),
              static_cast<ssize_t>(auth_ok.size()));
    EXPECT_EQ(auth_ok, "1000 OK auth\n");

    WriteFrame(peer, "1000 OK: bareos-dir Version: 26.0.0\n");
    WriteFrame(peer, "1002 additional info\n");
    WriteSignal(peer, BNET_EOD);
    close(peer);
  });

  EXPECT_NO_THROW(connection.Authenticate(cfg));

  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}

TEST(DirectorConnection, RejectsUnexpectedPostAuthInfoFrame)
{
  int sockets[2] = {-1, -1};
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  DirectorConnection connection;
  connection.fd_ = sockets[0];

  DirectorConfig cfg;
  cfg.username = "admin";
  cfg.password = "secret";
  cfg.json_mode = false;

  std::thread director([peer = sockets[1], cfg]() {
    int32_t header = 0;

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto hello_size = static_cast<size_t>(ntohl(header));
    std::string hello(hello_size, '\0');
    ASSERT_EQ(read(peer, hello.data(), hello.size()),
              static_cast<ssize_t>(hello.size()));

    WriteFrame(peer, "auth cram-md5 <director-challenge> ssl=0\n");

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto response_size = static_cast<size_t>(ntohl(header));
    std::string response(response_size, '\0');
    ASSERT_EQ(read(peer, response.data(), response.size()),
              static_cast<ssize_t>(response.size()));

    const std::string key = MakeCramMd5Key(cfg.password);
    auto expected_client_hmac = HmacMd5Digest(key, "<director-challenge>");
    ASSERT_EQ(expected_client_hmac.size(), 16U);
    EXPECT_EQ(response, BareosBase64Encode(expected_client_hmac.data(),
                                           expected_client_hmac.size()));

    WriteFrame(peer, "1000 OK auth\n");

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto challenge_size = static_cast<size_t>(ntohl(header));
    std::string challenge_msg(challenge_size, '\0');
    ASSERT_EQ(read(peer, challenge_msg.data(), challenge_msg.size()),
              static_cast<ssize_t>(challenge_msg.size()));

    const std::string prefix = "auth cram-md5c ";
    const auto challenge_begin = challenge_msg.find(prefix);
    ASSERT_NE(challenge_begin, std::string::npos);
    const auto challenge_end = challenge_msg.find(" ssl=", challenge_begin);
    ASSERT_NE(challenge_end, std::string::npos);
    const std::string challenge = challenge_msg.substr(
        challenge_begin + prefix.size(),
        challenge_end - (challenge_begin + prefix.size()));

    auto expected_director_hmac = HmacMd5Digest(key, challenge);
    ASSERT_EQ(expected_director_hmac.size(), 16U);
    WriteFrame(peer, BareosBase64Encode(expected_director_hmac.data(),
                                        expected_director_hmac.size()));

    ASSERT_EQ(read(peer, &header, sizeof(header)), sizeof(header));
    const auto auth_ok_size = static_cast<size_t>(ntohl(header));
    std::string auth_ok(auth_ok_size, '\0');
    ASSERT_EQ(read(peer, auth_ok.data(), auth_ok.size()),
              static_cast<ssize_t>(auth_ok.size()));
    EXPECT_EQ(auth_ok, "1000 OK auth\n");

    WriteFrame(peer, "1000 OK: bareos-dir Version: 26.0.0\n");
    WriteFrame(peer, "1999 unexpected trailing frame\n");
    close(peer);
  });

  try {
    connection.Authenticate(cfg);
    FAIL() << "expected Authenticate to fail";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("unexpected info message"),
              std::string::npos);
  }

  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}
