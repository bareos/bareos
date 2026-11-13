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
#include <jansson.h>
#include "http.h"

#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace auth::Oidc {
struct cleanup {
  void operator()(json_t* json) const
  {
    if (json) { json_decref(json); }
  }

#define DEFINE_OPENSSL_DESTRUCTOR(Type) \
  void operator()(Type* ptr) const      \
  {                                     \
    if (ptr) { Type##_free(ptr); }      \
  }

  DEFINE_OPENSSL_DESTRUCTOR(EVP_PKEY);
  DEFINE_OPENSSL_DESTRUCTOR(EVP_PKEY_CTX);
  DEFINE_OPENSSL_DESTRUCTOR(EVP_MD_CTX);
  DEFINE_OPENSSL_DESTRUCTOR(X509);
  DEFINE_OPENSSL_DESTRUCTOR(BIO);

#undef DEFINE_OPENSSL_DESTRUCTOR
};

template <typename T> void dump_json(T* result, json_t* json)
{
  json_dump_callback(
      json,
      +[](const char* buffer, size_t size, void* arg) -> int {
        auto* output = static_cast<T*>(arg);
        output->insert(output->end(), buffer, buffer + size);
        return 0;
      },
      result, JSON_COMPACT);
}
}  // namespace auth::Oidc

using json_ptr = std::unique_ptr<json_t, auth::Oidc::cleanup>;

using evp_pkey = std::unique_ptr<EVP_PKEY, auth::Oidc::cleanup>;
using evp_pkey_ctx = std::unique_ptr<EVP_PKEY_CTX, auth::Oidc::cleanup>;
using evp_md_ctx = std::unique_ptr<EVP_MD_CTX, auth::Oidc::cleanup>;
using x509 = std::unique_ptr<X509, auth::Oidc::cleanup>;
using bio = std::unique_ptr<BIO, auth::Oidc::cleanup>;

#endif  // BAREOS_LIB_BAUTH_OIDC_COMMON_H_
