/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2019 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "lib/alist.h"
#include "lib/tls_conf_cert.h"

TEST(bsock, config_tls_cert_verify_common_names_list_test)
{
  std::vector<std::string> list;
  const char* cstrings[3] = {"123", "456", "789"};
  list.push_back(std::string(cstrings[0]));
  list.push_back(std::string(cstrings[1]));
  list.push_back(std::string(cstrings[2]));

  TlsConfigCert tls_config_cert;

  /* list is destroyed by destructor of TlsConfigCert */
  tls_config_cert.allowed_certificate_common_names_ = list;

  std::vector<std::string> vec =
      tls_config_cert.allowed_certificate_common_names_;

  EXPECT_EQ(vec.size(), 3);
  EXPECT_STREQ(vec.at(0).c_str(), "123");
  EXPECT_STREQ(vec.at(1).c_str(), "456");
  EXPECT_STREQ(vec.at(2).c_str(), "789");
}
