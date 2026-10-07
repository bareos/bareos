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

struct OutboundAuthenticator {
  virtual bool authenticate(OutboundArgs args) = 0;
};

struct Algorithms {
  std::vector<std::unique_ptr<Prover>> prover;
  std::vector<std::unique_ptr<Verifier>> verifier;

  template <typename T, typename... Args>
    requires std::is_base_of<T, Prover>
  Algorithms& add_prover(Args... args)
  {
    prover.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    return *this;
  }
};

struct Authenticator {
  struct InboundArgs {
    BareosSocket* socket;
    uint32_t remote_version;

    const TlsResource* target;
    std::span<std::unique_ptr<Prover>> provers;
    std::span<std::unique_ptr<Verifier>> verifiers;
  };

  virtual bool authenticate_outbound(OutboundArgs args) = 0;
  virtual bool authenticate_inbound(InboundArgs args) = 0;
  virtual ~Authenticator() = default;
};

struct Md5Authenticator : Authenticator {
  bool authenticate_outbound(OutboundArgs args) override;
  bool authenticate_inbound(InboundArgs args) override;

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

struct NewAuthenticator : Authenticator {
  bool authenticate_outbound(OutboundArgs args) override;
  bool authenticate_inbound(InboundArgs args) override;
};

struct DefaultAuthenticator : Authenticator {
  bool authenticate_outbound(OutboundArgs args) override;
  bool authenticate_inbound(InboundArgs args) override;
};
};  // namespace auth

#endif  // BAREOS_LIB_BAUTH_H_
