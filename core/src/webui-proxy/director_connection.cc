/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
#include "ascii_control_characters.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <random>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/ssl.h>

// ---------------------------------------------------------------------------
// Bareos signal codes (negative length-prefix values)
// ---------------------------------------------------------------------------
static constexpr int32_t kBnetEod = -1;    // End of data
static constexpr int32_t kBnetEods = -2;   // End of data stream
static constexpr int32_t kBnetEof = -3;    // End of file
static constexpr int32_t kBnetError = -4;  // Error
[[maybe_unused]] static constexpr int32_t kBnetCmdOk
    = -15;  // Command succeeded
[[maybe_unused]] static constexpr int32_t kBnetCmdBegin
    = -16;                                       // Start command execution
static constexpr int32_t kBnetMainPrompt = -18;  // Server ready and waiting
static constexpr int32_t kBnetSelectInput
    = -19;                                      // Waiting for numeric selection
static constexpr int32_t kBnetSubPrompt = -27;  // At a sub-prompt

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Compute MD5(text) and return it as a lowercase hex string (32 chars).
std::string GetDirectorTlsPskSecret(const std::string& password)
{
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned int dlen = 0;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
  EVP_DigestUpdate(ctx, password.data(), password.size());
  EVP_DigestFinal_ex(ctx, digest, &dlen);
  EVP_MD_CTX_free(ctx);

  char hex[65] = {};
  for (unsigned int i = 0; i < dlen; ++i) {
    snprintf(hex + i * 2, 3, "%02x", digest[i]);
  }
  return {hex, dlen * 2};
}

