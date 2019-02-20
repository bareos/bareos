/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef LIB_CRAM_MD5_H_
#define LIB_CRAM_MD5_H_

class CramMd5Handshake {
 public:
  CramMd5Handshake(BareosSocket* bs,
                   const char* passwort,
                   TlsPolicy local_tls_policy);
  bool DoHandshake(bool initiated_by_remote);
  TlsPolicy RemoteTlsPolicy() const { return remote_tls_policy_; }

 private:
  static constexpr int debuglevel_ = 50;
  bool compatible_ = true;
  BareosSocket* bs_;
  const char* password_;
  TlsPolicy local_tls_policy_;
  TlsPolicy remote_tls_policy_;
  bool CramMd5Challenge();
  bool CramMd5Response();
  void InitRandom() const;
};

void hmac_md5(uint8_t* text,
              int text_len,
              uint8_t* key,
              int key_len,
              uint8_t* hmac);

#endif  // LIB_CRAM_MD5_H_
