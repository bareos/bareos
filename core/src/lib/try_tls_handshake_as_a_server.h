/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_TRY_TLS_HANDSHAKE_AS_A_SERVER_H_
#define BAREOS_LIB_TRY_TLS_HANDSHAKE_AS_A_SERVER_H_

#include <ranges>
#include <sstream>
#include "include/jcr.h"
#include "lib/bsock.h"
#include "lib/parse_conf.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/util.h"

struct UsePasswordsFromConfig : TlsSecretProvider {
  UsePasswordsFromConfig(ConfigurationParser* p) : parser{p}
  {
    // keep a shared_ptr to the current config, so a reload won't
    // free the memory we're going to use in the private context
    config_table_ = parser->GetResourcesContainer();
  }


  unsigned int get_shared_secret_for(std::string_view identity,
                                     std::span<unsigned char> output) override
  {
    auto [type, name] = global_resource::ParseQualifiedName(identity);
    auto r_type = parser->LocalTypeFromGlobalType(type);

    if (r_type < 0) {
      Dmsg1(100, "Could not parse resource type from %.*s.\n",
            (int)identity.size(), identity.data());
      return 0;
    }

    auto* res = parser->GetResWithName(r_type, name);
    if (!res) {
      auto type_name = global_resource::GetNameFromType(type);
      Dmsg1(100, "Could not find resource %.*s of type %.*s (%d).\n",
            (int)name.size(), name.data(), (int)type_name.size(),
            type_name.data(), r_type);
      return 0;
    }
    TlsResource* tls = dynamic_cast<TlsResource*>(res);
    if (!tls) {
      Dmsg1(100, "Could not get tls resource for %d.\n", r_type);
      return 0;
    }

    auto pw_len = strlen(tls->password_.value);
    if (pw_len >= output.size()) {
      Dmsg1(100, "Secret key for resource %s is too long: %zu vs max %zu\n",
            res->resource_name_, pw_len, output.size());
      return 0;
    }

    Dmsg1(200, "Using resource %s secret for the tls connection\n",
          res->resource_name_);
    memcpy(output.data(), tls->password_.value, pw_len);
    output[pw_len] = 0;
    chosen_resource = res;
    return pw_len;
  }

  std::optional<std::string> is_resource_name_different_from_tls_name(
      uint32_t resource_type,
      std::string_view name)
  {
    // if psk was not used, then we cannot reject anything!
    if (!chosen_resource) { return std::nullopt; }

    std::string unbashed_name{name};
    UnbashSpaces(unbashed_name.data());

    if (chosen_resource->rcode_ != resource_type
        || unbashed_name != chosen_resource->resource_name_) {
      std::stringstream msg;

      msg << "tried authenticating as '" << parser->ResToStr(resource_type)
          << "::" << unbashed_name << "' but used psk for '"
          << parser->ResToStr(chosen_resource->rcode_)
          << "::" << chosen_resource->resource_name_ << "'";

      return std::optional{msg.str()};
    }

    return std::nullopt;
  }

  // if tls-psk was _not_ used, this just returns nullptr.
  // the return value of this function in and of itself cannot be used
  // to determine whether tls-psk was used or not
  BareosResource* get_resource_for_used_secret() const
  {
    return chosen_resource;
  }

 protected:
  ConfigurationParser* parser;
  BareosResource* chosen_resource{nullptr};
  std::shared_ptr<ConfigResourcesContainer> config_table_;
};

// look not only in the config for passwords, but also
// allow tls via secrets set in the jcrs (sd_auth_key)
struct UseConfigAndJcrs : UsePasswordsFromConfig {
  UseConfigAndJcrs(ConfigurationParser* p) : UsePasswordsFromConfig{p} {}

  JobControlRecord* found_jcr{nullptr};

  unsigned int get_shared_secret_for(
      std::string_view identity,
      std::span<unsigned char> psk_buffer) override
  {
    auto [type, name] = global_resource::ParseQualifiedName(identity);

    if (type == global_resource::Type::Job) {
      Dmsg1(200, "Searching for job %.*s to create a tls connection\n",
            (int)name.size(), name.data());
      auto* jcr = get_jcr_by_full_name(name);
      if (!jcr) {
        Dmsg1(100,
              "Login attempt via job %.*s, but no such job is known to me\n",
              (int)name.size(), name.data());
        return 0;
      }
      found_jcr = jcr;

      auto key_length = strlen(jcr->sd_auth_key);
      if (key_length >= psk_buffer.size()) {
        Dmsg1(100, "Secret key for job %s is too long: %zu vs max %zu\n",
              jcr->Job, key_length, psk_buffer.size());
        return 0;
      }

      Dmsg1(200, "Using job %s secret for the tls connection\n", jcr->Job);
      memcpy(psk_buffer.data(), jcr->sd_auth_key, key_length);
      psk_buffer[key_length] = 0;
      return key_length;
    } else {
      return UsePasswordsFromConfig::get_shared_secret_for(identity,
                                                           psk_buffer);
    }
  }

  std::optional<std::string> check_job_name(std::string_view jobname)
  {
    if (auto* res = get_resource_for_used_secret()) {
      std::stringstream msg;

      msg << "tried authenticating as job " << jobname << " but used psk for '"
          << parser->ResToStr(res->rcode_) << "::" << res->resource_name_
          << "'";

      return std::optional{msg.str()};
    }

    if (!found_jcr) { return std::nullopt; }

    if (jobname != found_jcr->Job) {
      std::stringstream msg;

      msg << "tried authenticating as job " << jobname
          << " but used psk for job " << found_jcr->Job;

      return std::optional{msg.str()};
    }

    return std::nullopt;
  }

  std::optional<std::string> is_resource_name_different_from_tls_name(
      uint32_t resource_type,
      std::string_view name)
  {
    if (found_jcr) {
      std::stringstream msg;

      msg << "tried authenticating as resource "
          << parser->ResToStr(resource_type) << "::" << name
          << " but used psk for job " << found_jcr->Job;

      return std::optional{msg.str()};
    }

    return UsePasswordsFromConfig::is_resource_name_different_from_tls_name(
        resource_type, name);
  }

  JobControlRecord* get_job_for_used_secret() const { return found_jcr; }

  bool psk_used() const
  {
    return get_job_for_used_secret() != nullptr
           || get_resource_for_used_secret() != nullptr;
  }

  ~UseConfigAndJcrs()
  {
    if (found_jcr) { FreeJcr(found_jcr); }
  }
};

bool TryTlsHandshakeAsAServer(BareosSocket* bsock,
                              ConfigurationParser* parser,
                              TlsSecretProvider* data);

#endif  // BAREOS_LIB_TRY_TLS_HANDSHAKE_AS_A_SERVER_H_
