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

#ifndef BAREOS_LIB_TLS_CONF_NONE_H_
#define BAREOS_LIB_TLS_CONF_NONE_H_

class TlsConfigNone : public TlsConfigBase {

 public:
   char *cipherlist; /* TLS Cipher List */

   TlsConfigNone() : TlsConfigBase(), cipherlist(nullptr) {}
   ~TlsConfigNone() {};

   virtual uint32_t GetPolicy() const override { return BNET_TLS_NONE; }
//   std::shared_ptr<Tls> CreateClientContext() const override { return nullptr; }
//   std::shared_ptr<Tls> CreateServerContext() const override { return nullptr; }
   static bool enabled(u_int32_t policy) { return false; }
   static bool required(u_int32_t policy) { return false; }
};

#endif /* BAREOS_LIB_TLS_CONF_NONE_H_ */
