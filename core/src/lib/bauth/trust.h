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

#ifndef BAREOS_LIB_BAUTH_TRUST_H_
#define BAREOS_LIB_BAUTH_TRUST_H_

#include "lib/bauth.h"
#include "lib/util.h"

namespace auth::Trust {
struct Verifier : public auth::Verifier {
  using Base = auth::Verifier;

  Verifier() {}

  std::string_view name() const override { return "trust"; }
  std::vector<char> generate_challenge() override { return {}; }
  auth::Status step(std::vector<char>&, std::span<const char> received) override
  {
    if (received.empty()) { return auth::Status::Done; }

    Dmsg0(100, "trust verifier received %zu bytes\n", received.size());
    error = "received unexpected data";
    return auth::Status::Error;
  }
  const char* err() const override { return error.c_str(); }

  virtual ~Verifier() = default;

 private:
  std::string error;
};

struct Prover : public auth::Prover {
  Prover() {}

  using Base = auth::Prover;

  std::string_view name() const override { return "trust"; }
  bool step(std::vector<char>&, std::span<const char> request) override
  {
    if (request.empty()) { return true; }

    Dmsg0(100, "trust prover received %zu bytes\n", request.size());
    error = "trust prover received data";
    return false;
  }
  const char* err() const override { return error.c_str(); }
  bool done() const override { return true; }

  virtual ~Prover() = default;

 private:
  std::string error;
};
};  // namespace auth::Trust

#endif  // BAREOS_LIB_BAUTH_TRUST_H_
