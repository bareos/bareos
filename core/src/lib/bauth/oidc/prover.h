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

#ifndef BAREOS_LIB_BAUTH_OIDC_PROVER_H_
#define BAREOS_LIB_BAUTH_OIDC_PROVER_H_

#include "lib/bauth.h"

namespace auth::Oidc {
struct Prover : public auth::Prover {
  using Base = auth::Prover;

  std::string_view name() const override { return "oidc"; };
  bool step(std::vector<char>& response,
            std::span<const char> request) override;
  const char* err() const override { return error.c_str(); }
  bool done() const override { return current_state == RESPONSE_SENT; }

  virtual ~Prover() = default;

 private:
  std::string error;
  enum
  {
    INITIAL = 0,
    WAITING_FOR_CHALLENGE,
    RESPONSE_SENT,
    ERROR,
  } current_state{INITIAL};

  bool Error(std::string text)
  {
    if (error.empty()) {
      error = std::move(text);
    } else {
      ASSERT(current_state == ERROR);
    }
    current_state = ERROR;
    return false;
  }
};
};  // namespace auth::Oidc


#endif  // BAREOS_LIB_BAUTH_OIDC_PROVER_H_
