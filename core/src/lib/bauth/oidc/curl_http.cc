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

#include "include/baconfig.h"
#include "curl_http.h"
#include <curl/easy.h>

namespace auth::Oidc::http {
namespace {
size_t curl_write_cb(char* curldata, size_t size, size_t nmemb, void* userdata)
{
  auto* str = static_cast<std::string*>(userdata);

  size_t bytes = size * nmemb;

  str->append(curldata, bytes);
  return bytes;
}
CURLcode curl_easy_write_into_str(CURL* curl, std::string* buffer)
{
  ASSERT(buffer);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

  return curl_easy_perform(curl);
}
};  // namespace

std::string url_encode(std::span<const post_field> fields)
{
  std::string encoded;
  for (const auto& field : fields) {
    if (!encoded.empty()) { encoded += '&'; }

    char* escaped
        = curl_easy_escape(nullptr, field.value.data(), field.value.size());

    encoded += field.key;
    encoded += '=';
    encoded += escaped;

    curl_free(escaped);
  }

  return encoded;
}

bool curl_client::post(const std::string& url,
                       std::span<const post_field> fields)
{
  reset();
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  auto encoded = url_encode(fields);

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encoded.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);

  return perform();
}

bool curl_client::get(const std::string& url)
{
  reset();
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  return perform();
}

bool curl_client::perform()
{
  // Allow HTTPS only
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");

  // Allow HTTPS only for redirects
  curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");

  // Follow redirects
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Enforce TLS certificate verification
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  ret = curl_easy_write_into_str(curl, &content);
  return ret == CURLE_OK;
}
};  // namespace auth::Oidc::http
