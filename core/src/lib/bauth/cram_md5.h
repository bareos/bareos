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

#ifndef BAREOS_LIB_BAUTH_CRAM_MD5_H_
#define BAREOS_LIB_BAUTH_CRAM_MD5_H_

#include "lib/bauth.h"

namespace auth::CramMd5 {
struct Verifier : public auth::Verifier {
  using Base = auth::Verifier;

  Verifier(std::string name, std::string pass)
      : my_name{std::move(name)}, password{std::move(pass)}
  {
  }

  std::string_view name() const override;
  std::vector<char> generate_challenge() override;
  auth::Status step(std::vector<char>&,
                    std::span<const char> received) override;
  const char* err() const override { return error.c_str(); }

  virtual ~Verifier() = default;


  const std::string& cram_name() const { return my_name; }
  const std::string& cram_pw() const { return password; }

 private:
  enum
  {
    INITIAL = 0,
    GENERATED_CHALLENGE,
    ACCEPTED_RESULT,
    ERROR,
  } current_state{INITIAL};


  std::vector<char> expected{};
  std::string my_name;
  std::string password;
  std::string error;
};

struct Prover : public auth::Prover {
  Prover(std::string name, std::string pass)
      : my_name{std::move(name)}, password{std::move(pass)}
  {
  }

  using Base = auth::Prover;

  std::string_view name() const override;
  bool step(std::vector<char>& response,
            std::span<const char> request) override;
  const char* err() const override { return error.c_str(); }
  bool done() const override { return current_state == RESPONSE_SENT; }

  virtual ~Prover() = default;

  const std::string& cram_name() const { return my_name; }
  const std::string& cram_pw() const { return password; }

 private:
  std::string error;
  enum
  {
    WAITING_FOR_CHALLENGE = 0,
    RESPONSE_SENT,
    ERROR,
  } current_state{WAITING_FOR_CHALLENGE};

  std::string my_name;
  std::string password;
};
};  // namespace auth::CramMd5

#endif  // BAREOS_LIB_BAUTH_CRAM_MD5_H_
