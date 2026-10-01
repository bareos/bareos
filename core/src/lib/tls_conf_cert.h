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

#ifndef BAREOS_LIB_TLS_CONF_CERT_H_
#define BAREOS_LIB_TLS_CONF_CERT_H_

#include <string>
#include <vector>

enum class VerifyPeerSetting
{
  Never,        // we do not verify the peer at all
  IfAvailable,  // we only verify the peer when we received a certificate
  Always,       // we always verify the peer
};

namespace {
constexpr std::pair<std::string_view, VerifyPeerSetting>
    VerifyPeerSettingByName[] = {
        {"IfAvailable", VerifyPeerSetting::IfAvailable},
        {"Yes", VerifyPeerSetting::Always},
        {"No", VerifyPeerSetting::Never},
};

inline std::string as_str(VerifyPeerSetting to_convert)
{
  for (auto& [name, setting] : VerifyPeerSettingByName) {
    if (to_convert == setting) { return std::string{name}; }
  }

  return "<UNKNOWN>";
}
};  // namespace


class TlsConfigCert {
 public:
  VerifyPeerSetting verify_peer_{}; /* TLS Verify Peer Certificate */
  std::string ca_certfile_;         /* TLS CA Certificate File */
  std::string ca_certdir_;          /* TLS CA Certificate Directory */
  std::string crlfile_;  /* TLS CA Certificate Revocation List File */
  std::string certfile_; /* TLS Client Certificate File */
  std::string keyfile_;  /* TLS Client Key File */
  std::string dhfile_;   /* TLS Diffie-Hellman File */
  std::vector<std::string> allowed_certificate_common_names_;

  TlsConfigCert() = default;
  virtual ~TlsConfigCert() = default;
};

#endif  // BAREOS_LIB_TLS_CONF_CERT_H_
