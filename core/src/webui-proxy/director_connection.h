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
/**
 * @file
 * Bareos Director TCP client.
 *
 * Implements the Bareos wire protocol (4-byte big-endian length-prefix
 * framing) and CRAM-MD5 mutual authentication, then optionally activates
 * the JSON API mode.
 *
 * Protocol summary (client side):
 *  1. Connect TCP to director:9101
 *  2. Send:  Hello <name> calling\n               (as a Bareos frame)
 *  3. Recv:  auth cram-md5 <chal> ssl=<n>\n      (director challenge)
 *  4. Compute response = BareosBase64(HMAC-MD5(MD5(password), challenge))
 *  5. Send:  <response>\n
 *  6. Recv:  1000 OK auth\n
 *  7. Send:  auth cram-md5 <our_chal> ssl=0\n    (our challenge)
 *  8. Recv:  <director response>
 *  9. Verify director response; send 1000 OK auth\n (or 1999 failed)
 * 10. Recv:  1000 OK: <name> Version: ...         (welcome)
 * 11. (json mode) Send .api json compact=yes\n   (activate JSON API)
 */
#ifndef BAREOS_WEBUI_PROXY_DIRECTOR_CONNECTION_H_
#define BAREOS_WEBUI_PROXY_DIRECTOR_CONNECTION_H_

#include <string>

struct DirectorConfig {
  std::string host;
  int port{9101};
  std::string director_name;
  std::string username;
  std::string password;  // plaintext
  bool json_mode{true};
};

class DirectorConnection {
 public:
  /**
   * Connect to the director and perform full authentication.
   * After return the connection is ready for Call() / CallRaw().
   * Throws std::runtime_error on any failure.
   */
  void Connect(const DirectorConfig& cfg);

  /**
   * Send a command and receive the complete response string.
   * In json_mode=true, the response is a JSON object.
   * In json_mode=false, the response is a raw text string.
   * Throws std::runtime_error on I/O error.
   */
  std::string Call(const std::string& command);

  /** Cleanly disconnect from the director. */
  void Disconnect();

  ~DirectorConnection();

 private:
  int fd_{-1};
  bool json_mode_{true};

  // Bareos frame I/O
  void SendFrame(const std::string& data);
  // Receive one frame; returns empty on signal frames (len <= 0).
  std::string RecvFrame();
  // Receive a complete response (accumulate frames until BNET_EOD).
  std::string RecvResponse();

  void Authenticate(const DirectorConfig& cfg);
};

#endif  // BAREOS_WEBUI_PROXY_DIRECTOR_CONNECTION_H_
