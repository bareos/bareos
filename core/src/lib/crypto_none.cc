/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.

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
/*
 * crypto_none.c Encryption support functions when no backend.
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 */

#include "include/bareos.h"

#if !defined(HAVE_OPENSSL)

#include "jcr.h"
#include <assert.h>

/* Message Digest Structure */
struct Digest {
  crypto_digest_t type;
  JobControlRecord* jcr;
  union {
    SHA1_CTX sha1;
    MD5_CTX md5;
  };
};

/* Dummy Signature Structure */
struct Signature {
  JobControlRecord* jcr;
};

DIGEST* crypto_digest_new(JobControlRecord* jcr, crypto_digest_t type)
{
  DIGEST* digest;

  digest = (DIGEST*)malloc(sizeof(DIGEST));
  digest->type = type;
  digest->jcr = jcr;

  switch (type) {
    case CRYPTO_DIGEST_MD5:
      MD5_Init(&digest->md5);
      break;
    case CRYPTO_DIGEST_SHA1:
      SHA1Init(&digest->sha1);
      break;
    default:
      Jmsg1(jcr, M_ERROR, 0, _("Unsupported digest type=%d specified\n"), type);
      free(digest);
      return NULL;
  }

  return (digest);
}

bool CryptoDigestUpdate(DIGEST* digest, const uint8_t* data, uint32_t length)
{
  switch (digest->type) {
    case CRYPTO_DIGEST_MD5:
      /* Doesn't return anything ... */
      MD5_Update(&digest->md5, (unsigned char*)data, length);
      return true;
    case CRYPTO_DIGEST_SHA1:
      /* Doesn't return anything ... */
      SHA1Update(&digest->sha1, (const u_int8_t*)data, (unsigned int)length);
      return true;
    default:
      return false;
  }
}

bool CryptoDigestFinalize(DIGEST* digest, uint8_t* dest, uint32_t* length)
{
  switch (digest->type) {
    case CRYPTO_DIGEST_MD5:
      /* Guard against programmer error by either the API client or
       * an out-of-sync CRYPTO_DIGEST_MAX_SIZE */
      assert(*length >= CRYPTO_DIGEST_MD5_SIZE);
      *length = CRYPTO_DIGEST_MD5_SIZE;
      /* Doesn't return anything ... */
      MD5_Final((unsigned char*)dest, &digest->md5);
      return true;
    case CRYPTO_DIGEST_SHA1:
      /* Guard against programmer error by either the API client or
       * an out-of-sync CRYPTO_DIGEST_MAX_SIZE */
      assert(*length >= CRYPTO_DIGEST_SHA1_SIZE);
      *length = CRYPTO_DIGEST_SHA1_SIZE;
      SHA1Final((u_int8_t*)dest, &digest->sha1);
      return true;
    default:
      return false;
  }

  return false;
}

void CryptoDigestFree(DIGEST* digest) { free(digest); }

SIGNATURE* crypto_sign_new(JobControlRecord* jcr) { return NULL; }

crypto_error_t CryptoSignGetDigest(SIGNATURE* sig,
                                   X509_KEYPAIR* keypair,
                                   crypto_digest_t& type,
                                   DIGEST** digest)
{
  return CRYPTO_ERROR_INTERNAL;
}

crypto_error_t CryptoSignVerify(SIGNATURE* sig,
                                X509_KEYPAIR* keypair,
                                DIGEST* digest)
{
  return CRYPTO_ERROR_INTERNAL;
}

int CryptoSignAddSigner(SIGNATURE* sig, DIGEST* digest, X509_KEYPAIR* keypair)
{
  return false;
}

int CryptoSignEncode(SIGNATURE* sig, uint8_t* dest, uint32_t* length)
{
  return false;
}

SIGNATURE* crypto_sign_decode(JobControlRecord* jcr,
                              const uint8_t* sigData,
                              uint32_t length)
{
  return NULL;
}

void CryptoSignFree(SIGNATURE* sig) {}

X509_KEYPAIR* crypto_keypair_new(void) { return NULL; }

X509_KEYPAIR* crypto_keypair_dup(X509_KEYPAIR* keypair) { return NULL; }

int CryptoKeypairLoadCert(X509_KEYPAIR* keypair, const char* file)
{
  return false;
}

bool CryptoKeypairHasKey(const char* file) { return false; }

int CryptoKeypairLoadKey(X509_KEYPAIR* keypair,
                         const char* file,
                         CRYPTO_PEM_PASSWD_CB* pem_callback,
                         const void* pem_userdata)
{
  return false;
}

void CryptoKeypairFree(X509_KEYPAIR* keypair) {}

CRYPTO_SESSION* crypto_session_new(crypto_cipher_t cipher, alist* pubkeys)
{
  return NULL;
}

void CryptoSessionFree(CRYPTO_SESSION* cs) {}

bool CryptoSessionEncode(CRYPTO_SESSION* cs, uint8_t* dest, uint32_t* length)
{
  return false;
}

crypto_error_t CryptoSessionDecode(const uint8_t* data,
                                   uint32_t length,
                                   alist* keypairs,
                                   CRYPTO_SESSION** session)
{
  return CRYPTO_ERROR_INTERNAL;
}

CIPHER_CONTEXT* crypto_cipher_new(CRYPTO_SESSION* cs,
                                  bool encrypt,
                                  uint32_t* blocksize)
{
  return NULL;
}

bool CryptoCipherUpdate(CIPHER_CONTEXT* cipher_ctx,
                        const uint8_t* data,
                        uint32_t length,
                        const uint8_t* dest,
                        uint32_t* written)
{
  return false;
}

bool CryptoCipherFinalize(CIPHER_CONTEXT* cipher_ctx,
                          uint8_t* dest,
                          uint32_t* written)
{
  return false;
}

void CryptoCipherFree(CIPHER_CONTEXT* cipher_ctx) {}

const char* crypto_digest_name(DIGEST* digest)
{
  return crypto_digest_name(digest->type);
}
#endif /* HAVE_CRYPTO */

#if !defined(HAVE_OPENSSL) && !defined(HAVE_NSS)
/*
 * Dummy initialization functions when no crypto framework found.
 */
int InitCrypto(void) { return 0; }

int CleanupCrypto(void) { return 0; }
#endif /* !HAVE_OPENSSL && !HAVE_NSS */
