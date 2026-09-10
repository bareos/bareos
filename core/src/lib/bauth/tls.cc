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

#include "tls.h"

#ifndef AUTH_NAME
#  error AUTH_NAME missing
#endif

namespace auth::Tls {

std::string_view Verifier::name() const { return AUTH_NAME; };

auth::Status Verifier::step(std::vector<char>&, std::span<const char> received)
{
  if (received.size() != 0) {
    error = "expected empty payload";
    current_state = ERROR;
    return auth::Status::Error;
  }

  current_state = ACCEPTED_RESULT;
  return auth::Status::Done;
}

std::string_view Prover::name() const { return AUTH_NAME; };

bool Prover::step(std::vector<char>&, std::span<const char> request)
{
  if (request.size() != 0) {
    error = "expected empty payload";
    current_state = ERROR;
    return false;
  }

  current_state = RESPONSE_SENT;
  return true;
}
};  // namespace auth::Tls
