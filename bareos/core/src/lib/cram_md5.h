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
#ifndef BAREOS_LIB_CRAM_MD5_H_
#define BAREOS_LIB_CRAM_MD5_H_

#include "lib/tls_conf.h"

class BareosSocket;

class CramMd5Handshake {
 public:
  CramMd5Handshake(BareosSocket* bs,
                   const char* passwort,
                   TlsPolicy local_tls_policy,
                   const std::string& own_qualified_name);
  bool DoHandshake(bool initiated_by_remote);
  TlsPolicy RemoteTlsPolicy() const { return remote_tls_policy_; }
  std::string destination_qualified_name_;

  enum class HandshakeResult
  {
    NOT_INITIALIZED,
    SUCCESS,
    FORMAT_MISMATCH,
    NETWORK_ERROR,
    WRONG_HASH,
    REPLAY_ATTACK
  };

  mutable HandshakeResult result{HandshakeResult::NOT_INITIALIZED};

 private:
  static constexpr int debuglevel_ = 50;
  bool compatible_ = true;
  BareosSocket* bs_ = nullptr;
  const char* password_ = nullptr;
  TlsPolicy local_tls_policy_ = kBnetTlsUnknown;
  TlsPolicy remote_tls_policy_ = kBnetTlsUnknown;
  const std::string own_qualified_name_;
  std::string own_qualified_name_bashed_spaces_;
  bool CramMd5Challenge();
  bool CramMd5Response();
  void InitRandom() const;

  enum class ComparisonResult
  {
    FAILURE,
    IS_SAME,
    IS_DIFFERENT
  };

  ComparisonResult CompareChallengeWithOwnQualifiedName(
      const char* challenge) const;
};

void hmac_md5(uint8_t* text,
              int text_len,
              uint8_t* key,
              int key_len,
              uint8_t* hmac);

#endif  // BAREOS_LIB_CRAM_MD5_H_
