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

#ifndef BAREOS_LIB_TLS_CONF_BASE_H_
#define BAREOS_LIB_TLS_CONF_BASE_H_

struct PskCredentials;

class DLL_IMP_EXP TlsConfigBase {
public:
   bool enable;  /*!< Enable TLS */
   bool require; /*!< Require TLS */

   virtual uint32_t GetPolicy() const = 0;

   virtual void SetPskCredentials(const PskCredentials &credentials) {};

   virtual bool GetAuthenticate() const { return false; }
   virtual bool GetVerifyPeer() const { return false; }
   virtual std::vector<std::string> AllowedCertificateCommonNames() const { return std::vector<std::string>(); }

   typedef enum {
      BNET_TLS_NONE = 0,            /*!< cannot do TLS */
      BNET_TLS_ENABLED = 1 << 0,    /*!< TLS with certificates is allowed but not required on my end */
      BNET_TLS_REQUIRED = 1 << 1,   /*!< TLS with certificates is required */
   } Policy_e;

protected:
   TlsConfigBase() : enable(false), require(false) {}
   virtual ~TlsConfigBase() {}
};

#endif /* BAREOS_LIB_TLS_CONF_BASE_H_ */
