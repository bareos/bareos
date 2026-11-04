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

#ifndef BAREOS_LIB_BAUTH_OIDC_COMMON_H_
#define BAREOS_LIB_BAUTH_OIDC_COMMON_H_

#include "lib/bauth.h"
#include <curl/curl.h>
#include <jansson.h>

namespace auth::Oidc {
struct cleanup {
  void operator()(CURL* curl) const
  {
    if (curl) { curl_easy_cleanup(curl); }
  }

  void operator()(json_t* json) const
  {
    if (json) { json_decref(json); }
  }
};
}  // namespace auth::Oidc

using curl_ptr = std::unique_ptr<CURL, auth::Oidc::cleanup>;
using json_ptr = std::unique_ptr<json_t, auth::Oidc::cleanup>;

namespace auth::Oidc {
struct url_encoder {
  void add_kv(std::string_view key, std::string_view value)
  {
    if (!encoded.empty()) { encoded += '&'; }

    char* escaped = curl_easy_escape(nullptr, value.data(), value.size());

    encoded += key;
    encoded += '=';
    encoded += escaped;

    curl_free(escaped);
  }


  const char* c_str() const { return encoded.c_str(); }
  const std::string& str() const& { return encoded; }
  std::string_view view() const { return encoded; }

  std::string&& str() && { return std::move(encoded); }

 private:
  std::string encoded{};
};

CURLcode curl_easy_write_into_str(CURL* curl, std::string* buffer);

struct site_metadata {
  std::string token_endpoint{};
  std::string jwks_uri{};
  std::string issuer{};
  std::string device_authorization_endpoint{};
};

site_metadata fetch_metadata(CURL* curl, const std::string& url);
};  // namespace auth::Oidc


#endif  // BAREOS_LIB_BAUTH_OIDC_COMMON_H_
