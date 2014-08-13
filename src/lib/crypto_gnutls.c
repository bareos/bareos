/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * crypto_gnutls.c Encryption support functions when GNUTLS backend.
 *
 * Author: Marco van Wieringen <marco.van.wieringen@bareos.com>
 */

#include "bareos.h"

#if defined(HAVE_GNUTLS)

#include "jcr.h"
#include <assert.h>

#include <gnutls/gnutls.h>

#if defined(HAVE_CRYPTO)

#include <gnutls/x509.h>

/* Message Digest Structure */
struct Digest {
   crypto_digest_t type;
   JCR *jcr;
   union {
      SHA1_CTX sha1;
      MD5_CTX md5;
   };
};

/* Dummy Signature Structure */
struct Signature {
   JCR *jcr;
};

DIGEST *crypto_digest_new(JCR *jcr, crypto_digest_t type)
{
   DIGEST *digest;

   digest = (DIGEST *)malloc(sizeof(DIGEST));
   digest->type = type;
   digest->jcr = jcr;

   switch (type) {
   case CRYPTO_DIGEST_MD5:
      MD5Init(&digest->md5);
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

bool crypto_digest_update(DIGEST *digest, const uint8_t *data, uint32_t length)
{
   switch (digest->type) {
   case CRYPTO_DIGEST_MD5:
      /* Doesn't return anything ... */
      MD5Update(&digest->md5, (unsigned char *) data, length);
      return true;
   case CRYPTO_DIGEST_SHA1:
      int ret;
      if ((ret = SHA1Update(&digest->sha1, (const u_int8_t *) data, length)) == shaSuccess) {
         return true;
      } else {
         Jmsg1(NULL, M_ERROR, 0, _("SHA1Update() returned an error: %d\n"), ret);
         return false;
      }
      break;
   default:
      return false;
   }
}

bool crypto_digest_finalize(DIGEST *digest, uint8_t *dest, uint32_t *length)
{
   switch (digest->type) {
   case CRYPTO_DIGEST_MD5:
      /* Guard against programmer error by either the API client or
       * an out-of-sync CRYPTO_DIGEST_MAX_SIZE */
      assert(*length >= CRYPTO_DIGEST_MD5_SIZE);
      *length = CRYPTO_DIGEST_MD5_SIZE;
      /* Doesn't return anything ... */
      MD5Final((unsigned char *)dest, &digest->md5);
      return true;
   case CRYPTO_DIGEST_SHA1:
      /* Guard against programmer error by either the API client or
       * an out-of-sync CRYPTO_DIGEST_MAX_SIZE */
      assert(*length >= CRYPTO_DIGEST_SHA1_SIZE);
      *length = CRYPTO_DIGEST_SHA1_SIZE;
      if (SHA1Final(&digest->sha1, (u_int8_t *) dest) == shaSuccess) {
         return true;
      } else {
         return false;
      }
      break;
   default:
      return false;
   }

   return false;
}

void crypto_digest_free(DIGEST *digest)
{
   free(digest);
}


SIGNATURE *crypto_sign_new(JCR *jcr)
{
   return NULL;
}

crypto_error_t crypto_sign_get_digest(SIGNATURE *sig, X509_KEYPAIR *keypair,
                                      crypto_digest_t &type, DIGEST **digest)
{
   return CRYPTO_ERROR_INTERNAL;
}

crypto_error_t crypto_sign_verify(SIGNATURE *sig, X509_KEYPAIR *keypair, DIGEST *digest)
{
   return CRYPTO_ERROR_INTERNAL;
}

int crypto_sign_add_signer(SIGNATURE *sig, DIGEST *digest, X509_KEYPAIR *keypair)
{
   return false;
}

int crypto_sign_encode(SIGNATURE *sig, uint8_t *dest, uint32_t *length)
{
   return false;
}

SIGNATURE *crypto_sign_decode(JCR *jcr, const uint8_t *sigData, uint32_t length)
{
   return NULL;
}

void crypto_sign_free(SIGNATURE *sig)
{
}

X509_KEYPAIR *crypto_keypair_new(void)
{
   return NULL;
}

X509_KEYPAIR *crypto_keypair_dup(X509_KEYPAIR *keypair)
{
   return NULL;
}

int crypto_keypair_load_cert(X509_KEYPAIR *keypair, const char *file)
{
   return false;
}

bool crypto_keypair_has_key(const char *file)
{
   return false;
}

int crypto_keypair_load_key(X509_KEYPAIR *keypair, const char *file, CRYPTO_PEM_PASSWD_CB *pem_callback, const void *pem_userdata)
{
   return false;
}

void crypto_keypair_free(X509_KEYPAIR *keypair)
{
}

CRYPTO_SESSION *crypto_session_new(crypto_cipher_t cipher, alist *pubkeys)
{
   return NULL;
}

void crypto_session_free(CRYPTO_SESSION *cs)
{
}

bool crypto_session_encode(CRYPTO_SESSION *cs, uint8_t *dest, uint32_t *length)
{
   return false;
}

crypto_error_t crypto_session_decode(const uint8_t *data, uint32_t length, alist *keypairs, CRYPTO_SESSION **session)
{
   return CRYPTO_ERROR_INTERNAL;
}

CIPHER_CONTEXT *crypto_cipher_new(CRYPTO_SESSION *cs, bool encrypt, uint32_t *blocksize)
{
   return NULL;
}

bool crypto_cipher_update(CIPHER_CONTEXT *cipher_ctx, const uint8_t *data, uint32_t length, const uint8_t *dest, uint32_t *written)
{
   return false;
}

bool crypto_cipher_finalize(CIPHER_CONTEXT *cipher_ctx, uint8_t *dest, uint32_t *written)
{
   return false;
}

void crypto_cipher_free(CIPHER_CONTEXT *cipher_ctx)
{
}

const char *crypto_digest_name(DIGEST *digest)
{
   return crypto_digest_name(digest->type);
}
#endif /* HAVE_CRYPTO */

/* Are we initialized? */
static int crypto_initialized = false;

int init_crypto(void)
{
   int status;

   if ((status = gnutls_global_init()) != 0) {
      Jmsg1(NULL, M_ABORT, 0,
            _("Unable to init GNUTLS: ERR=%d\n"), status);
   }

   crypto_initialized = true;

   return status;
}

int cleanup_crypto(void)
{
   /*
    * Ensure that we've actually been initialized; Doing this here decreases the
    * complexity of client's termination/cleanup code.
    */
   if (!crypto_initialized) {
      return 0;
   }

   gnutls_global_deinit();

   crypto_initialized = false;

   return 0;
}
#endif /* HAVE_GNUTLS */
