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

#ifndef BAREOS_LIB_BAUTH_OIDC_CURL_HTTP_H_
#define BAREOS_LIB_BAUTH_OIDC_CURL_HTTP_H_

#include "http.h"
#include "include/baconfig.h"

#include <curl/curl.h>
#include <curl/easy.h>

namespace auth::Oidc::http {

struct curl_client : public client {
  curl_client() : curl{curl_easy_init()} {}

  bool get(const std::string& url) override;
  bool post(const std::string& url,
            std::span<const post_field> fields = {}) override;

  std::string error() override { return curl_easy_strerror(ret); }
  std::string result() override
  {
    if (ret != CURLE_OK) { return {}; }
    return content;
  }
  std::uint32_t return_code() override
  {
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    ASSERT(http_code >= 0);
    ASSERT(http_code < 16384);

    return http_code;
  }

  ~curl_client() { curl_easy_cleanup(curl); }

 private:
  CURL* curl{};
  CURLcode ret{CURLE_OK};  // return value of last perform()
  std::string content;

  void reset()
  {
    content.clear();
    ret = CURLE_OK;
    curl_easy_reset(curl);
  }

  bool perform();
};

};  // namespace auth::Oidc::http

#endif  // BAREOS_LIB_BAUTH_OIDC_CURL_HTTP_H_
