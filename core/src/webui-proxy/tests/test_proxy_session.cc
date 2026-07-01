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

#include "../proxy_auth_session.h"
#include "../director_connection.h"
#include "../proxy_session.h"
#include "../bareos_base64.h"

#include "lib/cram_md5.h"
#include "lib/bnet_protocol_signals.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <jansson.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <utility>
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

  int ReleasePeer()
  {
    const int fd = fds_[1];
    fds_[1] = -1;
    return fd;
  }

 private:
  int fds_[2] = {-1, -1};
};

struct TcpListener {
  int fd{-1};
  uint16_t port{0};

  ~TcpListener()
  {
    if (fd >= 0) { close(fd); }
  }
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

std::string ReadUntilClosed(int fd)
{
  std::string result;
  std::array<char, 256> buffer{};
  while (true) {
    const auto received = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (received < 0) {
      ADD_FAILURE() << "recv() failed while reading response";
      return result;
    }
    if (received == 0) { return result; }
    result.append(buffer.data(), static_cast<size_t>(received));
  }
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

TcpListener CreateMockDirectorListener()
{
  TcpListener listener;
  listener.fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listener.fd < 0) {
    throw std::runtime_error("failed to create mock director socket");
  }

  int reuse = 1;
  if (setsockopt(listener.fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
      != 0) {
    throw std::runtime_error("failed to configure mock director socket");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(listener.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
      != 0) {
    throw std::runtime_error("failed to bind mock director socket");
  }

  socklen_t len = sizeof(addr);
  if (getsockname(listener.fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    throw std::runtime_error("failed to query mock director port");
  }
  listener.port = ntohs(addr.sin_port);

  if (listen(listener.fd, 1) != 0) {
    throw std::runtime_error("failed to listen on mock director socket");
  }

  return listener;
}

std::thread StartMockDirectorSession(const TcpListener& listener,
                                     std::string password)
{
  return std::thread([fd = listener.fd, password = std::move(password)]() {
    const int peer = ::accept(fd, nullptr, nullptr);
    ASSERT_GE(peer, 0);

    auto read_frame = [peer]() {
      const auto header = ReadExact(peer, 4);
      const auto size = static_cast<int32_t>(
          (static_cast<uint32_t>(static_cast<unsigned char>(header[0])) << 24)
          | (static_cast<uint32_t>(static_cast<unsigned char>(header[1])) << 16)
          | (static_cast<uint32_t>(static_cast<unsigned char>(header[2])) << 8)
          | static_cast<uint32_t>(static_cast<unsigned char>(header[3])));
      if (size <= 0) { return std::string(); }
      return ReadExact(peer, static_cast<size_t>(size));
    };

    auto write_frame = [peer](std::string_view payload) {
      const int32_t header = htonl(static_cast<int32_t>(payload.size()));
      WriteAll(peer, &header, sizeof(header));
      if (!payload.empty()) { WriteAll(peer, payload.data(), payload.size()); }
    };

    const std::string hello = read_frame();
    EXPECT_NE(hello.find("Hello admin calling version "), std::string::npos);

    const std::string key = MakeCramMd5Key(password);
    const std::string director_challenge = "<director-challenge>";
    write_frame("auth cram-md5 " + director_challenge + " ssl=0\n");

    const std::string response = read_frame();
    auto expected_client_hmac = HmacMd5Digest(key, director_challenge);
    EXPECT_EQ(response, BareosBase64Encode(expected_client_hmac.data(),
                                           expected_client_hmac.size()));

    write_frame("1000 OK auth\n");

    const std::string challenge_msg = read_frame();
    const std::string prefix = "auth cram-md5c ";
    const auto challenge_begin = challenge_msg.find(prefix);
    EXPECT_NE(challenge_begin, std::string::npos);
    const auto challenge_end = challenge_msg.find(" ssl=", challenge_begin);
    EXPECT_NE(challenge_end, std::string::npos);
    const std::string challenge = challenge_msg.substr(
        challenge_begin + prefix.size(),
        challenge_end - (challenge_begin + prefix.size()));

    auto expected_director_hmac = HmacMd5Digest(key, challenge);
    write_frame(BareosBase64Encode(expected_director_hmac.data(),
                                   expected_director_hmac.size()));

    const std::string auth_ok = read_frame();
    EXPECT_EQ(auth_ok, "1000 OK auth\n");

    write_frame("1000 OK: bareos-dir Version: 26.0.0\n");
    write_frame("1002 additional info\n");
    const auto command = read_frame();
    EXPECT_EQ(command, ".api json\n");
    const int32_t main_prompt = htonl(BNET_MAIN_PROMPT);
    WriteAll(peer, &main_prompt, sizeof(main_prompt));
    ::close(peer);
  });
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

std::string BuildWsHandshakeRequest(std::string_view cookie_header = {})
{
  std::string request(kValidHandshakeRequest);
  if (!cookie_header.empty()) {
    request.insert(
        request.size() - 2,
        std::string("Cookie: ") + std::string(cookie_header) + "\r\n");
  }
  return request;
}

std::string ExchangeWsSession(const ProxyConfig& config,
                              std::string_view cookie_header,
                              std::string_view session_message)
{
  SocketPair sockets;
  const int server_fd = sockets.ReleaseLocal();
  std::thread session([server_fd, &config]() {
    RunProxySession(server_fd, "test-peer", config);
  });

  const auto request = BuildWsHandshakeRequest(cookie_header);
  WriteAll(sockets.peer(), request.data(), request.size());
  const auto handshake = ReadUntilMarker(sockets.peer(), "\r\n\r\n");
  if (handshake.find("HTTP/1.1 101 Switching Protocols") == std::string::npos) {
    ::close(sockets.ReleasePeer());
    session.join();
    return handshake;
  }

  const auto frame = BuildMaskedFrame(0x1u, session_message);
  WriteAll(sockets.peer(), frame.data(), frame.size());

  const auto response = ReadServerTextFrame(sockets.peer());
  ::close(sockets.ReleasePeer());
  session.join();
  return response;
}

std::string ExchangeHttpRequest(const ProxyConfig& config,
                                std::string_view request_text)
{
  SocketPair sockets;
  const int server_fd = sockets.ReleaseLocal();
  std::thread session([server_fd, &config]() {
    RunProxySession(server_fd, "test-peer", config);
  });

  WriteAll(sockets.peer(), request_text.data(), request_text.size());
  const auto response = ReadUntilClosed(sockets.peer());
  session.join();
  return response;
}

std::string HttpBody(std::string_view response)
{
  const auto header_end = response.find("\r\n\r\n");
  if (header_end == std::string_view::npos) {
    ADD_FAILURE() << "missing HTTP header terminator";
    return {};
  }
  return std::string(response.substr(header_end + 4));
}

std::string HttpStatusLine(std::string_view response)
{
  const auto line_end = response.find("\r\n");
  if (line_end == std::string_view::npos) {
    ADD_FAILURE() << "missing HTTP status line";
    return {};
  }
  return std::string(response.substr(0, line_end));
}

std::vector<std::string> GetAuthenticatedDirectors(std::string_view text)
{
  json_error_t error{};
  json_t* root = json_loadb(text.data(), text.size(), 0, &error);
  if (!root) {
    ADD_FAILURE() << "failed to parse JSON response";
    return {};
  }
  std::unique_ptr<json_t, decltype(&json_decref)> guard(root, &json_decref);
  json_t* array = json_object_get(root, "authenticatedDirectors");
  if (!json_is_array(array)) {
    ADD_FAILURE() << "missing authenticatedDirectors";
    return {};
  }

  std::vector<std::string> directors;
  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(array, index, entry)
  {
    const char* director
        = json_string_value(json_object_get(entry, "director"));
    if (!director) {
      ADD_FAILURE() << "missing director entry";
      return {};
    }
    directors.emplace_back(director);
  }
  return directors;
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

TEST(ProxySession, NormalizesConsoleTextForJson)
{
  std::string invalid_utf8 = "media ";
  invalid_utf8.push_back(static_cast<char>(0xC3));
  invalid_utf8.push_back('(');
  invalid_utf8.push_back('\0');
  invalid_utf8 += "name";
  EXPECT_EQ(NormalizeJsonText(invalid_utf8), "media \xEF\xBF\xBD( name");
}

TEST(ProxySession, ListsConfiguredDirectors)
{
  ProxyConfig config;
  config.configured_directors.emplace(
      "bareos-dir",
      DirectorTargetConfig{
          .address = "prod.example.test", .port = 19101, .name = "bareos-dir"});
  config.configured_directors.emplace(
      "site-b",
      DirectorTargetConfig{
          .address = "dr.example.test", .port = 29101, .name = "bareos-dir"});

  const auto response = ExchangeHttpRequest(config,
                                            "GET /api/directors HTTP/1.1\r\n"
                                            "Host: localhost\r\n\r\n");

  EXPECT_EQ(GetJsonStringField(HttpBody(response), "type"), "director_list");
  EXPECT_EQ(GetJsonStringArrayField(HttpBody(response), "directors"),
            std::vector<std::string>({"bareos-dir", "site-b"}));
}

TEST(ProxySession, RejectsWsWithoutSessionCookie)
{
  const auto response
      = ExchangeHttpRequest(ProxyConfig{},
                            "GET /ws HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                            "Sec-WebSocket-Version: 13\r\n\r\n");

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 401 Unauthorized");
}

TEST(ProxySession, ConnectsWsWithSessionCookie)
{
  const auto listener = CreateMockDirectorListener();

  ProxyConfig config;
  config.configured_directors.emplace(
      "bareos-dir",
      DirectorTargetConfig{.address = "127.0.0.1",
                           .port = static_cast<int>(listener.port),
                           .name = "bareos-dir",
                           .tls_psk_disable = true});
  const auto session_id
      = ProxyAuthSessionStore::CreateSession("admin", "secret", "bareos-dir");
  ASSERT_TRUE(ProxyAuthSessionStore::StoreDirectorCredentials(
      session_id, "bareos-dir", "admin", "secret"));

  auto director = StartMockDirectorSession(listener, "secret");
  const auto response = ExchangeWsSession(
      config, std::string("bareos_proxy_session=") + session_id,
      R"({"type":"session","director":"bareos-dir","mode":"json"})");

  director.join();
  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_ok");
  EXPECT_EQ(GetJsonStringField(response, "director"), "bareos-dir");
  ProxyAuthSessionStore::RemoveSession(session_id);
}

TEST(ProxySession, RejectsUnsupportedWsMode)
{
  ProxyConfig config;
  config.configured_directors.emplace(
      "bareos-dir",
      DirectorTargetConfig{
          .address = "127.0.0.1", .port = 19101, .name = "bareos-dir"});
  const auto session_id
      = ProxyAuthSessionStore::CreateSession("admin", "secret", "bareos-dir");
  ASSERT_TRUE(ProxyAuthSessionStore::StoreDirectorCredentials(
      session_id, "bareos-dir", "admin", "secret"));

  const auto response = ExchangeWsSession(
      config, std::string("bareos_proxy_session=") + session_id,
      R"({"type":"session","director":"bareos-dir","mode":"json-v2"})");

  EXPECT_EQ(GetJsonStringField(response, "type"), "auth_error");
  EXPECT_NE(GetJsonStringField(response, "message").find("unsupported mode"),
            std::string::npos);
  ProxyAuthSessionStore::RemoveSession(session_id);
}

TEST(ProxySession, RejectsLoginWithExpiredSessionCookie)
{
  // Simulate an expired session: cookie is present but session is gone.
  const std::string body = R"({"username":"admin","password":"secret"})";
  const std::string request
      = std::string(
            "POST /api/session/directors/bareos-dir/login HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Cookie: bareos_proxy_session=nonexistent-session-id\r\n"
            "Content-Length: ")
        + std::to_string(body.size()) + "\r\n\r\n" + body;
  const auto response = ExchangeHttpRequest(ProxyConfig{}, request);

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("Max-Age=0"), std::string::npos);
}

TEST(ProxySession, ReturnsNotFoundForNonWsTargets)
{
  const auto response = ExchangeHttpRequest(
      ProxyConfig{}, "GET /not-ws HTTP/1.1\r\nHost: localhost\r\n\r\n");

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 404 Not Found");
}

TEST(ProxySession, RequiresWebSocketUpgradeForWsTarget)
{
  const auto response = ExchangeHttpRequest(
      ProxyConfig{}, "GET /ws HTTP/1.1\r\nHost: localhost\r\n\r\n");

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 426 Upgrade Required");
  EXPECT_NE(response.find("Upgrade: websocket"), std::string::npos);
}

TEST(ProxySession, ReturnsMultiDirectorSessionInfoOverHttp)
{
  const auto session_id
      = ProxyAuthSessionStore::CreateSession("admin", "secret", "bareos-dir");
  ASSERT_TRUE(ProxyAuthSessionStore::StoreDirectorCredentials(
      session_id, "site-b", "ops", "site-secret"));

  const std::string request = std::string(
                                  "GET /api/session HTTP/1.1\r\n"
                                  "Host: localhost\r\n"
                                  "Cookie: bareos_proxy_session=")
                              + session_id + "\r\n\r\n";
  const auto response = ExchangeHttpRequest(ProxyConfig{}, request);

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(GetAuthenticatedDirectors(HttpBody(response)),
            std::vector<std::string>({"bareos-dir", "site-b"}));

  ProxyAuthSessionStore::RemoveSession(session_id);
}

TEST(ProxySession, DeleteSessionEndsTheWholeSession)
{
  const auto session_id
      = ProxyAuthSessionStore::CreateSession("admin", "secret", "bareos-dir");

  const std::string request = std::string(
                                  "DELETE /api/session HTTP/1.1\r\n"
                                  "Host: localhost\r\n"
                                  "Cookie: bareos_proxy_session=")
                              + session_id + "\r\n\r\n";
  const auto response = ExchangeHttpRequest(ProxyConfig{}, request);

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 204 No Content");
  EXPECT_NE(response.find("Max-Age=0"), std::string::npos);
  EXPECT_FALSE(ProxyAuthSessionStore::LookupSession(session_id));
}

TEST(ProxySession, ReuseEndpointReportsAlreadyAuthenticatedDirectors)
{
  const auto session_id
      = ProxyAuthSessionStore::CreateSession("admin", "secret", "bareos-dir");
  ASSERT_TRUE(ProxyAuthSessionStore::StoreDirectorCredentials(
      session_id, "site-b", "admin", "secret"));

  const std::string body
      = R"({"directors":["site-b"],"sourceDirector":"bareos-dir"})";
  const std::string request = std::string(
                                  "POST /api/session/reuse HTTP/1.1\r\n"
                                  "Host: localhost\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Cookie: bareos_proxy_session=")
                              + session_id + "\r\nContent-Length: "
                              + std::to_string(body.size()) + "\r\n\r\n" + body;
  const auto response = ExchangeHttpRequest(ProxyConfig{}, request);

  EXPECT_EQ(HttpStatusLine(response), "HTTP/1.1 200 OK");
  EXPECT_NE(HttpBody(response).find("\"already_authenticated\""),
            std::string::npos);

  ProxyAuthSessionStore::RemoveSession(session_id);
}
