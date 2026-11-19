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

  void set_used() { used = true; }
  bool was_used() const { return used; }

 private:
  bool used{false};
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

struct NamedVerifier {
  std::string name;
  std::unique_ptr<Verifier> impl;
};

struct challenge {
  std::string internal_name;  // this is used when talking to the server
  std::string display_name;   // this can be shown to the user
  std::string type;           // this is used to select the handler
  std::string challenge;
};

struct AlgorithmChooser {
  virtual size_t choose(std::span<const std::pair<Prover*, challenge*>>) = 0;
};

struct DefaultAlgorithmChooser : AlgorithmChooser {
  size_t choose(std::span<const std::pair<Prover*, challenge*>>) override
  {
    return 0;
  }
};

extern DefaultAlgorithmChooser default_chooser;

struct Algorithms {
  std::vector<std::unique_ptr<Prover>> prover;
  std::vector<NamedVerifier> verifier;
  AlgorithmChooser* chooser = &default_chooser;

  template <typename T, typename... Args>
    requires(std::is_base_of_v<Prover, T> && !std::is_base_of_v<Verifier, T>)
  T& add(Args&&... args)
  {
    auto p = std::make_unique<T>(std::forward<Args>(args)...);
    auto ptr = p.get();
    prover.emplace_back(std::move(p));
    return *ptr;
  }

  template <typename T, typename... Args>
    requires(!std::is_base_of_v<Prover, T> && std::is_base_of_v<Verifier, T>)
  T& add(std::string name, Args&&... args)
  {
    auto v = std::make_unique<T>(std::forward<Args>(args)...);
    auto ptr = v.get();
    verifier.emplace_back(std::move(name), std::move(v));
    return *ptr;
  }
};

namespace Version1 {
/* a cram-md5 challenge consists of three parts:
 *  - a current timestamp,
 *  - a random value, and
 *  - some way to identify our own challenges
 * cram_identity is used for the third part.  It makes sure
 * that you cannot use us, to solve our own challenge.
 * This value can be anything, but it should always be the same for the
 * livetime of the program, otherwise it will not do its job!
 * This string shall _NOT_ contain whitespace! */
bool authenticate_outbound(std::string_view identity, OutboundArgs args);
bool authenticate_inbound(std::string_view identity, InboundArgs args);
};  // namespace Version1

namespace Version2 {
// introduced in 26.0.0
bool authenticate_outbound(Algorithms& algs, OutboundArgs args);
bool authenticate_inbound(Algorithms& algs, InboundArgs args);
};  // namespace Version2

/* these will automatically use the correct one (Version1, Version2, ...)
 * depending on the remote.  The identity for Version1 is taken from
 * a cram-md5 prover/verifier pair.  If no such pair exists, then no fallback
 * is used. */
bool authenticate_inbound(Algorithms& algs, InboundArgs args);
bool authenticate_outbound(Algorithms& algs, OutboundArgs args);

};  // namespace auth

#endif  // BAREOS_LIB_BAUTH_H_
