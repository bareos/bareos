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

#ifndef BAREOS_LIB_TLS_CONF_H_
#define BAREOS_LIB_TLS_CONF_H_

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
   std::string identity_;
   std::string psk_;

 public:
   PskCredentials(std::string &identity, std::string &psk) : identity_(identity), psk_(psk) {
      Dmsg2(100, "Construct PskCredentials: id=%s, passwd=%s\n", identity_.c_str(), psk_.c_str());
   }
   PskCredentials(const char *identity, const char *psk)
      : identity_(std::string(identity)), psk_(std::string(psk)) {
      Dmsg2(100, "Construct PskCredentials: id=%s, passwd=%s\n", identity_.c_str(), psk_.c_str());
   }

   std::string &get_identity() { return identity_; }
   std::string &get_psk() { return psk_; }

   ~PskCredentials() {
      Dmsg2(100, "Destruct PskCredentials: id=%s, passwd=%s\n", identity_.c_str(), psk_.c_str());
   }
};

class DLL_IMP_EXP TlsBase {
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

   virtual ~TlsBase() {}
   TlsBase() : enable(false), require(false) {}
};

class DLL_IMP_EXP TlsCert : public TlsBase {
   static u_int32_t const policy_offset = 0;
 public:
   bool authenticate;       /* Authenticate with TLS */
   bool VerifyPeer;        /* TLS Verify Peer Certificate */
   std::string *CaCertfile; /* TLS CA Certificate File */
   std::string *CaCertdir;  /* TLS CA Certificate Directory */
   std::string *crlfile;     /* TLS CA Certificate Revocation List File */
   std::string *certfile;    /* TLS Client Certificate File */
   std::string *keyfile;     /* TLS Client Key File */
   std::string *cipherlist;  /* TLS Cipher List */
   std::string *dhfile;      /* TLS Diffie-Hellman File */
   alist *AllowedCns;      /* TLS Allowed Certificate Common Names (Clients) */
   //   TLS_CONTEXT *ctx;   /* Shared TLS Context */
   std::string *pem_message;
   TlsCert()
      : TlsBase(), authenticate(false), VerifyPeer(0),
        CaCertfile(nullptr), CaCertdir(nullptr), crlfile(nullptr), certfile(nullptr),
        keyfile(nullptr), cipherlist(nullptr), dhfile(nullptr), AllowedCns(nullptr),
        pem_message(nullptr) {}
   ~TlsCert();

   virtual uint32_t GetPolicy() const override;

   int (*TlsPemCallback)(char *buf, int size, const void *userdata);

   bool GetVerifyPeer() const override { return VerifyPeer; }
   alist *GetVerifyList() const override { return AllowedCns; }
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

class DLL_IMP_EXP TlsPsk : public TlsBase {
   static u_int32_t const policy_offset = 2;

 public:
   char *cipherlist; /* TLS Cipher List */

   TlsPsk() : TlsBase(), cipherlist(nullptr) {}
   ~TlsPsk();

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

class TlsResource;

uint32_t GetLocalTlsPolicyFromConfiguration(TlsResource *tls_configuration);
TlsBase *SelectTlsFromPolicy(TlsResource *tls_configuration, uint32_t remote_policy);

#endif //BAREOS_LIB_TLS_CONF_H_
