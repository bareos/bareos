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

#include "../proxy_session.h"

#include <sys/socket.h>
#include <unistd.h>

#include <jansson.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr std::string_view kValidHandshakeRequest
    = "GET /ws HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";

class SocketPair {
 public:
  SocketPair() { EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0); }

  ~SocketPair()
  {
    if (fds_[0] >= 0) { close(fds_[0]); }
    if (fds_[1] >= 0) { close(fds_[1]); }
  }

  int local() const { return fds_[0]; }
  int peer() const { return fds_[1]; }

  int ReleaseLocal()
  {
    const int fd = fds_[0];
    fds_[0] = -1;
    return fd;
  }

 private:
  int fds_[2] = {-1, -1};
};

void WriteAll(int fd, const void* data, size_t size)
{
  const auto* ptr = static_cast<const char*>(data);
  size_t remaining = size;
  while (remaining > 0) {
    const auto written = ::write(fd, ptr, remaining);
    ASSERT_GT(written, 0);
    ptr += written;
    remaining -= static_cast<size_t>(written);
  }
}

std::string ReadExact(int fd, size_t size)
{
  std::string result(size, '\0');
  size_t offset = 0;
  while (offset < size) {
    const auto received = ::recv(fd, result.data() + offset, size - offset, 0);
    if (received <= 0) {
      ADD_FAILURE() << "recv() failed while reading " << size << " bytes";
      return {};
    }
    offset += static_cast<size_t>(received);
  }
  return result;
}

std::string ReadUntilMarker(int fd, std::string_view marker)
{
  std::string result;
  std::array<char, 256> buffer{};
  while (result.find(marker) == std::string::npos) {
    const auto received = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (received <= 0) {
      ADD_FAILURE() << "recv() failed while reading HTTP response";
      return result;
    }
    result.append(buffer.data(), static_cast<size_t>(received));
  }
  return result;
}

std::string BuildMaskedFrame(uint8_t opcode,
                             std::string_view payload,
                             bool fin = true)
{
  std::string frame;
  frame.push_back(static_cast<char>((fin ? 0x80u : 0u) | (opcode & 0x0Fu)));

  constexpr std::array<unsigned char, 4> kMask = {0x01u, 0x02u, 0x03u, 0x04u};
  const size_t len = payload.size();
  if (len <= 125) {
    frame.push_back(static_cast<char>(0x80u | len));
  } else if (len <= 65535) {
    frame.push_back(static_cast<char>(0x80u | 126u));
    frame.push_back(static_cast<char>((len >> 8) & 0xFFu));
    frame.push_back(static_cast<char>(len & 0xFFu));
  } else {
    frame.push_back(static_cast<char>(0x80u | 127u));
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFFu));
    }
  }

  frame.append(reinterpret_cast<const char*>(kMask.data()), kMask.size());
  for (size_t i = 0; i < payload.size(); ++i) {
    frame.push_back(static_cast<char>(payload[i] ^ kMask[i % kMask.size()]));
  }
  return frame;
}

std::string ReadServerTextFrame(int fd)
{
  const auto header = ReadExact(fd, 2);
  const auto first = static_cast<unsigned char>(header[0]);
  const auto second = static_cast<unsigned char>(header[1]);
  EXPECT_EQ(first & 0x0Fu, 0x01u);
  EXPECT_EQ(second & 0x80u, 0u);

  uint64_t payload_size = second & 0x7Fu;
  if (payload_size == 126u) {
    const auto extended = ReadExact(fd, 2);
    payload_size
        = (static_cast<uint64_t>(static_cast<unsigned char>(extended[0])) << 8)
          | static_cast<unsigned char>(extended[1]);
  } else if (payload_size == 127u) {
    const auto extended = ReadExact(fd, 8);
    payload_size = 0;
    for (const char byte : extended) {
      payload_size = (payload_size << 8) | static_cast<unsigned char>(byte);
    }
  }

  return ReadExact(fd, static_cast<size_t>(payload_size));
}

std::string GetJsonStringField(std::string_view text, const char* key)
{
  json_error_t error{};
  json_t* root = json_loadb(text.data(), text.size(), 0, &error);
  if (!root) {
    ADD_FAILURE() << "failed to parse JSON response";
    return {};
  }
  std::unique_ptr<json_t, decltype(&json_decref)> guard(root, &json_decref);
  const char* value = json_string_value(json_object_get(root, key));
  if (!value) {
    ADD_FAILURE() << "missing JSON string field: " << key;
    return {};
  }
  return value ? std::string(value) : std::string();
}

std::vector<std::string> GetJsonStringArrayField(std::string_view text,
                                                 const char* key)
{
  json_error_t error{};
  json_t* root = json_loadb(text.data(), text.size(), 0, &error);
  if (!root) {
    ADD_FAILURE() << "failed to parse JSON response";
    return {};
  }
  std::unique_ptr<json_t, decltype(&json_decref)> guard(root, &json_decref);
  json_t* array = json_object_get(root, key);
  if (!json_is_array(array)) {
    ADD_FAILURE() << "missing JSON string array field: " << key;
    return {};
  }

  std::vector<std::string> values;
  values.reserve(json_array_size(array));
  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(array, index, entry)
  {
    const char* value = json_string_value(entry);
    if (!value) {
      ADD_FAILURE() << "non-string JSON array element at index " << index;
      return {};
    }
    values.emplace_back(value);
  }
  return values;
}

