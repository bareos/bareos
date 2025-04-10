/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
// Author: Landon Fuller <landonf@opendarwin.org>
/**
 * @file
 * crypto.h Encryption support functions
 */

#ifndef BAREOS_LIB_CRYPTO_H_
#define BAREOS_LIB_CRYPTO_H_

#include <optional>
#include <cstdint>
#include <string>

template <typename T> class alist;
class JobControlRecord;

/* Opaque X509 Public/Private Key Pair Structure */
typedef struct X509_Keypair X509_KEYPAIR;

/* Opaque Message Signature Structure */
typedef struct Signature SIGNATURE;

/* Opaque PKI Symmetric Key Data Structure */
typedef struct Crypto_Session CRYPTO_SESSION;

/* Opaque Encryption/Decryption Context Structure */
typedef struct Cipher_Context CIPHER_CONTEXT;

/* PEM Decryption Passphrase Callback */
typedef int(CRYPTO_PEM_PASSWD_CB)(char* buf, int size, const void* userdata);

/** Server TLS-PSK callback */
typedef int(CRYPTO_TLS_PSK_SERVER_CB)(const char* identity,
                                      unsigned char* psk,
                                      unsigned int max_psk_len);

/** Client TLS-PSK callback */
typedef int(CRYPTO_TLS_PSK_CLIENT_CB)(char* identity,
                                      unsigned int max_identity_len,
                                      unsigned char* psk,
                                      unsigned int max_psk_len);

/* Digest Types */
typedef enum
{
  /* These are stored on disk and MUST NOT change */
  CRYPTO_DIGEST_NONE = 0,
  CRYPTO_DIGEST_MD5 = 1,
  CRYPTO_DIGEST_SHA1 = 2,
  CRYPTO_DIGEST_SHA256 = 3,
  CRYPTO_DIGEST_SHA512 = 4,
  CRYPTO_DIGEST_XXH128 = 5
} crypto_digest_t;

/* Cipher Types */
typedef enum
{
  /* These are not stored on disk */
  CRYPTO_CIPHER_NONE = 0,
  CRYPTO_CIPHER_BLOWFISH_CBC = 1,
  CRYPTO_CIPHER_3DES_CBC = 2,
  CRYPTO_CIPHER_AES_128_CBC = 3,
  CRYPTO_CIPHER_AES_192_CBC = 4,
  CRYPTO_CIPHER_AES_256_CBC = 5,
  CRYPTO_CIPHER_CAMELLIA_128_CBC = 6,
  CRYPTO_CIPHER_CAMELLIA_192_CBC = 7,
  CRYPTO_CIPHER_CAMELLIA_256_CBC = 8,
  CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1 = 9,
  CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1 = 10
} crypto_cipher_t;

/* Crypto API Errors */
typedef enum
{
  CRYPTO_ERROR_NONE = 0,           /* No error */
  CRYPTO_ERROR_NOSIGNER = 1,       /* Signer not found */
  CRYPTO_ERROR_NORECIPIENT = 2,    /* Recipient not found */
  CRYPTO_ERROR_INVALID_DIGEST = 3, /* Unsupported digest algorithm */
  CRYPTO_ERROR_INVALID_CRYPTO = 4, /* Unsupported encryption algorithm */
  CRYPTO_ERROR_BAD_SIGNATURE = 5,  /* Signature is invalid */
  CRYPTO_ERROR_DECRYPTION = 6,     /* Decryption error */
  CRYPTO_ERROR_INTERNAL = 7        /* Internal Error */
} crypto_error_t;

/* Message Digest Sizes */
#define CRYPTO_DIGEST_MD5_SIZE 16    /* 128 bits */
#define CRYPTO_DIGEST_SHA1_SIZE 20   /* 160 bits */
#define CRYPTO_DIGEST_SHA256_SIZE 32 /* 256 bits */
#define CRYPTO_DIGEST_SHA512_SIZE 64 /* 512 bits */
#define CRYPTO_DIGEST_XXH128_SIZE 16 /* 128 bits */

/* Maximum Message Digest Size */
#define CRYPTO_DIGEST_MAX_SIZE 64
#define CRYPTO_CIPHER_MAX_BLOCK_SIZE 32

class DigestInitException : public std::exception {};