std::string GetDirectorTlsPskIdentity(const std::string& console_name)
{
  std::string identity = "R_CONSOLE";
  identity.reserve(identity.size() + 1 + console_name.size());
  identity.push_back(AsciiControlCharacters::RecordSeparator());
  identity.append(console_name);
  return identity;
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

static std::string GetOpenSslError()
{
  std::string error;
  while (unsigned long code = ERR_get_error()) {
    char buffer[256] = {};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    if (!error.empty()) { error += ": "; }
    error += buffer;
  }
  return error.empty() ? "unknown OpenSSL error" : error;
}

static int GetDirectorConnectionSslCtxExDataIndex()
{
  static const int index
      = SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  return index;
}

unsigned int TlsPskClientCallback(SSL* ssl,
                                  const char* /*hint*/,
                                  char* identity,
                                  unsigned int max_identity_len,
                                  unsigned char* psk,
                                  unsigned int max_psk_len)
{
  SSL_CTX* ssl_ctx = SSL_get_SSL_CTX(ssl);
  if (!ssl_ctx) { return 0; }

  auto* connection = static_cast<DirectorConnection*>(
      SSL_CTX_get_ex_data(ssl_ctx, GetDirectorConnectionSslCtxExDataIndex()));
  if (!connection) { return 0; }

  const std::string& tls_psk_identity = connection->tls_psk_identity_;
  const std::string& tls_psk_secret = connection->tls_psk_secret_;

  if (tls_psk_identity.empty() || tls_psk_secret.empty()) { return 0; }
  if (tls_psk_identity.size() + 1 > max_identity_len
      || tls_psk_secret.size() > max_psk_len) {
    return 0;
  }

  std::memcpy(identity, tls_psk_identity.c_str(),
              tls_psk_identity.size() + 1);
  std::memcpy(psk, tls_psk_secret.data(), tls_psk_secret.size());
  return static_cast<unsigned int>(tls_psk_secret.size());
}

// ---------------------------------------------------------------------------
// Frame I/O
// ---------------------------------------------------------------------------

void DirectorConnection::WriteAll(const void* buf, size_t len)
{
  const auto* p = static_cast<const uint8_t*>(buf);
  while (len > 0) {
    int n = 0;
    if (ssl_) {
      n = SSL_write(ssl_, p, static_cast<int>(len));
      if (n <= 0) {
        int ssl_error = SSL_get_error(ssl_, n);
        if (ssl_error == SSL_ERROR_WANT_READ
            || ssl_error == SSL_ERROR_WANT_WRITE) {
          continue;
        }
        throw std::runtime_error("Director: TLS write failed: "
                                 + GetOpenSslError());
      }
    } else {
      n = static_cast<int>(::send(fd_, p, len, MSG_NOSIGNAL));
      if (n < 0 && errno == EINTR) { continue; }
    }
    if (!ssl_ && n <= 0) { throw std::runtime_error("Director: send failed"); }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

void DirectorConnection::ReadAll(void* buf, size_t len)
{
  auto* p = static_cast<uint8_t*>(buf);
  while (len > 0) {
    int n = 0;
    if (ssl_) {
      n = SSL_read(ssl_, p, static_cast<int>(len));
      if (n <= 0) {
        int ssl_error = SSL_get_error(ssl_, n);
        if (ssl_error == SSL_ERROR_WANT_READ
            || ssl_error == SSL_ERROR_WANT_WRITE) {
          continue;
        }
        throw std::runtime_error("Director: TLS read failed: "
                                 + GetOpenSslError());
      }
    } else {
      n = static_cast<int>(::recv(fd_, p, len, MSG_WAITALL));
      if (n < 0 && errno == EINTR) { continue; }
    }
    if (!ssl_ && n <= 0) {
      throw std::runtime_error("Director: connection closed by peer");
    }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

void DirectorConnection::SendFrame(const std::string& data)
{
  int32_t hdr = htonl(static_cast<int32_t>(data.size()));
  WriteAll(&hdr, 4);
  if (!data.empty()) { WriteAll(data.data(), data.size()); }
}

bool DirectorConnection::HasPendingInput() const
{
  if (ssl_ && SSL_pending(ssl_) > 0) { return true; }
  if (fd_ < 0) { return false; }

  struct pollfd pfd {
    fd_, POLLIN, 0
  };
  int rc = poll(&pfd, 1, 0);
  return rc > 0 && (pfd.revents & POLLIN);
}

void DirectorConnection::DrainPendingInput()
{
  while (HasPendingInput()) {
    int32_t signal = 0;
    std::string frame = RecvFrame(&signal);
    if (signal == kBnetError) {
      throw std::runtime_error("Director: received BNET_ERROR signal");
    }
    if (!frame.empty()) {
      // Discard queued JSON/text frames that belong to a previous command.
      continue;
    }
  }
}

std::string DirectorConnection::RecvFrame(int32_t* signal)
{
  int32_t hdr_net;
  ReadAll(&hdr_net, 4);
  int32_t len = ntohl(hdr_net);

  if (len <= 0) {
    if (signal) { *signal = len; }
    return {};  // signal frame (BNET_EOD, BNET_ERROR, etc.) — no payload
  }

  if (signal) { *signal = 0; }

  std::string msg(static_cast<size_t>(len), '\0');
  ReadAll(msg.data(), static_cast<size_t>(len));
  return msg;
}

std::string DirectorConnection::RecvResponse()
{
  std::string result;
  while (true) {
    int32_t hdr_net;
    ReadAll(&hdr_net, 4);
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
    ReadAll(chunk.data(), static_cast<size_t>(len));
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
  const std::string key = GetDirectorTlsPskSecret(cfg.password);

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
  // Use the normal Call() path here so we consume both the data response and
  // the terminating prompt. Otherwise the last .api response can remain queued
  // and shift every subsequent dashboard command by one response.
  if (cfg.json_mode) {
    Call(".api json");
    Call(".api json compact=yes");
  }
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void DirectorConnection::Connect(const DirectorConfig& cfg)
{
  json_mode_ = cfg.json_mode;
  tls_psk_active_ = false;
  tls_psk_identity_ = GetDirectorTlsPskIdentity(cfg.username);
  tls_psk_secret_ = GetDirectorTlsPskSecret(cfg.password);

  auto connect_and_authenticate = [this, &cfg](bool use_tls) {
    ConnectTcp(cfg);
    if (use_tls) { ConnectTlsPsk(cfg); }
    Authenticate(cfg);
  };

  if (cfg.tls_psk_disable) {
    try {
      connect_and_authenticate(false);
    } catch (...) {
      Disconnect();
      throw;
    }
    return;
  }

  if (cfg.tls_psk_require) {
    try {
      connect_and_authenticate(true);
    } catch (...) {
      Disconnect();
      throw;
    }
    return;
  }

  try {
    connect_and_authenticate(true);
    return;
  } catch (...) {
    if (tls_psk_active_) {
      Disconnect();
      throw;
    }
    Disconnect();
  }

  try {
    connect_and_authenticate(false);
  } catch (...) {
    Disconnect();
    throw;
  }
}

void DirectorConnection::ConnectTcp(const DirectorConfig& cfg)
{
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
}

void DirectorConnection::ConnectTlsPsk(const DirectorConfig& cfg)
{
  ssl_ctx_ = SSL_CTX_new(TLS_client_method());
  if (!ssl_ctx_) {
    throw std::runtime_error("Director: could not create TLS-PSK context: "
                             + GetOpenSslError());
  }

  if (SSL_CTX_set_cipher_list(ssl_ctx_, "PSK") != 1) {
    throw std::runtime_error("Director: could not set TLS-PSK cipher list: "
                             + GetOpenSslError());
  }
  if (SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION) != 1
      || SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_2_VERSION) != 1) {
    throw std::runtime_error("Director: could not restrict TLS-PSK protocol: "
                             + GetOpenSslError());
  }

  SSL_CTX_set_ex_data(ssl_ctx_, GetDirectorConnectionSslCtxExDataIndex(),
                      this);
  SSL_CTX_set_psk_client_callback(ssl_ctx_, TlsPskClientCallback);

  ssl_ = SSL_new(ssl_ctx_);
  if (!ssl_) {
    throw std::runtime_error("Director: could not create TLS-PSK session: "
                             + GetOpenSslError());
  }
  if (SSL_set_fd(ssl_, fd_) != 1) {
    throw std::runtime_error("Director: could not bind TLS-PSK session: "
                             + GetOpenSslError());
  }
  if (SSL_set_tlsext_host_name(ssl_, cfg.host.c_str()) != 1) {
    throw std::runtime_error("Director: could not set TLS-PSK server name: "
                             + GetOpenSslError());
  }
  while (true) {
    int rc = SSL_connect(ssl_);
    if (rc == 1) { break; }
    int ssl_error = SSL_get_error(ssl_, rc);
    if (ssl_error == SSL_ERROR_WANT_READ
        || ssl_error == SSL_ERROR_WANT_WRITE) {
      continue;
    }
    throw std::runtime_error("Director: TLS-PSK handshake failed: "
                             + GetOpenSslError());
  }
  tls_psk_active_ = true;
}

CallResult DirectorConnection::Call(const std::string& command)
{
  if (json_mode_) { DrainPendingInput(); }
  SendFrame(command + "\n");
  // Unified receive strategy for both JSON and raw (text) mode:
  //
  // Skip all *leading* signals before any data arrives.  This handles:
  //   - Stale BNET_MAIN_PROMPT left by JSON mode's RecvDataFrame() after auth
  //   - BNET_CMD_BEGIN / BNET_CMD_OK preamble signals in JSON mode
  //   - BNET_MSGS_PENDING before the response in text mode
  //
  // Once data starts arriving, accumulate all consecutive data frames and
  // stop as soon as any signal is received.  This handles:
  //   JSON mode: data chunk(s) terminated by BNET_MAIN_PROMPT (no BNET_EOD)
  //   Text mode: data terminated by BNET_EOD or BNET_MAIN_PROMPT
  //   Interactive: menu text terminated by BNET_SELECT_INPUT / BNET_SUB_PROMPT
  //
  // Large responses (> ~1 MB) arrive as multiple consecutive data frames with
  // no signals between them; the loop accumulates them all correctly.
  CallResult result;
  while (true) {
    int32_t hdr_net;
    ReadAll(&hdr_net, 4);
    int32_t len = ntohl(hdr_net);

    if (len < 0) {
      if (len == kBnetError) {
        throw std::runtime_error("Director: received BNET_ERROR signal");
      }
      if (result.text.empty()) {
        if (len == kBnetMainPrompt) {
          result.prompt = DirectorPrompt::Main;
          break;
        }
        if (len == kBnetSubPrompt) {
          result.prompt = DirectorPrompt::Sub;
          break;
        }
        if (len == kBnetSelectInput) {
          result.prompt = DirectorPrompt::Select;
          break;
        }
        if (len == kBnetEod || len == kBnetEods || len == kBnetEof) {
          result.prompt = DirectorPrompt::Other;
          break;
        }
      }
      if (!result.text.empty()) {
        // Signal after data: determine prompt type and stop.
        if (len == kBnetMainPrompt) {
          result.prompt = DirectorPrompt::Main;
        } else if (len == kBnetSubPrompt) {
          result.prompt = DirectorPrompt::Sub;
        } else if (len == kBnetSelectInput) {
          result.prompt = DirectorPrompt::Select;
        } else {
          result.prompt = DirectorPrompt::Other;
        }
        break;
      }
      continue;  // leading signal before data → skip
    }
    if (len == 0) { continue; }  // keep-alive

    std::string chunk(static_cast<size_t>(len), '\0');
    ReadAll(chunk.data(), static_cast<size_t>(len));
    result.text += chunk;
  }
  return result;
}

void DirectorConnection::Disconnect()
{
  if (fd_ >= 0 || ssl_ || ssl_ctx_) {
    // Best-effort: send quit command
    try {
      if (fd_ >= 0) { SendFrame("quit\n"); }
    } catch (...) {
    }
    if (ssl_) {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
      ssl_ = nullptr;
    }
    if (ssl_ctx_) {
      SSL_CTX_free(ssl_ctx_);
      ssl_ctx_ = nullptr;
    }
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }
  tls_psk_active_ = false;
}

DirectorConnection::~DirectorConnection() { Disconnect(); }
