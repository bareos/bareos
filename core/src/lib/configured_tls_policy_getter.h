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

#ifndef BAREOS_LIB_CONFIGURED_TLS_POLICY_GETTER_H_
#define BAREOS_LIB_CONFIGURED_TLS_POLICY_GETTER_H_

#include "lib/tls_conf.h"

#include <memory>
#include <string>

class ConfiguredTlsPolicyGetterPrivate;
class ConfigurationParser;

class ConfiguredTlsPolicyGetter {
 public:
  ConfiguredTlsPolicyGetter(const ConfigurationParser& my_config);
  ~ConfiguredTlsPolicyGetter();
  bool GetConfiguredTlsPolicyFromCleartextHello(
      const std::string& r_code_str,
      const std::string& name,
      TlsPolicy& tls_policy_out) const;

 private:
  std::unique_ptr<ConfiguredTlsPolicyGetterPrivate> impl_;
};

#endif  // BAREOS_LIB_CONFIGURED_TLS_POLICY_GETTER_H_
