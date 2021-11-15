/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2021 Bareos GmbH & Co. KG

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

class TlsConfigCert {
 public:
  bool verify_peer_ = false; /* TLS Verify Peer Certificate */
  std::string ca_certfile_;  /* TLS CA Certificate File */
  std::string ca_certdir_;   /* TLS CA Certificate Directory */
  std::string crlfile_;      /* TLS CA Certificate Revocation List File */
  std::string certfile_;     /* TLS Client Certificate File */
  std::string keyfile_;      /* TLS Client Key File */
  std::string dhfile_;       /* TLS Diffie-Hellman File */
  std::vector<std::string> allowed_certificate_common_names_;

  // std::string pem_message_; /* not implemented */

  TlsConfigCert() = default;
  virtual ~TlsConfigCert() = default;

  int (*TlsPemCallback)(char* buf, int size, const void* userdata);
};

#endif  // BAREOS_LIB_TLS_CONF_CERT_H_
