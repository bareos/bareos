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
/**
 * @file
 * Bareos Director TCP client.
 *
 * Connects to the Bareos Director over TCP, upgrades that transport to
 * TLS-PSK, and then runs the Bareos wire protocol (4-byte big-endian
 * length-prefix framing) with CRAM-MD5 mutual authentication.
 *
 * Protocol summary (client side):
 *  1. Connect TCP to director:9101
 *  2. Establish TLS-PSK on that socket
 *  3. Send:  Hello <name> calling\n               (as a Bareos frame)
 *  4. Recv:  auth cram-md5 <chal> ssl=<n>\n      (director challenge)
 *  5. Compute response = BareosBase64(HMAC-MD5(MD5(password), challenge))
 *  6. Send:  <response>\n
 *  7. Recv:  1000 OK auth\n
 *  8. Send:  auth cram-md5 <our_chal> ssl=0\n    (our challenge)
 *  9. Recv:  <director response>
 * 10. Verify director response; send 1000 OK auth\n (or 1999 failed)
 * 11. Recv:  1000 OK: <name> Version: ...         (welcome)
 * 12. (json mode) Send .api json compact=yes\n   (activate JSON API)
 */
#ifndef BAREOS_WEBUI_PROXY_DIRECTOR_CONNECTION_H_
#define BAREOS_WEBUI_PROXY_DIRECTOR_CONNECTION_H_

#include <cstddef>
#include <string>

struct ssl_ctx_st;
struct ssl_st;

/** Prompt state signalled by the director at the end of a raw-mode response. */
enum class DirectorPrompt
{
  Main,    ///< BNET_MAIN_PROMPT: ready for next command  ("* ")
  Sub,     ///< BNET_SUB_PROMPT: waiting for sub-command  ("> ")
  Select,  ///< BNET_SELECT_INPUT: waiting for a numeric selection
  Other,   ///< any other signal (BNET_EOD etc.)
};

/** Return value of DirectorConnection::Call(). */
struct CallResult {
  std::string text;
  DirectorPrompt prompt{DirectorPrompt::Main};
};

struct DirectorConfig {
  std::string host;
  int port{9101};
  std::string director_name;
  std::string username;
  std::string password;  // plaintext
  bool json_mode{true};
  bool tls_psk_require{true};
};

std::string GetDirectorTlsPskIdentity(const std::string& console_name);
std::string GetDirectorTlsPskSecret(const std::string& password);

class DirectorConnection {
 public:
  /* Connect to the director and perform full authentication.
   * After return the connection is ready for Call() / CallRaw().
   * Throws std::runtime_error on any failure. */
  void Connect(const DirectorConfig& cfg);

  /* Send a command and receive the complete response string.
   * In json_mode=true, the response is a JSON object.
   * In json_mode=false, the response is a raw text string.
   * The returned CallResult also carries the prompt type that terminated the
   * response so callers can update the UI prompt indicator.
   * Throws std::runtime_error on I/O error. */
  CallResult Call(const std::string& command);

  /** Cleanly disconnect from the director. */
  void Disconnect();

  ~DirectorConnection();

 private:
  friend unsigned int TlsPskClientCallback(ssl_st* ssl,
                                           const char* hint,
                                           char* identity,
                                           unsigned int max_identity_len,
                                           unsigned char* psk,
                                           unsigned int max_psk_len);

  int fd_{-1};
  bool json_mode_{true};
  ssl_ctx_st* ssl_ctx_{nullptr};
  ssl_st* ssl_{nullptr};
  std::string tls_psk_identity_;
  std::string tls_psk_secret_;

  void WriteAll(const void* buf, size_t len);
  void ReadAll(void* buf, size_t len);
  void ConnectTcp(const DirectorConfig& cfg);
  void ConnectTlsPsk(const DirectorConfig& cfg);

  // Bareos frame I/O
  void SendFrame(const std::string& data);
  // Receive one frame; returns empty on signal frames (len <= 0).
  // When signal is non-null, it receives the raw negative frame code.
  std::string RecvFrame(int32_t* signal = nullptr);
  // Receive a complete response (accumulate frames until BNET_EOD).
  std::string RecvResponse();

  void Authenticate(const DirectorConfig& cfg);
};

#endif  // BAREOS_WEBUI_PROXY_DIRECTOR_CONNECTION_H_