std::string ExchangeMessage(const ProxyConfig& config, std::string_view message)
{
  SocketPair sockets;
  const int server_fd = sockets.ReleaseLocal();
  std::thread session([server_fd, &config]() {
    RunProxySession(server_fd, "test-peer", config);
  });

  WriteAll(sockets.peer(), kValidHandshakeRequest.data(),
           kValidHandshakeRequest.size());
  const auto handshake = ReadUntilMarker(sockets.peer(), "\r\n\r\n");
  EXPECT_NE(handshake.find("HTTP/1.1 101 Switching Protocols"),
            std::string::npos);

  const auto frame = BuildMaskedFrame(0x1u, message);
  WriteAll(sockets.peer(), frame.data(), frame.size());

  const auto response = ReadServerTextFrame(sockets.peer());
  session.join();
  return response;
}

std::string ExchangeAuthMessage(std::string_view auth_message)
{
  const ProxyConfig config;
  return ExchangeMessage(config, auth_message);
}

}  // namespace

TEST(ProxySession, TrimsWhitespaceAroundCommands)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("  list jobs \r\n"), "list jobs");
}

TEST(ProxySession, PreservesTrailingTabForCompletion)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("  list cl\t \r\n"), "list cl\t");
}

TEST(ProxySession, PreservesStandaloneTabCompletion)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("\t"), "\t");
}

TEST(ProxySession, TreatsQuitAsExpectedMainPromptExit)
{
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, "quit"));
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, "exit"));
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, ".quit"));
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, ".exit"));
}

TEST(ProxySession, DoesNotTreatSubPromptExitAsConsoleClose)
{
  EXPECT_FALSE(IsExpectedConsoleExitCommand(false, "quit"));
  EXPECT_FALSE(IsExpectedConsoleExitCommand(false, "exit"));
  EXPECT_FALSE(IsExpectedConsoleExitCommand(true, "done"));
}

TEST(ProxySession, SuppressesOutOfBandMessagesNotification)
{
  EXPECT_TRUE(
      ShouldSuppressRawConsoleChunk("messages", "You have messages.\n"));
  EXPECT_TRUE(ShouldSuppressRawConsoleChunk("mess", "You have messages.\n"));
  EXPECT_TRUE(ShouldSuppressRawConsoleChunk("MESSAGE", "You have messages.\n"));
  EXPECT_FALSE(ShouldSuppressRawConsoleChunk("help", "You have messages.\n"));
  EXPECT_FALSE(ShouldSuppressRawConsoleChunk("mes", "You have messages.\n"));
  EXPECT_FALSE(
      ShouldSuppressRawConsoleChunk("messages", "You have no messages.\n"));
}

TEST(ProxySession, FiltersMessagesNotificationFromCommandOutput)
{
  EXPECT_EQ(FilterRawConsoleChunk("messages", "You have messages.\n"), "");
  EXPECT_EQ(FilterRawConsoleChunk(
                "messages", "You have no messages.\nYou have messages.\n"),
            "You have no messages.\n");
  EXPECT_EQ(FilterRawConsoleChunk("messages", "You have messages."), "");
  EXPECT_EQ(FilterRawConsoleChunk("help", "You have messages.\n"),
            "You have messages.\n");
}

TEST(ProxySession, ListsConfiguredDirectors)
{
  ProxyConfig config;
  config.configured_directors.emplace(
      "prod",
      DirectorTargetConfig{
          .host = "prod.example.test", .port = 19101, .name = "bareos-dir"});
  config.configured_directors.emplace(
      "dr", DirectorTargetConfig{
                .host = "dr.example.test", .port = 29101, .name = "dr-dir"});

  const auto response = ExchangeMessage(config, R"({"type":"list_directors"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "director_list");
  EXPECT_EQ(GetJsonStringArrayField(response, "directors"),
            std::vector<std::string>({"dr", "prod"}));
}

TEST(ProxySession, RequiresExplicitUsernameInAuthMessage)
{
  const auto response = ExchangeAuthMessage(
      R"({"type":"auth","password":"secret","mode":"json"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_error");
  EXPECT_EQ(GetJsonStringField(response, "message"),
            "Auth message requires string field 'username'");
}

TEST(ProxySession, RequiresExplicitPasswordInAuthMessage)
{
  const auto response = ExchangeAuthMessage(
      R"({"type":"auth","username":"admin","mode":"json"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_error");
  EXPECT_EQ(GetJsonStringField(response, "message"),
            "Auth message requires string field 'password'");
}

TEST(ProxySession, RejectsEmptyDirectorInAuthMessage)
{
  ProxyConfig config;
  config.configured_directors.emplace(
      "prod",
      DirectorTargetConfig{
          .host = "prod.example.test", .port = 19101, .name = "bareos-dir"});

  const auto response = ExchangeMessage(
      config,
      R"({"type":"auth","username":"admin","password":"secret","director":"","mode":"json"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_error");
  EXPECT_EQ(GetJsonStringField(response, "message"),
            "Proxy config: no director selected");
}

TEST(ProxySession, RejectsUnknownDirectorInAuthMessage)
{
  ProxyConfig config;
  config.configured_directors.emplace(
      "prod",
      DirectorTargetConfig{
          .host = "prod.example.test", .port = 19101, .name = "bareos-dir"});

  const auto response = ExchangeMessage(
      config,
      R"({"type":"auth","username":"admin","password":"secret","director":"other-dir","mode":"json"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_error");
  EXPECT_EQ(GetJsonStringField(response, "message"),
            "Proxy config: director 'other-dir' is not in the allowlist");
}

TEST(ProxySession, RejectsUnsupportedAuthMode)
{
  const auto response = ExchangeAuthMessage(
      R"({"type":"auth","username":"admin","password":"secret","director":"bareos-dir","mode":"json-v2"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_error");
  EXPECT_EQ(GetJsonStringField(response, "message"),
            "Auth message has unsupported mode 'json-v2'");
}
