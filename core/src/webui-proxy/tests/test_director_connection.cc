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

#include "ascii_control_characters.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

TEST(DirectorConnection, BuildsTlsPskIdentityForConsole)
{
  const std::string identity = GetDirectorTlsPskIdentity("admin-tls");
  std::string expected = "R_CONSOLE";
  expected.push_back(AsciiControlCharacters::RecordSeparator());
  expected.append("admin-tls");

  EXPECT_EQ(identity, expected);
}

TEST(DirectorConnection, UsesMd5HexPasswordAsTlsPsk)
{
  EXPECT_EQ(GetDirectorTlsPskSecret("secret"),
            "5ebe2294ecd0e0f08eab7690d2a6ee69");
}

TEST(DirectorConfig, EnablesTlsPskByDefault)
{
  DirectorConfig cfg;

  EXPECT_FALSE(cfg.tls_psk_disable);
}

TEST(DirectorConfig, AllowsDisablingTlsPsk)
{
  DirectorConfig cfg;

  cfg.tls_psk_disable = true;

  EXPECT_TRUE(cfg.tls_psk_disable);
}

TEST(DirectorConnection, StartsWithoutTlsPskTransport)
{
  DirectorConnection connection;

  EXPECT_FALSE(connection.UsesTlsPsk());
}

namespace {
constexpr int32_t kBnetStartRtree = -25;
constexpr int32_t kBnetSubPrompt = -27;

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
    WriteSignal(peer, kBnetStartRtree);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    WriteFrame(peer, "cwd is: /\n$ ");
    WriteSignal(peer, kBnetSubPrompt);
    close(peer);
  });

  const auto result = connection.Call("find *");

  EXPECT_EQ(result.text, "selection ready\ncwd is: /\n$ ");
  EXPECT_EQ(result.prompt, DirectorPrompt::Sub);

  director.join();
  close(connection.fd_);
  connection.fd_ = -1;
}
