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

#include "cram_md5.h"

#include <cstdlib>
#include <sstream>
#include <string_view>
#include <openssl/evp.h>
#include <openssl/core_names.h>

#ifndef AUTH_NAME
#  error AUTH_NAME missing
#endif

namespace auth::CramMd5 {
struct md5mac {
  char data[16];
};

md5mac do_hmac_md5(const char* text,
                   size_t text_len,
                   const char* key,
                   size_t key_len)
{
  EVP_MAC* hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
  EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(hmac);

  OSSL_PARAM params[] = {
      OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char*)"MD5", 3),
      OSSL_PARAM_construct_end()};

  EVP_MAC_init(ctx, reinterpret_cast<const unsigned char*>(key), key_len,
               params);

  EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(text), text_len);

  md5mac result{};
  size_t size{};

  EVP_MAC_final(ctx, reinterpret_cast<unsigned char*>(result.data), &size,
                sizeof(result.data));

  EVP_MAC_CTX_free(ctx);
  EVP_MAC_free(hmac);

  return result;
}

std::string_view Verifier::name() const { return AUTH_NAME; }

std::vector<char> Verifier::generate_challenge()
{
  std::stringstream challenge_builder{};
  challenge_builder << "<" << (uint32_t)random() << "." << (uint32_t)time(NULL)
                    << "@" << my_name << ">";

  auto result = challenge_builder.str();

  auto mac = do_hmac_md5(result.data(), result.size(), password.data(),
                         password.size());

  expected.insert(expected.end(), mac.data, mac.data + std::size(mac.data));

  current_state = GENERATED_CHALLENGE;
  return {result.begin(), result.end()};
}

auth::Status Verifier::step(std::vector<char>&, std::span<const char> received)
{
  switch (current_state) {
    case GENERATED_CHALLENGE: {
      if (received.size() != expected.size()
          || memcmp(received.data(), expected.data(), expected.size()) != 0) {
        error = "received bad answer\n";
        current_state = ERROR;

        return auth::Status::Error;
      }

      current_state = ACCEPTED_RESULT;
      Dmsg0(100, "received correct answer\n");
      return auth::Status::Done;
    } break;
    default: {
      if (error.empty()) {
        error = "unexpected state " + std::to_string(current_state);
      }

      current_state = ERROR;
      return auth::Status::Error;
    } break;
  }
  return auth::Status::Error;
}

std::string_view Prover::name() const { return AUTH_NAME; }

bool Prover::step(std::vector<char>& response, std::span<const char> request)
{
  switch (current_state) {
    case WAITING_FOR_CHALLENGE: {
      // request should look like
      // <*.*@name>
      // we first need to make sure that we are not fed our own request,
      // i.e. check that our name is not that name

      std::string_view view{request.begin(), request.end()};
      auto start = view.find('@');
      if (start == view.npos) {
        error = "no @ found in request";
        current_state = ERROR;
        return false;
      }

      auto end = view.find('>', start);
      if (end == view.npos) {
        error = "no > found after @ in request";
        current_state = ERROR;
        return false;
      }

      if (end != view.size() - 1) {
        error = "bad position for closing > in request";
        current_state = ERROR;
        return false;
      }

      // a char cannot both be < and @
      ASSERT(end - start >= 1);

      auto received_name = view.substr(start + 1, end - start - 1);

      // this is _technically_ ok, but we simply reject them
      // as no bareos sender would ever do this
      if (received_name.size() == 0) {
        error = "received empty name in request";
        current_state = ERROR;
        return false;
      }

      if (received_name == my_name) {
        // TODO: check if we really need the exception for R_STORAGE
        error = "cannot accept my own name";
        current_state = ERROR;
        return false;
      }


      auto mac = do_hmac_md5(view.data(), view.size(), password.data(),
                             password.size());

      response.insert(response.end(), mac.data, mac.data + std::size(mac.data));

      current_state = RESPONSE_SENT;
      return true;
    } break;
    case RESPONSE_SENT: {
      error = "unexpected request sent (already sent response once)";
      current_state = ERROR;
      return false;
    } break;
    default: {
      if (error.empty()) {
        error = "internal error (bad state: ";
        error += std::to_string(current_state);
        error += ")";
      }
      return false;
    } break;
  }
}
};  // namespace auth::CramMd5
