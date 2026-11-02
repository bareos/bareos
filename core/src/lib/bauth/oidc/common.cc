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

#include "common.h"

#include <jansson.h>

namespace auth::Oidc {
namespace {
size_t curl_write_cb(char* curldata, size_t size, size_t nmemb, void* userdata)
{
  auto* str = static_cast<std::string*>(userdata);

  size_t bytes = size * nmemb;

  str->append(curldata, bytes);
  return bytes;
}
};  // namespace
CURLcode curl_easy_write_into_str(CURL* curl, std::string* buffer)
{
  ASSERT(buffer);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

  return curl_easy_perform(curl);
}

site_metadata fetch_metadata(CURL* curl, const std::string& url)
{
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  std::string response;
  auto res = curl_easy_write_into_str(curl, &response);

  if (res != CURLE_OK) { throw std::runtime_error{"could not fetch metadata"}; }

  curl_easy_reset(curl);

  json_error_t err = {};
  json_ptr json{json_loadb(response.data(), response.size(), 0, &err)};
  if (!json || !json_is_object(json.get())) {
    throw std::runtime_error{"bad json"};
  }

  const char* token_endpoint{};
  const char* jwks_uri{};
  const char* issuer{};
  const char* device_authorization_endpoint{};

  if (json_unpack_ex(json.get(), &err, 0, "{s:s, s:s, s:s, s:s}",
                     "token_endpoint", &token_endpoint, "jwks_uri", &jwks_uri,
                     "issuer", &issuer, "device_authorization_endpoint",
                     &device_authorization_endpoint)
      < 0) {
    throw std::runtime_error{std::string{"missing values in json object: "}
                             + response};
  }

  return {token_endpoint, jwks_uri, issuer, device_authorization_endpoint};
}
};  // namespace auth::Oidc
