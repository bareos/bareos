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

#ifndef BAREOS_LIB_TLS_CONF_CERT_H_
#define BAREOS_LIB_TLS_CONF_CERT_H_

class DLL_IMP_EXP TlsConfigCert : public TlsConfigBase {
public:
   bool authenticate;         /* Authenticate with TLS */
   bool VerifyPeer;           /* TLS Verify Peer Certificate */
   std::string *CaCertfile;   /* TLS CA Certificate File */
   std::string *CaCertdir;    /* TLS CA Certificate Directory */
   std::string *crlfile;      /* TLS CA Certificate Revocation List File */
   std::string *certfile;     /* TLS Client Certificate File */
   std::string *keyfile;      /* TLS Client Key File */
   std::string *cipherlist;   /* TLS Cipher List */
   std::string *dhfile;       /* TLS Diffie-Hellman File */
   alist *AllowedCns;         /* TLS Allowed Certificate Common Names (Clients) */

   std::string *pem_message;

   TlsConfigCert()
      : TlsConfigBase(), authenticate(false), VerifyPeer(0),
        CaCertfile(nullptr), CaCertdir(nullptr), crlfile(nullptr), certfile(nullptr),
        keyfile(nullptr), cipherlist(nullptr), dhfile(nullptr), AllowedCns(nullptr),
        pem_message(nullptr) {}
   ~TlsConfigCert();

   virtual uint32_t GetPolicy() const override;

   int (*TlsPemCallback)(char *buf, int size, const void *userdata);

   bool GetVerifyPeer() const override { return VerifyPeer; }
   alist *GetVerifyList() const override { return AllowedCns; }
   bool GetAuthenticate() const override { return authenticate; }

//   std::shared_ptr<Tls> CreateClientContext() const override;
//   std::shared_ptr<Tls> CreateServerContext() const override;

   /**
    * Checks whether the given @param policy matches the configured value
    * @param policy
    * @return true if policy means enabled
    */
   static bool enabled(u_int32_t policy);

   /**
    * Checks whether the given @param policy matches the configured value
    * @param policy
    * @return true if policy means required
    */
   static bool required(u_int32_t policy);

private:
   static u_int32_t const policy_offset = 0;
};

#endif /* BAREOS_LIB_TLS_CONF_CERT_H_ */