struct Digest {
  JobControlRecord* jcr;
  crypto_digest_t type;

  Digest(JobControlRecord* t_jcr, crypto_digest_t t_type)
      : jcr(t_jcr), type(t_type)
  {
  }
  virtual ~Digest() = default;
  virtual bool Update(const uint8_t* data, uint32_t length) = 0;
  virtual bool Finalize(uint8_t* data, uint32_t* length) = 0;
};
/* Opaque Message Digest Structure */
typedef struct Digest DIGEST;


int InitCrypto(void);
int CleanupCrypto(void);
DIGEST* crypto_digest_new(JobControlRecord* jcr, crypto_digest_t type);
inline bool CryptoDigestUpdate(DIGEST* digest,
                               const uint8_t* data,
                               uint32_t length)
{
  return digest->Update(data, length);
}
inline bool CryptoDigestFinalize(DIGEST* digest,
                                 uint8_t* dest,
                                 uint32_t* length)
{
  return digest->Finalize(dest, length);
}
void CryptoDigestFree(DIGEST* digest);
SIGNATURE* crypto_sign_new(JobControlRecord* jcr);
crypto_error_t CryptoSignGetDigest(SIGNATURE* sig,
                                   X509_KEYPAIR* keypair,
                                   crypto_digest_t& algorithm,
                                   DIGEST** digest);
crypto_error_t CryptoSignVerify(SIGNATURE* sig,
                                X509_KEYPAIR* keypair,
                                DIGEST* digest);
int CryptoSignAddSigner(SIGNATURE* sig, DIGEST* digest, X509_KEYPAIR* keypair);
int CryptoSignEncode(SIGNATURE* sig, uint8_t* dest, uint32_t* length);
SIGNATURE* crypto_sign_decode(JobControlRecord* jcr,
                              const uint8_t* sigData,
                              uint32_t length);
void CryptoSignFree(SIGNATURE* sig);
CRYPTO_SESSION* crypto_session_new(crypto_cipher_t cipher,
                                   alist<X509_KEYPAIR*>* pubkeys);
void CryptoSessionFree(CRYPTO_SESSION* cs);
bool CryptoSessionEncode(CRYPTO_SESSION* cs, uint8_t* dest, uint32_t* length);
crypto_error_t CryptoSessionDecode(const uint8_t* data,
                                   uint32_t length,
                                   alist<X509_KEYPAIR*>* keypairs,
                                   CRYPTO_SESSION** session);
CRYPTO_SESSION* CryptoSessionDecode(const uint8_t* data, uint32_t length);
CIPHER_CONTEXT* crypto_cipher_new(CRYPTO_SESSION* cs,
                                  bool encrypt,
                                  uint32_t* blocksize);
bool CryptoCipherUpdate(CIPHER_CONTEXT* cipher_ctx,
                        const uint8_t* data,
                        uint32_t length,
                        const uint8_t* dest,
                        uint32_t* written);
bool CryptoCipherFinalize(CIPHER_CONTEXT* cipher_ctx,
                          uint8_t* dest,
                          uint32_t* written);
void CryptoCipherFree(CIPHER_CONTEXT* cipher_ctx);
X509_KEYPAIR* crypto_keypair_new(void);
X509_KEYPAIR* crypto_keypair_dup(X509_KEYPAIR* keypair);
int CryptoKeypairLoadCert(X509_KEYPAIR* keypair, const char* file);
bool CryptoKeypairHasKey(const char* file);
int CryptoKeypairLoadKey(X509_KEYPAIR* keypair,
                         const char* file,
                         CRYPTO_PEM_PASSWD_CB* pem_callback,
                         const void* pem_userdata);
void CryptoKeypairFree(X509_KEYPAIR* keypair);
int CryptoDefaultPemCallback(char* buf, int size, const void* userdata);
const char* crypto_digest_name(crypto_digest_t type);
const char* crypto_digest_name(DIGEST* digest);
crypto_digest_t CryptoDigestStreamType(int stream);
const char* crypto_strerror(crypto_error_t error);

std::optional<std::string> compute_hash(const std::string& unhashed,
                                        const std::string& digestname
                                        = "SHA256");

#endif  // BAREOS_LIB_CRYPTO_H_
