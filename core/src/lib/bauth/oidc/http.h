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

#ifndef BAREOS_LIB_BAUTH_OIDC_HTTP_H_
#define BAREOS_LIB_BAUTH_OIDC_HTTP_H_

#include <cstdint>
#include <string>
#include <span>

namespace auth::Oidc::http {

struct post_field {
  std::string key;
  std::string value;
};


struct client {
  virtual bool get(const std::string& url) = 0;
  virtual bool post(const std::string& url,
                    std::span<const post_field> fields = {})
      = 0;

  virtual std::string error() = 0;
  virtual std::string result() = 0;
  virtual std::uint32_t return_code() = 0;

  virtual ~client() = default;
};

};  // namespace auth::Oidc::http

#endif  // BAREOS_LIB_BAUTH_OIDC_HTTP_H_
