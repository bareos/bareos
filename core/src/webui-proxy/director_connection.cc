/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
#include "director_connection.h"
#include "bareos_base64.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <random>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>

// ---------------------------------------------------------------------------
// Bareos signal codes (negative length-prefix values)
// ---------------------------------------------------------------------------
static constexpr int32_t kBnetEod = -1;          // End of data
static constexpr int32_t kBnetEods = -2;         // End of data stream
static constexpr int32_t kBnetEof = -3;          // End of file
static constexpr int32_t kBnetError = -4;        // Error
static constexpr int32_t kBnetCmdOk = -15;       // Command succeeded
static constexpr int32_t kBnetCmdBegin = -16;    // Start command execution
static constexpr int32_t kBnetMainPrompt = -18;  // Server ready and waiting
static constexpr int32_t kBnetSubPrompt = -27;   // At a sub-prompt

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void WriteAll(int fd, const void* buf, size_t len)
{
  const auto* p = static_cast<const uint8_t*>(buf);
  while (len > 0) {
    ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
    if (n <= 0) { throw std::runtime_error("Director: send failed"); }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

static void ReadAll(int fd, void* buf, size_t len)
{
  auto* p = static_cast<uint8_t*>(buf);
  while (len > 0) {
    ssize_t n = ::recv(fd, p, len, MSG_WAITALL);
    if (n <= 0) {
      throw std::runtime_error("Director: connection closed by peer");
    }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

// Compute MD5(text) and return it as a lowercase hex string (32 chars).
static std::string Md5Hex(const std::string& text)
{
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned int dlen = 0;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
  EVP_DigestUpdate(ctx, text.data(), text.size());
  EVP_DigestFinal_ex(ctx, digest, &dlen);
  EVP_MD_CTX_free(ctx);

  char hex[65] = {};
  for (unsigned int i = 0; i < dlen; ++i) {
    snprintf(hex + i * 2, 3, "%02x", digest[i]);
  }
  return {hex, dlen * 2};
}

/**
 * Compute HMAC-MD5(key, data) and return the 16-byte raw digest.
 * The key is the MD5 hex string of the plaintext password.
 */
static std::array<uint8_t, 16> HmacMd5(const std::string& key,
                                       const std::string& data)
{
  std::array<uint8_t, 16> result{};
  unsigned int len = 16;

  HMAC(EVP_md5(), key.data(), static_cast<int>(key.size()),
       reinterpret_cast<const uint8_t*>(data.data()), data.size(),
       result.data(), &len);
  return result;
}

// ---------------------------------------------------------------------------
// Frame I/O
// ---------------------------------------------------------------------------

void DirectorConnection::SendFrame(const std::string& data)
{
  int32_t hdr = htonl(static_cast<int32_t>(data.size()));
  WriteAll(fd_, &hdr, 4);
  if (!data.empty()) { WriteAll(fd_, data.data(), data.size()); }
}

std::string DirectorConnection::RecvFrame()
{
  int32_t hdr_net;
  ReadAll(fd_, &hdr_net, 4);
  int32_t len = ntohl(hdr_net);

  if (len <= 0) {
    return {};  // signal frame (BNET_EOD, BNET_ERROR, etc.) — no payload
  }

  std::string msg(static_cast<size_t>(len), '\0');
  ReadAll(fd_, msg.data(), static_cast<size_t>(len));
  return msg;
}

std::string DirectorConnection::RecvResponse()
{
  std::string result;
  while (true) {
    int32_t hdr_net;
    ReadAll(fd_, &hdr_net, 4);
    int32_t len = ntohl(hdr_net);

    if (len == kBnetEod || len == kBnetEods || len == kBnetEof) {
      break;  // end of response
    }
    if (len == kBnetError) {
      throw std::runtime_error("Director: received BNET_ERROR signal");
    }
    if (len < 0) {
      continue;  // other signals (BNET_CMD_BEGIN, BNET_CMD_OK, …) — skip
    }
    if (len == 0) {
      continue;  // keep-alive — skip
    }

    std::string chunk(static_cast<size_t>(len), '\0');
    ReadAll(fd_, chunk.data(), static_cast<size_t>(len));
    result += chunk;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Authentication
// ---------------------------------------------------------------------------

void DirectorConnection::Authenticate(const DirectorConfig& cfg)
{
  // The CRAM-MD5 key is MD5(plaintext_password) as a hex string.
  const std::string key = Md5Hex(cfg.password);

  // Step 1: send Hello with version so the director uses the >= 18.2 protocol.
  const std::string hello
      = "Hello " + cfg.username + " calling version " BAREOS_FULL_VERSION "\n";
  SendFrame(hello);

  // Step 2: receive director's challenge
  std::string challenge_msg = RecvFrame();
  if (challenge_msg.empty()) {
    throw std::runtime_error(
        "Director: no challenge received after Hello (got signal frame)");
  }
  // Challenge format: "auth cram-md5 <token> ssl=<n>\n"
  char token_buf[512] = {};
  int ssl_val = 0;
  if (sscanf(challenge_msg.c_str(), "auth cram-md5 %511s ssl=%d", token_buf,
             &ssl_val)
      < 1) {
    throw std::runtime_error("Director: unexpected response to Hello: "
                             + challenge_msg);
  }
  std::string director_challenge(token_buf);

  // Step 3: compute and send our CRAM-MD5 response (no trailing newline)
  auto hmac = HmacMd5(key, director_challenge);
  std::string response = BareosBase64Encode(hmac.data(), 16, true);
  SendFrame(response);

  // Step 4: receive director's "1000 OK auth\n"
  std::string auth_result = RecvFrame();
  if (auth_result.find("1000 OK auth") == std::string::npos) {
    throw std::runtime_error("Director: authentication failed: " + auth_result);
  }

  // Step 5: send our own challenge to verify the director
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<uint32_t> dist(1000000000u, 4000000000u);
  uint32_t rand_val = dist(rng);
  uint32_t ts = static_cast<uint32_t>(std::time(nullptr));
  std::string our_challenge = "<" + std::to_string(rand_val) + "."
                              + std::to_string(ts) + "@" + cfg.username + ">";
  std::string our_challenge_msg = "auth cram-md5 " + our_challenge + " ssl=0\n";
  SendFrame(our_challenge_msg);

  // Step 6: receive director's CRAM-MD5 response to our challenge.
  // The director includes a null terminator in the frame length, so strip it.
  std::string dir_response = RecvFrame();
  while (!dir_response.empty()
         && (dir_response.back() == '\n' || dir_response.back() == '\0')) {
    dir_response.pop_back();
  }

  // Step 7: verify director response (accept both compatible and
  // non-compatible)
  auto expected_hmac = HmacMd5(key, our_challenge);
  std::string expected_compat
      = BareosBase64Encode(expected_hmac.data(), 16, true);
  std::string expected_noncompat
      = BareosBase64Encode(expected_hmac.data(), 16, false);

  bool ok = (dir_response == expected_compat)
            || (dir_response == expected_noncompat);

  if (ok) {
    SendFrame("1000 OK auth\n");
  } else {
    SendFrame("1999 Authorization failed.\n");
    throw std::runtime_error(
        "Director: failed to verify director identity (wrong CRAM-MD5 "
        "response)");
  }

  // Step 8: receive welcome (>= 18.2: "1000\x1eOK: <name> Version: ...").
  // The director also sends a second info frame (1002\x1e...) that we discard.
  std::string welcome = RecvFrame();
  if (welcome.find("1000") == std::string::npos) {
    throw std::runtime_error("Director: unexpected welcome: " + welcome);
  }
  // Consume the info message frame (1002).
  RecvFrame();

  // Step 9: activate JSON API if requested.
  // Signal frames (e.g. BNET_MSGS_PENDING) may appear between commands;
  // loop until we receive the actual data frame for each .api response.
  if (cfg.json_mode) {
    auto RecvDataFrame = [this]() {
      while (true) {
        std::string f = RecvFrame();
        if (!f.empty()) { return f; }
      }
    };
    SendFrame(".api json\n");
    RecvDataFrame();

    SendFrame(".api json compact=yes\n");
    RecvDataFrame();
  }
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void DirectorConnection::Connect(const DirectorConfig& cfg)
{
  json_mode_ = cfg.json_mode;

  // Resolve host and connect
  struct addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo* res = nullptr;
  const std::string port_str = std::to_string(cfg.port);
  int rc = getaddrinfo(cfg.host.c_str(), port_str.c_str(), &hints, &res);
  if (rc != 0) {
    throw std::runtime_error(std::string("Director: getaddrinfo: ")
                             + gai_strerror(rc));
  }

  for (auto* ai = res; ai != nullptr; ai = ai->ai_next) {
    fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd_ < 0) { continue; }
    if (::connect(fd_, ai->ai_addr, ai->ai_addrlen) == 0) { break; }
    ::close(fd_);
    fd_ = -1;
  }
  freeaddrinfo(res);

  if (fd_ < 0) {
    throw std::runtime_error("Director: could not connect to " + cfg.host + ":"
                             + std::to_string(cfg.port));
  }

  try {
    Authenticate(cfg);
  } catch (...) {
    ::close(fd_);
    fd_ = -1;
    throw;
  }
}

std::string DirectorConnection::Call(const std::string& command)
{
  SendFrame(command + "\n");
  if (json_mode_) {
    // JSON API mode: the director sends data in one or more frames then a
    // terminal signal (BNET_MAIN_PROMPT, -18), with no trailing BNET_EOD.
    // Large responses (> ~1 MB, e.g. thousands of jobs) arrive in multiple
    // consecutive data frames.  Additionally, a stale BNET_MAIN_PROMPT from
    // the previous command may sit at the front of the read buffer.
    //
    // Strategy: skip ALL leading signals until the first data frame arrives,
    // then accumulate data frames and stop as soon as any signal appears.
    // This handles both single-frame and multi-frame responses and correctly
    // drains leftover signals from preceding commands.
    std::string result;
    while (true) {
      int32_t hdr_net;
      ReadAll(fd_, &hdr_net, 4);
      int32_t len = ntohl(hdr_net);

      if (len < 0) {
        if (!result.empty()) { break; }  // signal after data → done
        continue;                        // leading signal → skip
      }
      if (len == 0) { continue; }  // keep-alive

      std::string chunk(static_cast<size_t>(len), '\0');
      ReadAll(fd_, chunk.data(), static_cast<size_t>(len));
      result += chunk;
    }
    return result;
  }
  return RecvResponse();
}

void DirectorConnection::Disconnect()
{
  if (fd_ >= 0) {
    // Best-effort: send quit command
    try {
      SendFrame("quit\n");
    } catch (...) {
    }
    ::close(fd_);
    fd_ = -1;
  }
}

DirectorConnection::~DirectorConnection() { Disconnect(); }
