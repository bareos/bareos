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

#ifndef BAREOS_TLS_CONF_H
#define BAREOS_TLS_CONF_H

/*
 * TLS enabling values. Value is important for comparison, ie:
 * if (tls_remote_policy < BNET_TLS_CERTIFICATE_REQUIRED) { ... }

 cert allowed    cert required    psk allowed    psk-required        illegal    combination name
0    0    0    0            none
0    0    0    1        x
0    0    1    0            psk allowed
0    0    1    1            psk required
0    1    0    0        x
0    1    0    1        x
0    1    1    0        x
0    1    1    1        x
1    0    0    0            cert allowed
1    0    0    1        x
1    0    1    0            both allowed
1    0    1    1        x
1    1    0    0            cert required
1    1    0    1        x
1    1    1    0        x
1    1    1    1        x

 * This bitfield has following valid combinations:
                        none         cert allowed    cert required    both allowed    psk allowed     psk required
    none               plain           plain         no connection     plain           plain          no connection
    cert allowed       plain           cert             cert           cert          no connection    no connection
    cert required   no connection      cert             cert           cert          no connection    no connection
    both allowed       plain           cert             cert           cert            psk               psk
    psk allowed        plain        no connection    no connection     psk             psk               psk
    psk required    no connection   no connection    no connection     psk             psk               psk
 */

/**
 * store all TLS specific settings
 * and backend specific context.
 */

class PskCredentials {
   std::string m_identity;
   std::string m_psk;

 public:
   PskCredentials(std::string &identity, std::string &psk) : m_identity(identity), m_psk(psk) {
      Dmsg2(100, "Construct PskCredentials: id=%s, passwd=%s\n", m_identity.c_str(), m_psk.c_str());
   }
   PskCredentials(const char *identity, const char *psk)
      : m_identity(std::string(identity)), m_psk(std::string(psk)) {
      Dmsg2(100, "Construct PskCredentials: id=%s, passwd=%s\n", m_identity.c_str(), m_psk.c_str());
   }

   std::string &get_identity() { return m_identity; }
   std::string &get_psk() { return m_psk; }

   ~PskCredentials() {
      Dmsg2(100, "Destruct PskCredentials: id=%s, passwd=%s\n", m_identity.c_str(), m_psk.c_str());
   }
};

class DLL_IMP_EXP tls_base_t {
 public:
   bool enable;  /*!< Enable TLS */
   bool require; /*!< Require TLS */

   /**
    * Abstract Methods
    */
   virtual uint32_t GetPolicy() const = 0;

   virtual std::shared_ptr<TLS_CONTEXT> CreateClientContext(
      std::shared_ptr<PskCredentials> credentials) const = 0;

   virtual std::shared_ptr<TLS_CONTEXT> CreateServerContext(
      std::shared_ptr<PskCredentials> credentials) const = 0;

   virtual bool GetAuthenticate() const { return false; }
   virtual bool GetVerifyPeer() const { return false; }
   virtual alist *GetVerifyList() const { return nullptr; }

 protected:
   typedef enum {
      BNET_TLS_NONE = 0,      /*!< cannot do TLS */
      BNET_TLS_ENABLED = 1 << 0, /*!< TLS with certificates is allowed but not required on my end */
      BNET_TLS_REQUIRED = 1 << 1, /*!< TLS with certificates is required */
   } Policy_e;

   virtual ~tls_base_t() {}
   tls_base_t() : enable(false), require(false) {}
};

class DLL_IMP_EXP tls_cert_t : public tls_base_t {
   static u_int32_t const policy_offset = 0;
 public:
   bool authenticate;       /* Authenticate with TLS */
   bool verify_peer;        /* TLS Verify Peer Certificate */
   std::string *ca_certfile; /* TLS CA Certificate File */
   std::string *ca_certdir;  /* TLS CA Certificate Directory */
   std::string *crlfile;     /* TLS CA Certificate Revocation List File */
   std::string *certfile;    /* TLS Client Certificate File */
   std::string *keyfile;     /* TLS Client Key File */
   std::string *cipherlist;  /* TLS Cipher List */
   std::string *dhfile;      /* TLS Diffie-Hellman File */
   alist *allowed_cns;      /* TLS Allowed Certificate Common Names (Clients) */
   //   TLS_CONTEXT *ctx;   /* Shared TLS Context */
   std::string *pem_message;
   tls_cert_t()
      : tls_base_t(), authenticate(false), verify_peer(0),
        ca_certfile(nullptr), ca_certdir(nullptr), crlfile(nullptr), certfile(nullptr),
        keyfile(nullptr), cipherlist(nullptr), dhfile(nullptr), allowed_cns(nullptr),
        pem_message(nullptr) {}
   ~tls_cert_t();

   virtual uint32_t GetPolicy() const override;

   int (*tls_pem_callback)(char *buf, int size, const void *userdata);

   bool GetVerifyPeer() const override { return verify_peer; }
   alist *GetVerifyList() const override { return allowed_cns; }
   bool GetAuthenticate() const override { return authenticate; }

   /**
    * Implementation can be found in appropriate tls-implementation line tls_openssl.c
    */
   std::shared_ptr<TLS_CONTEXT> CreateClientContext(
      std::shared_ptr<PskCredentials> credentials) const override;

   /**
    * Implementation can be found in appropriate tls-implementation line tls_openssl.c
    */
   std::shared_ptr<TLS_CONTEXT> CreateServerContext(
      std::shared_ptr<PskCredentials> credentials) const override;

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
};

class DLL_IMP_EXP tls_psk_t : public tls_base_t {
   static u_int32_t const policy_offset = 2;

 public:
   char *cipherlist; /* TLS Cipher List */

   tls_psk_t() : tls_base_t(), cipherlist(nullptr) {}
   ~tls_psk_t();

   virtual uint32_t GetPolicy() const override;

   /**
    * Implementation can be found in appropriate tls-implementation line tls_openssl.c
    */
   std::shared_ptr<TLS_CONTEXT> CreateClientContext(
      std::shared_ptr<PskCredentials> credentials) const override;

   /**
    * Implementation can be found in appropriate tls-implementation line tls_openssl.c
    */
   std::shared_ptr<TLS_CONTEXT> CreateServerContext(
      std::shared_ptr<PskCredentials> credentials) const override;

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
};

#endif //BAREOS_TLS_CONF_H
