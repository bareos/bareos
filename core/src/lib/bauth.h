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
#ifndef BAREOS_LIB_BAUTH_H_
#define BAREOS_LIB_BAUTH_H_

#include "lib/hello.h"
#include "lib/bsock.h"

#include <span>
#include <type_traits>
#include <vector>
#include <string_view>

namespace auth {
enum Status
{
  InProgress,
  Done,
  Error,
};

struct Verifier {
  virtual std::string_view name() const = 0;
  virtual std::vector<char> generate_challenge() = 0;
  virtual Status step(std::vector<char>& request, std::span<const char> data)
      = 0;
  virtual const char* err() const = 0;

  virtual ~Verifier() = default;
};

struct Prover {
  virtual std::string_view name() const = 0;
  virtual bool step(std::vector<char>& response, std::span<const char> data)
      = 0;
  virtual const char* err() const = 0;
  virtual bool done() const = 0;

  virtual ~Prover() = default;
};
};  // namespace auth

namespace auth {
struct OutboundArgs {
  JobControlRecord* jcr;
  BareosSocket* socket;
  const TlsResource* target;
};

enum class ConnectionType
{
  /* This connection is insecure, we do not know who we are talking to */
  Insecure,
  /* The connection is secure, but we do not trust the other side yet. */
  Untrusted,
  /* The connection is secure and we trust the other side. */
  Trusted,
};

struct InboundArgs {
  BareosSocket* socket;
  uint32_t remote_version;
  const TlsResource* target;
  ConnectionType type;
};

struct OutboundAuthenticator {
  virtual bool authenticate(OutboundArgs args) = 0;
};

struct InboundAuthenticator {
  virtual bool authenticate(InboundArgs args) = 0;
  virtual const TlsResource* tls_settings() = 0;
  virtual ~InboundAuthenticator() = default;
};

struct Algorithms {
  std::vector<std::unique_ptr<Prover>> prover;
  std::vector<std::unique_ptr<Verifier>> verifier;

  template <typename T, typename... Args>
    requires(std::is_base_of_v<Prover, T> != std::is_base_of_v<Verifier, T>)
  T& add(Args&&... args)
  {
    if constexpr (std::is_base_of_v<Prover, T>) {
      auto p = std::make_unique<T>(std::forward<Args>(args)...);
      auto ptr = p.get();
      prover.emplace_back(std::move(p));
      return *ptr;
    } else {
      auto v = std::make_unique<T>(std::forward<Args>(args)...);
      auto ptr = v.get();
      verifier.emplace_back(std::move(v));
      return *ptr;
    }
  }
};

struct Md5Authenticator {
  bool authenticate_outbound(OutboundArgs args);
  bool authenticate_inbound(InboundArgs args);

  Md5Authenticator();
  Md5Authenticator(std::string identity);

  /* a cram-md5 challenge consists of three parts:
   *  - a current timestamp,
   *  - a random value, and
   *  - some way to identify our own challenges
   * cram_identity is used for the third part.  It makes sure
   * that you cannot use us, to solve our own challenge.
   * This value can be anything, but it should always be the same for the
   * livetime of the program, otherwise it will not do its job!
   * This string shall _NOT_ contain whitespace! */
  std::string cram_identity;
};

struct NewAuthenticator {
  bool authenticate_outbound(std::span<std::unique_ptr<Prover>> provers,
                             std::span<std::unique_ptr<Verifier>> verifiers,
                             OutboundArgs args);
  bool authenticate_inbound(std::span<std::unique_ptr<Prover>> provers,
                            std::span<std::unique_ptr<Verifier>> verifiers,
                            InboundArgs args);
};
};  // namespace auth

#endif  // BAREOS_LIB_BAUTH_H_
