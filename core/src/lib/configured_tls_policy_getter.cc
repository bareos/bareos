/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#include "lib/configured_tls_policy_getter.h"
#include "lib/parse_conf.h"

#include "include/jcr.h"
#include "lib/ascii_control_characters.h"
#include "lib/bstringlist.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "include/make_unique.h"

#include <algorithm>

class ConfiguredTlsPolicyGetterPrivate {
 public:
  ConfiguredTlsPolicyGetterPrivate(const ConfigurationParser& my_config)
      : my_config_(my_config)
  {
    return;
  }
  ~ConfiguredTlsPolicyGetterPrivate() = default;

  TlsPolicy GetTlsPolicyForRootConsole() const;
  TlsPolicy GetTlsPolicyForJob(const std::string& name) const;
  TlsPolicy GetTlsPolicyForResourceCodeAndName(const std::string& r_code_str,
                                               const std::string& name) const;
  const ConfigurationParser& my_config_;
};

ConfiguredTlsPolicyGetter::ConfiguredTlsPolicyGetter(
    const ConfigurationParser& my_config)
    : impl_(std::make_unique<ConfiguredTlsPolicyGetterPrivate>(my_config))
{
  return;
}

ConfiguredTlsPolicyGetter::~ConfiguredTlsPolicyGetter() = default;

TlsPolicy ConfiguredTlsPolicyGetterPrivate::GetTlsPolicyForRootConsole() const
{
  TlsResource* own_tls_resource = reinterpret_cast<TlsResource*>(
      my_config_.GetNextRes(my_config_.r_own_, nullptr));
  if (!own_tls_resource) {
    Dmsg1(100, "Could not find own tls resource: %d\n", my_config_.r_own_);
    return kBnetTlsUnknown;
  }
  return own_tls_resource->GetPolicy();
}

TlsPolicy ConfiguredTlsPolicyGetterPrivate::GetTlsPolicyForJob(
    const std::string& name) const
{
  BStringList job_information(name, AsciiControlCharacters::RecordSeparator());
  std::string unified_job_name;
  if (job_information.size() == 2) {
    unified_job_name = job_information[1].c_str();
  } else if (job_information.size() == 1) { /* client before Release 18.2 */
    unified_job_name = job_information[0];
    unified_job_name.erase(
        std::remove(unified_job_name.begin(), unified_job_name.end(), '\n'),
        unified_job_name.end());
  } else {
    Dmsg1(100, "Could not get unified job name: %s\n", name.c_str());
    return TlsPolicy::kBnetTlsUnknown;
  }
  return JcrGetTlsPolicy(unified_job_name.c_str());
}

TlsPolicy ConfiguredTlsPolicyGetterPrivate::GetTlsPolicyForResourceCodeAndName(
    const std::string& r_code_str,
    const std::string& name) const
{
  uint32_t r_code =
      my_config_.qualified_resource_name_type_converter_->StringToResourceType(
          r_code_str);
  if (r_code < 0) { return TlsPolicy::kBnetTlsUnknown; }

  TlsResource* foreign_tls_resource = reinterpret_cast<TlsResource*>(
      my_config_.GetResWithName(r_code, name.c_str()));
  if (!foreign_tls_resource) {
    Dmsg2(100, "Could not find foreign tls resource: %s-%s\n",
          r_code_str.c_str(), name.c_str());
    return TlsPolicy::kBnetTlsUnknown;
  }
  return foreign_tls_resource->GetPolicy();
}

bool ConfiguredTlsPolicyGetter::GetConfiguredTlsPolicyFromCleartextHello(
    const std::string& r_code_str,
    const std::string& name,
    TlsPolicy& tls_policy_out) const
{
  TlsPolicy tls_policy;
  if (name == std::string("*UserAgent*")) {
    tls_policy = impl_->GetTlsPolicyForRootConsole();
  } else if (r_code_str == std::string("R_JOB")) {
    tls_policy = impl_->GetTlsPolicyForJob(name);
  } else {
    tls_policy = impl_->GetTlsPolicyForResourceCodeAndName(r_code_str, name);
  }
  if (tls_policy == TlsPolicy::kBnetTlsUnknown) {
    Dmsg2(100, "Could not find foreign tls resource: %s-%s\n",
          r_code_str.c_str(), name.c_str());
    return false;
  } else {
    tls_policy_out = tls_policy;
    return true;
  }
}
